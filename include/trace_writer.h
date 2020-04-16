#pragma once

#include <string>
#include <fstream>
#include <memory>
#include <iostream>

#include <rvnbinresource/writer.h>

#include "writer_errors.h"
#include "trace_sections.h"
#include "trace_section_writers.h"

namespace reven {
namespace backend {
namespace plugins {
namespace file {
namespace libbintrace {

/**
 * This is the base class for writing a binary file trace.
 * See readme.md for general information, and trace-format.md for a detailed description of the binary format.
 *
 * The sections must be written in order, which is enforced by the *SectionWriter object passing.
 *
 * The EventsSectionWriter object is meant to live for most of the trace writing.
 */
class TraceWriter
{
public:
	TraceWriter(std::unique_ptr<std::ostream>&& output_stream, const MachineDescription& machine_description,
	            const char* tool_name, const char* tool_version, const char* tool_info);

	//! First section to be written: initial memory section.
	//! Using the created object, you must write the memory content of the regions declared in machine_description.
	//! @warning The amount of data written for each region must match the region's size declared in machine description.
	//! @warning The order in which you write regions must be the same as the one declared in machine_description.
	InitialMemorySectionWriter start_initial_memory_section();

	//! Second section: initial registers.
	//! Using the created object, you should write each register declared in machine_description once and once only.
	InitialRegistersSectionWriter start_initial_registers_section(InitialMemorySectionWriter&& writer);

	//! Third section: events. You will create this object once, and use it to write all events occuring in the trace.
	EventsSectionWriter start_events_section(InitialRegistersSectionWriter&& writer);

	//! This will finish the section, but not close the stream. You should probably not do anything with the TraceWriter
	//! but destroy so the stream closes.
	void finish_events_section(EventsSectionWriter&& writer);

	const Header& header() { return header_; }
	const MachineDescription& machine() { return machine_; }

protected:
	binresource::Writer writer_;

private:
	Header header_;
	MachineDescription machine_;

	bool initial_section_written_;
};

}}}}}
