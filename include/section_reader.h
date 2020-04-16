#pragma once

#include <rvnbinresource/reader.h>
#include <cstdint>

#include "trace_section_readers.h"

namespace reven {
namespace backend {
namespace plugins {
namespace file {
namespace libbintrace {

class SectionReader {
public:
	SectionReader(const char* name, binresource::Reader& reader);
	std::uint64_t stream_pos() const;
	std::ios::pos_type section_stream_pos() const { return start_stream_pos_; }
	std::uint64_t bytes_left() const;
	void seek(std::uint64_t position);
	void seek_to_end();

	template <typename T>
	T read()
	{
		T value ;
		read(reinterpret_cast<std::uint8_t*>(&value), sizeof(T));
		return value;
	}

	template <typename T>
	std::string read_string()
	{
		std::string result;
		for (T size = read<T>(); size > 0; --size) {
			result.push_back(read<char>());
		}
		return result;
	}

	template <typename T>
	T read(std::size_t max)
	{
		T value = 0;
		read(reinterpret_cast<std::uint8_t*>(&value), std::min(sizeof(T), max));
		return value;
	}

	void read(std::uint8_t* buffer, std::size_t size);

	const char* name() const;

private:
	void fill_stream_buffer();

	const char* name_;
	binresource::Reader& reader_;
	std::uint64_t bytes_lefts_;
	std::uint64_t declared_size_;
	std::ios::pos_type start_stream_pos_;

	std::vector<std::uint8_t> stream_buffer_;
	std::uint64_t bytes_read_in_buffer_;
	std::uint64_t bytes_left_in_stream_buffer_;
};

}}}}}
