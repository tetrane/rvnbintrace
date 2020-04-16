#pragma once

#include <rvnbinresource/writer.h>

#include "cache_sections.h"
#include "section_writer.h"

namespace reven {
namespace backend {
namespace plugins {
namespace file {
namespace libbintrace {

class CacheWriter;

void write_cache_header(binresource::Writer&, const CacheHeader& data);
void write_cache_index(binresource::Writer&, const CacheIndex& data);

class CachePointsSectionWriter : public ExternalSectionTraceWriter
{
public:
	//! @name To declare a new cache point, call these methods in the order described below.
	//!
	//! Once a declaration is started, you must finish it with `finish_cache_point` before you can start another one.
	//!
	//! @{
	//!
	//! 1/ Call this first when declaring a new cache point.
	//! @param context_id the context id of this cache point
	//! @param trace_stream_pos the offset of the trace file the trace reader will have to place itself to read further
	//! events (starting at context_id + 1). It is up to the caller to ensure this offset makes sense, otherwise this
	//! cache file will be useless.
	void start_cache_point(std::uint64_t context_id, std::uint64_t trace_stream_pos);

	//! 2/ Call this as many times as necessary to write all registers.
	//! @warning Missing a register will render the cache file invalid.
	void write_register(RegisterId reg_id, const std::uint8_t* buffer, std::uint64_t size);

	//! 3/ Once all registers have been written, call this as many times as necessary to write all modified pages.
	//! @note buffer size is implicitly the page size that has been declared while instanciating @ref CacheWriter.
	void write_memory_page(std::uint64_t address, const std::uint8_t* buffer);

	//! 4/ Call this to finish the declaration of the cache point.
	void finish_cache_point();

	//! @}

	//! Indicates if a cache point is in the process of being defined. Attempting to define another cache point while
	//! this method returns `true` is undefined behavior.
	bool is_cache_point_started();

private:
	friend CacheWriter;
	CachePointsSectionWriter(binresource::Writer&& writer, std::uint32_t page_size,
	                  const MachineDescription& machine);

	//! It is the responsibility of CacheWriter to write the index to stream.
	const CacheIndex& index() const { return index_; }

	CacheIndex index_;

	std::uint64_t current_reg_count_pos_;
	std::uint32_t current_reg_count_;
	CacheIndex::CacheOffsetsType::iterator current_cache_offsets_;
	std::uint32_t page_size_;
	bool cache_point_started_;

	void do_finalize() override;
};

}}}}}
