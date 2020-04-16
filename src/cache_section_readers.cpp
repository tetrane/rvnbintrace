#include <cache_section_readers.h>

#include <reader_errors.h>
#include <section_reader.h>

namespace reven {
namespace backend {
namespace plugins {
namespace file {
namespace libbintrace {

CacheHeader read_cache_header(binresource::Reader& reader)
{
	SectionReader section_reader("cache header", reader);
	CacheHeader data;

	data.page_size = section_reader.read<std::uint32_t>();

	section_reader.seek_to_end();
	return data;
}

CacheIndex read_cache_index(binresource::Reader& reader)
{
	SectionReader section_reader("cache index", reader);
	CacheIndex data;

	for (std::size_t count = section_reader.read<std::uint64_t>(); count > 0; --count) {
		CacheIndex::CacheOffsets offsets;
		auto context_id = section_reader.read<std::uint64_t>();
		offsets.trace_stream_offset = section_reader.read<std::uint64_t>();
		offsets.cpu_cache_stream_offset = section_reader.read<std::uint64_t>();

		for (std::size_t count = section_reader.read<std::uint32_t>(); count > 0; --count) {
			CacheIndex::PageCacheOffsets page;
			page.page_address = section_reader.read<std::uint64_t>();
			page.cache_stream_offset = section_reader.read<std::uint64_t>();
			offsets.page_offsets.push_back(page);
		}

		data.cache_points.insert(std::make_pair(context_id, offsets));
	}

	section_reader.seek_to_end();
	return data;
}

}}}}}
