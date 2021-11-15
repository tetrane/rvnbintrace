#include <trace_writer.h>

#include <cstdint>
#include <utility>
#include <set>

#include <rvnmetadata/metadata-common.h>
#include <rvnmetadata/metadata-bin.h>

#include <common.h>
#include <trace_section_writers.h>
#include <section_writer.h>
#include <writer_errors.h>

namespace reven {
namespace backend {
namespace plugins {
namespace file {
namespace libbintrace {

using Meta = ::reven::metadata::Metadata;
using MetaType = ::reven::metadata::ResourceType;
using MetaVersion = ::reven::metadata::Version;

TraceWriter::TraceWriter(std::unique_ptr<std::ostream>&& output_stream, const MachineDescription& machine_description,
                         const char* tool_name, const char* tool_version, const char* tool_info)
  : writer_([&output_stream, tool_name, tool_version, tool_info]() {
    	const auto md = Meta(
    		MetaType::TraceBin,
    		MetaVersion::from_string(format_version),
    		tool_name,
    		MetaVersion::from_string(tool_version),
    		tool_info + std::string(" - using rvnbintrace ") + writer_version
    	);

    	return binresource::Writer::create(std::move(output_stream), metadata::to_bin_raw_metadata(md));
    }())
  , machine_(machine_description)
  , initial_section_written_(false)
{
	header_.compression = 0u; // no compression

	write_trace_header(writer_, header_);
	write_trace_machine_description(writer_, machine_);
}

InitialMemorySectionWriter TraceWriter::start_initial_memory_section()
{
	if (initial_section_written_)
		throw std::logic_error("You cannot create an initial memory section twice");

	initial_section_written_ = true;
	return InitialMemorySectionWriter(std::move(writer_), machine());
}

InitialRegistersSectionWriter TraceWriter::start_initial_registers_section(InitialMemorySectionWriter&& writer)
{
	return InitialRegistersSectionWriter(std::move(writer.finalize()), machine());
}

EventsSectionWriter TraceWriter::start_events_section(InitialRegistersSectionWriter&& writer)
{
	return EventsSectionWriter(std::move(writer.finalize()), machine());
}

void TraceWriter::finish_events_section(EventsSectionWriter&& writer)
{
	writer_ = std::move(writer.finalize());
}

}}}}}
