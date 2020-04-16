#pragma once

#include <rvnbinresource/writer.h>

#include "trace_sections.h"
#include "section_writer.h"

namespace reven {
namespace backend {
namespace plugins {
namespace file {
namespace libbintrace {

void write_trace_header(binresource::Writer&, const Header& data);
void write_trace_machine_description(binresource::Writer&, const MachineDescription& data);

class TraceWriter;

class InitialMemorySectionWriter : protected ExternalSectionTraceWriter
{
public:
	//! Writes a chunk of initial memory.
	//! Be sure to write memory region's content in the same order they were declared in machine description.
	void write(const std::uint8_t* buffer, std::uint64_t size);

private:
	friend TraceWriter;
	InitialMemorySectionWriter(binresource::Writer&& writer, const MachineDescription& machine);

	void do_finalize() override;
};

class InitialRegistersSectionWriter : public ExternalSectionTraceWriter
{
public:
	void write(RegisterId reg_id, const std::uint8_t* buffer, std::uint64_t size);

private:
	friend TraceWriter;
	InitialRegistersSectionWriter(binresource::Writer&& writer, const MachineDescription& machine);

	std::ostream::pos_type initial_context_count_stream_pos_;
	std::uint32_t initial_context_count_;
	void do_finalize() override;
};

class EventsSectionWriter : public ExternalSectionTraceWriter
{
public:
	//! @name To declare a new event, call these methods in the order described below.
	//!
	//! Once a declaration is started, you must finish it with `finish_event` before you can start another one.
	//!
	//! @{
	//!
	//! @name 1/ Call one of these functions to start declaring a new event, depending on its type.
	//! @{
	//! Start an instruction
	void start_event_instruction();

	//! Start an unspecified event, and provide a description. The `description` cannot be larger than 255 characters.
	void start_event_other(const std::string& description);
	//! @}

	//! 2/ Call this as many times as necessary to declare all memory writes occuring in the trace.
	void write_memory(std::uint64_t address, const std::uint8_t* buffer, std::uint64_t size);

	//! @name 3/ Call these as many times as necessary to declare register writes.
	//! @note You do not need to write all registers, only those which changed. You can even call declare the same
	//! register twice.
	//! @note You can interleave these methods with one another.
	//! @{
	//! Declare a register write and pass its new value in buffer.
	//! @warning `size` must match the register's size defined in machine description.
	void write_register(RegisterId reg_id, const std::uint8_t* buffer, std::uint64_t size);

	//! Declare a register write by creating a register operation instead of passing the full new value.
	//! @warning @reg_id must have been defined as a register operation in machine description
	void write_register_action(RegisterId reg_id);
	//! @}

	//! 4/ Close the opened event.
	void finish_event();

	//! @}

	//! Indicates if an event is in the process of being defined. Attempting to define another cache point while this
	//! method returns `true` is undefined behavior.
	bool is_event_started();

	std::uint64_t event_count() const { return event_count_; }

private:
	friend TraceWriter;
	EventsSectionWriter(binresource::Writer&& writer, const MachineDescription& machine);

	void write_event_diff_size();
	void continue_on_next_event();

	std::uint64_t event_count_;
	//! Stream position of the total count of events in this trace, to be updated on finish_event()
	std::ostream::pos_type event_count_stream_pos_;
	//! Stream position of the current diff size, if any.
	std::ostream::pos_type diff_element_count_stream_pos_;
	//! Count of memory changes in the current diff.
	std::uint8_t current_diff_mem_count_;
	//! Count of register changes in the current diff.
	std::uint8_t current_diff_reg_count_;

	void do_finalize() override;
};

}}}}}
