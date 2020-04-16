#pragma once

#include <string>
#include <fstream>
#include <memory>
#include <iostream>

#include <rvnbinresource/writer.h>

#include "writer_errors.h"
#include "cache_sections.h"
#include "cache_section_writers.h"

namespace reven {
namespace backend {
namespace plugins {
namespace file {
namespace libbintrace {

/**
 * This is the base class for writing the cache of a file trace.
 * See readme.md for general information, and cache-format.md for a detailed description of the binary format.
 *
 * The CachePointsSectionWriter object is meant to live for most of the trace writing. When to write cache points is up
 * to the caller, no specific interval is required.
 *
 * Each cache point must contain a full register dump, along with each page that has changed since the previous cache
 * point was written (or the beginning of the trace if not applicable).
 *
 * See also @ref CachePointsSectionWriter documentation
 */
class CacheWriter
{
public:
	CacheWriter(std::unique_ptr<std::ostream>&& output_stream, std::uint32_t page_size,
	            const MachineDescription& machine_description,
	            const char* tool_name, const char* tool_version, const char* tool_info);

	//! You should call this once at the start of your trace.
	//! The returned object is meant to live for most of the trace writing duration.
	CachePointsSectionWriter start_cache_points_section();

	//! You should call this once, when trace writing is done.
	void finish_cache_points_section(CachePointsSectionWriter&& writer);

	const CacheHeader& header() { return header_; }

protected:
	const MachineDescription& machine() { return machine_; }
	binresource::Writer writer_;

private:
	CacheHeader header_;
	MachineDescription machine_;
};

}}}}}
