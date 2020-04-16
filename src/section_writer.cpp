#include <section_writer.h>

#include <cstring>

#include <reader_errors.h>

namespace reven {
namespace backend {
namespace plugins {
namespace file {
namespace libbintrace {

SectionWriter::SectionWriter(const char* name, binresource::Writer* writer)
  : name_(name), writer_(writer), bytes_written_(0), total_bytes_flushed_(0), bytes_not_flushed_(0)
{
	writer_->stream().write(reinterpret_cast<const char*>(&bytes_written_), sizeof(bytes_written_));
	stream_buffer_.resize(1024*1024*4);
	ensure_stream_status();
}

void SectionWriter::finalize()
{
	flush_stream_buffer();
	writer_->stream().seekp(0 - bytes_written_ - sizeof(std::uint64_t), std::ios::cur);
	writer_->stream().write(reinterpret_cast<const char*>(&bytes_written_), sizeof(bytes_written_));
	writer_->stream().seekp(bytes_written_, std::ios::cur);
	ensure_stream_status();
}

const char* SectionWriter::name() const
{
	return name_;
}

void SectionWriter::write_buffer(const std::uint8_t* buffer, std::size_t size)
{
	if (size > left_in_stream_buffer())
		flush_stream_buffer();

	if (size > left_in_stream_buffer()){
		writer_->stream().write(reinterpret_cast<const char*>(buffer), size);
		total_bytes_flushed_ += size;
	} else {
		std::memcpy(stream_buffer_.data() + bytes_not_flushed_, buffer, size);
		bytes_not_flushed_ += size;
	}
	bytes_written_ += size;
	ensure_stream_status();
}

void SectionWriter::write_buffer_back_at(std::uint64_t pos, const std::uint8_t* buffer, std::size_t size)
{
	if (pos < total_bytes_flushed_) {
		flush_stream_buffer();
		writer_->stream().seekp(0 - (bytes_written_ - pos), std::ios::cur);
		writer_->stream().write(reinterpret_cast<const char*>(buffer), size);
		writer_->stream().seekp(bytes_written_ - pos - size, std::ios::cur);
		ensure_stream_status();
	} else {
		std::memcpy(stream_buffer_.data() + (pos - total_bytes_flushed_), buffer, size);
	}
}

void SectionWriter::ensure_stream_status()
{
	if (not writer_->stream())
		throw UnexpectedEndOfStream(name());
}

void SectionWriter::flush_stream_buffer()
{
	writer_->stream().write(reinterpret_cast<const char*>(stream_buffer_.data()), bytes_not_flushed_);
	total_bytes_flushed_ += bytes_not_flushed_;
	bytes_not_flushed_ = 0;
}

std::uint64_t SectionWriter::left_in_stream_buffer()
{
	return stream_buffer_.size() - bytes_not_flushed_;
}

ExternalSectionTraceWriter::ExternalSectionTraceWriter(binresource::Writer&& writer, const MachineDescription& machine,
                             const char* writer_name)
  : writer_(std::move(writer))
  , machine_(machine)
  , section_writer_(writer_name, &writer_)
{
	if (not writer_.stream())
		throw std::logic_error("Creating ExternalSectionTraceWriter with bad writer");
}

ExternalSectionTraceWriter::ExternalSectionTraceWriter(ExternalSectionTraceWriter&& rhs)
  : writer_(std::move(rhs.writer_))
  , machine_(std::move(rhs.machine_))
  , section_writer_(std::move(rhs.section_writer_))
{
	section_writer_.writer_ = &writer_;
}

const MachineDescription::Register& ExternalSectionTraceWriter::find_register(RegisterId reg_id)
{
	auto reg = machine_.registers.find(reg_id);
	if (reg == machine_.registers.end())
		throw NonsenseValue(section_writer_.name(), std::to_string(reg_id) + "in an invalid register id");
	return reg->second;
}

binresource::Writer&&  ExternalSectionTraceWriter::finalize()
{
	do_finalize();

	section_writer_.finalize();
	return std::move(writer_);
}

}}}}}
