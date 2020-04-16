#pragma once

#include <vector>
#include <cstdint>
#include <memory>

#include <rvnbinresource/writer.h>

#include "writer_errors.h"
#include "trace_sections.h"

namespace reven {
namespace backend {
namespace plugins {
namespace file {
namespace libbintrace {

class SectionWriter {
public:
	SectionWriter(const char* name, binresource::Writer* writer);

	std::uint64_t bytes_written() { return bytes_written_; }
	void finalize();

	template <typename WriteType, typename InputType>
	void write(const InputType& value)
	{
		WriteType converted = static_cast<WriteType>(value);
		if (converted != value)
			throw ValueTooBig(name());
		write_buffer(reinterpret_cast<const std::uint8_t*>(&converted), sizeof(WriteType));
	}

	template <typename InputType>
	void write(const InputType& value, std::size_t max)
	{
		if (max > sizeof(InputType))
			throw ValueTooBig(name());

		InputType mask = 0;
		mask = (~mask);
		mask >>= (sizeof(InputType) - max)*8;
		InputType converted = value & mask;

		if (converted != value)
			throw ValueTooBig(name());

		write_buffer(reinterpret_cast<const std::uint8_t*>(&converted), max);
	}

	template <typename T>
	void write_string(const std::string& str)
	{
		write_sized_buffer<T>(reinterpret_cast<const std::uint8_t*>(str.data()), str.size());
	}

	template <typename T>
	void write_sized_buffer(const std::uint8_t* buffer, std::size_t size)
	{
		if (size != static_cast<T>(size))
			throw ValueTooBig(name());

		write<T>(size);
		write_buffer(buffer, size);
	}

	void write_buffer(const std::uint8_t* buffer, std::size_t size);
	void write_buffer_back_at(std::uint64_t pos, const std::uint8_t* buffer, std::size_t size);

	const char* name() const;

private:
	void ensure_stream_status();
	void flush_stream_buffer();
	std::uint64_t left_in_stream_buffer();

	const char* name_;
	binresource::Writer* writer_;
	std::uint64_t bytes_written_;

	std::vector<std::uint8_t> stream_buffer_;
	std::uint64_t total_bytes_flushed_;
	std::uint64_t bytes_not_flushed_;

	friend class ExternalSectionTraceWriter;
};

class ExternalSectionTraceWriter
{
public:
	ExternalSectionTraceWriter(binresource::Writer&& writer, const MachineDescription& machine, const char* writer_name);
	binresource::Writer&& finalize();
	std::uint64_t stream_pos() { return section_writer_.bytes_written(); }

	ExternalSectionTraceWriter(const ExternalSectionTraceWriter&) = delete;
	ExternalSectionTraceWriter(ExternalSectionTraceWriter&& rhs);

private:
	binresource::Writer writer_;

protected:
	const MachineDescription::Register& find_register(RegisterId reg_id);
	template <typename T>
	void write_at_position(const T& value, std::uint64_t pos);
	virtual void do_finalize() {}

	const MachineDescription& machine_;
	SectionWriter section_writer_;
};

template <typename T>
void ExternalSectionTraceWriter::write_at_position(const T& value, std::uint64_t pos)
{
	section_writer_.write_buffer_back_at(pos, reinterpret_cast<const uint8_t*>(&value), sizeof(value));
}

}}}}}
