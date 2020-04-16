#include <cache_section_writers.h>

#include <writer_errors.h>

namespace reven {
namespace backend {
namespace plugins {
namespace file {
namespace libbintrace {

void write_cache_header(binresource::Writer& writer, const CacheHeader& data)
{
	SectionWriter section_writer("cache header", &writer);

	section_writer.write<std::uint32_t>(data.page_size);

	section_writer.finalize();
}

void write_cache_index(binresource::Writer& writer, const CacheIndex& data)
{
	SectionWriter section_writer("cache index", &writer);

	section_writer.write<std::uint64_t>(data.cache_points.size());
	for (const auto& cache_point : data.cache_points) {
		section_writer.write<std::uint64_t>(cache_point.first);
		section_writer.write<std::uint64_t>(cache_point.second.trace_stream_offset);
		section_writer.write<std::uint64_t>(cache_point.second.cpu_cache_stream_offset);
		section_writer.write<std::uint32_t>(cache_point.second.page_offsets.size());
		for (const auto& page : cache_point.second.page_offsets) {
			section_writer.write<std::uint64_t>(page.page_address);
			section_writer.write<std::uint64_t>(page.cache_stream_offset);
		}
	}

	section_writer.finalize();
}

CachePointsSectionWriter::CachePointsSectionWriter(binresource::Writer&& writer, std::uint32_t page_size,
                                     const MachineDescription& machine)
  : ExternalSectionTraceWriter(std::move(writer), machine, "cache points")
  , page_size_(page_size)
  , cache_point_started_(false)
{
}

void CachePointsSectionWriter::start_cache_point(std::uint64_t context_id, std::uint64_t trace_stream_pos)
{
	if (cache_point_started_)
		throw std::logic_error("Called start_cache_point twice with no finish_cache_point in between");
	if (index_.cache_points.find(context_id) != index_.cache_points.end())
		throw NonsenseValue(section_writer_.name(), std::to_string(context_id) + " has a cache point already");

	current_cache_offsets_ =
	  index_.cache_points
	    .emplace(std::make_pair(context_id, CacheIndex::CacheOffsets{ trace_stream_pos, stream_pos(), {} }))
	    .first;

	current_reg_count_pos_ = stream_pos();
	section_writer_.write<std::uint16_t>(0u);
	current_reg_count_ = 0;
	cache_point_started_= true;
}

void CachePointsSectionWriter::write_register(RegisterId reg_id, const std::uint8_t* buffer, std::uint64_t size)
{
	if (not cache_point_started_)
		throw std::logic_error("Called write_register before start_cache_point.");
	if (current_cache_offsets_->second.page_offsets.size() != 0)
		throw std::logic_error("Called write_register after write_memory.");
	auto reg = machine_.registers.find(reg_id);
	if (reg == machine_.registers.end())
		throw NonsenseValue(section_writer_.name(), std::to_string(reg_id) + " is an unknown register id");
	if (reg->second.size != size)
		throw NonsenseValue(section_writer_.name(), std::to_string(reg_id) + " 's size is " +
		                                              std::to_string(reg->second.size) + ", not " +
		                                              std::to_string(size));

	section_writer_.write<std::uint16_t>(reg_id);
	section_writer_.write_sized_buffer<std::uint16_t>(buffer, size);
	++current_reg_count_;
}

void CachePointsSectionWriter::write_memory_page(std::uint64_t address, const std::uint8_t* buffer)
{
	if (not cache_point_started_)
		throw std::logic_error("Called write_memory before start_cache_point.");
	if (address % page_size_ != 0)
		throw NonsenseValue(section_writer_.name(), std::to_string(address) + " is not aligned on page size");

	bool found = false;
	for (const auto& region : machine_.memory_regions) {
		if (address >= region.start and address + page_size_ <= region.start + region.size) {
			found = true;
			break;
		}
	}

	if (not found)
		throw NonsenseValue(section_writer_.name(), std::to_string(address) + " is outside of known memory regions");

	current_cache_offsets_->second.page_offsets.push_back({ address, stream_pos() });
	section_writer_.write_buffer(buffer, page_size_);
}

void CachePointsSectionWriter::finish_cache_point()
{
	if (not cache_point_started_)
		throw std::logic_error("Called finish_cache_point before start_cache_point.");
	write_at_position<std::uint16_t>(current_reg_count_, current_reg_count_pos_);
	cache_point_started_ = false;
	current_cache_offsets_ = index_.cache_points.end();
}

bool CachePointsSectionWriter::is_cache_point_started()
{
	return cache_point_started_;
}

void CachePointsSectionWriter::do_finalize()
{
	if (is_cache_point_started())
		finish_cache_point();
}

}}}}}
