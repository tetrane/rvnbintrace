#pragma once

#include <string>
#include <fstream>
#include <memory>
#include <iostream>
#include <vector>

#include <rvnmetadata/metadata.h>
#include <rvnbinresource/reader.h>

#include "section_reader.h"
#include "reader_errors.h"
#include "trace_sections.h"

namespace reven {
namespace backend {
namespace plugins {
namespace file {
namespace libbintrace {

/**
 * This is the base class for reading a binary file trace.
 * See readme.md for general information, and trace-format.md for a detailed description of the binary format.
 *
 * This object is meant to read events sequentially. You cannot use this object directly. You must inherit from it and
 * implement do_* callbacks which will be called while reading a new event on file.
 *
 * It cannot easily browse to a random context, but used in conjunction with a CacheReader object, it is possible to
 * @ref seek to known context_id's locations, and start
 * reading events from there.
 *
 * The EventsSectionWriter object is meant to live for most of the trace writing.
 */
class TraceReader
{
public:
	TraceReader(std::unique_ptr<std::istream>&& input_stream);

	//! The current file stream position. Use this information to seek to known locations.
	std::uint64_t stream_pos();

	//! Set the object as if context_id was just read, and file was now at stream_position.
	void seek(std::uint64_t context_id, std::uint64_t stream_position);

	//! Will read the next event in the trace. This will cause the user-defined callbacks to be called. Once the
	//! function returns, the object is ready to read the next event.
	bool read_next_event();

	const Header& header() const { return header_; }
	const MachineDescription& machine() const { return machine_description_; }

	//! The total count of events in the trace.
	std::uint64_t event_count() const { return event_count_; }

	//! Returns the next event to be read, or event being read.
	std::uint64_t next_event_index() const { return current_event_id_; }

	//! Returns the metadata of the resource
	metadata::Metadata metadata() const { return metadata::Metadata::from_raw_metadata(reader_.metadata()); }

	static metadata::Version resource_version();

	static metadata::ResourceType resource_type();

protected:
	//! The location in the stream of the initial memory regions
	const std::vector<std::ios::pos_type>& initial_memory_regions_stream_positions() const { return memory_positions_; }

	//! Return the comprehensive dump of the initial registers values.
	const RegisterContainer& initial_registers() const { return initial_cpu_; }

	//! @name User-defined mandatory callback
	//! @{


	//! @name The first functions to be called when we start reading an event.
	//!
	//! When an event starts (during a call to @read_next_event), one of these method will be called once. Which method is called depends on the event's
	//! type.
	//!
	//! @{

	//! We've started reading an instruction
	virtual void do_event_instruction() = 0;

	//! We've started reading an event of type `other` has started, with `description`
	virtual void do_event_other(const std::string& description) = 0;

	//! @}

	// Must return pointers to two valid buffers (their sizes must match register's size):
	// - first: Valid area, which contains latest known register value. Will be read-only
	// - second: Valid area where we'll write the new register content. Buffer content will be ignored and overwritten.
	// Note those two pointers can be identical.
	virtual std::pair<const std::uint8_t*, std::uint8_t*> do_register_rw_buffers(RegisterId id) = 0;

	//! Must return a buffer to place the new content of physical memory at `address`, of size `size`
	//! The returned buffer cannot be null, and its size must be <= `size` and > 0.
	//! If the returned size is less than `size`, we will write what we can into the passed buffer and then call again
	//! with an updated address, requesting a buffer for what is left.
	virtual std::pair<std::uint8_t*, std::uint64_t> do_memory_before_write(std::uint64_t address,
	                                                                            std::uint64_t size) = 0;

	//! Each call to `do_memory_before_write` will be followed with one call to `do_memory_after_write` once the buffer
	//! is written, so you can decide what to do with the new value. Note that if `do_memory_before_write` is called
	//! multiple times in a row because it returned a buffer smaller than required, this method will be called multiple
	//! times too, interleaved.
	//! @param address The address passed to the corresponding call to `do_memory_before_write`
	//! @param address The writable `buffer` returned by the corresponding call to `do_memory_before_write`
	//! @param address The buffer's `size` returned by the corresponding call to `do_memory_before_write`
	//! @note This callback method is for user's convenience only, and can simply do nothing.
	virtual void do_memory_after_write(std::uint64_t address, const std::uint8_t* buffer, std::uint64_t size) = 0;

	//! @}

	//! The trace's stream.
	binresource::Reader reader_;
private:
	void build_quick_access_vectors(const MachineDescription& machine);
	void process_register_write(SectionReader* reader, RegisterId reg_id);
	void process_memory_write(SectionReader* reader, std::uint64_t address, std::uint64_t size);

	Header header_;
	MachineDescription machine_description_;
	RegisterContainer initial_cpu_;
	std::vector<std::ios::pos_type> memory_positions_;

	//! Quick-access vector for register actions properties stored in machine_description_.
	//! If Value.first is false, the Key is not associated with any action.
	std::vector<std::pair<bool, MachineDescription::RegisterOperation>> register_operations_;

	//! Quick-access vector for register properties stored in machine_description_
	//! If Value.first is false, the Key is not associated with any register.
	std::vector<std::pair<bool, MachineDescription::Register>> registers_;

	std::unique_ptr<SectionReader> events_reader_;
	std::uint64_t current_event_id_;
	std::uint64_t event_count_;
};

}}}}}
