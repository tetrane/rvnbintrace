#include <section_reader.h>

#include <cstring>

#include <reader_errors.h>

namespace reven {
namespace backend {
namespace plugins {
namespace file {
namespace libbintrace {

SectionReader::SectionReader(const char* name, binresource::Reader& reader)
  : name_(name), reader_(reader)
{
	stream_buffer_.resize(16*1024); // test shows that 16KiB seems like a sweet spot.
	bytes_read_in_buffer_ = 0;
	bytes_left_in_stream_buffer_ = 0;
	bytes_lefts_ = 8;
	bytes_lefts_ = read<std::uint64_t>();
	declared_size_ = bytes_lefts_;
	start_stream_pos_ = reader_.stream().tellg();
}

std::uint64_t SectionReader::stream_pos() const
{
	return declared_size_ - bytes_lefts_;
}

std::uint64_t SectionReader::bytes_left() const
{
	return bytes_lefts_;
}

void SectionReader::seek(std::uint64_t position)
{
	if (position > declared_size_)
		throw std::logic_error("Trying to seek outside section");
	reader_.stream().seekg(start_stream_pos_ + static_cast<std::ios::pos_type>(position));
	reader_.stream().clear();
	bytes_read_in_buffer_ = 0;
	bytes_left_in_stream_buffer_ = 0;
	bytes_lefts_ = declared_size_ - position;

}

void SectionReader::seek_to_end()
{
	if (bytes_left_in_stream_buffer_ > bytes_lefts_)
		throw std::logic_error("Too much data read in section buffer");
	reader_.stream().seekg(start_stream_pos_ + static_cast<std::ios::pos_type>(declared_size_));
	reader_.stream().clear();
	bytes_lefts_ = 0;
}

const char* SectionReader::name() const
{
	return name_;
}

void SectionReader::read(std::uint8_t* buffer, std::size_t size)
{
	if (size > bytes_lefts_) {
		throw UnexpectedEndOfSection(name());
	}

	while(size > 0) {
		auto pass_size = std::min(size, bytes_left_in_stream_buffer_);
		std::memcpy(buffer, stream_buffer_.data() + bytes_read_in_buffer_, pass_size);
		buffer += pass_size;
		size -= pass_size;
		bytes_read_in_buffer_ += pass_size;
		bytes_left_in_stream_buffer_ -= pass_size;
		bytes_lefts_ -= pass_size;

		if (size != 0) {
			fill_stream_buffer();
			if (not bytes_left_in_stream_buffer_)
				throw UnexpectedEndOfStream(name());
		}
	}
}

void SectionReader::fill_stream_buffer()
{
	auto size = std::min<std::size_t>(bytes_lefts_, stream_buffer_.size());
	reader_.stream().read(reinterpret_cast<char*>(stream_buffer_.data()), size);
	if (static_cast<std::size_t>(reader_.stream().gcount()) < size){
		reader_.stream().clear();
		size = static_cast<std::size_t>(reader_.stream().gcount());
	}
	bytes_read_in_buffer_ = 0;
	bytes_left_in_stream_buffer_ = size;
}

}}}}}
