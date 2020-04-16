#include <cache_writer.h>

#include <cstdint>
#include <utility>
#include <set>
#include <cassert>

#include <rvnmetadata/metadata.h>

#include <common.h>
#include <cache_section_writers.h>

namespace reven {
namespace backend {
namespace plugins {
namespace file {
namespace libbintrace {

using Meta = ::reven::metadata::Metadata;
using MetaType = ::reven::metadata::ResourceType;
using MetaVersion = ::reven::metadata::Version;

CacheWriter::CacheWriter(std::unique_ptr<std::ostream>&& output_stream, std::uint32_t page_size,
                         const MachineDescription& machine_description,
                         const char* tool_name, const char* tool_version, const char* tool_info)
  : writer_([&output_stream, tool_name, tool_version, tool_info]() {
    	const auto md = Meta(
    		MetaType::TraceCache,
    		MetaVersion::from_string(format_version),
    		tool_name,
    		MetaVersion::from_string(tool_version),
    		tool_info + std::string(" - using rvnbintrace ") + writer_version
    	);

    	return binresource::Writer::create(std::move(output_stream), md.to_bin_raw_metadata());
    }())
  , machine_(machine_description)
{
	header_.page_size = page_size;

	write_cache_header(writer_, header_);
}

CachePointsSectionWriter CacheWriter::start_cache_points_section()
{
	return CachePointsSectionWriter(std::move(writer_), header().page_size, machine_);
}

void CacheWriter::finish_cache_points_section(CachePointsSectionWriter&& writer)
{
	const auto& index = writer.index();
	writer_ = std::move(writer.finalize());
	write_cache_index(writer_, index);
}

}}}}}
