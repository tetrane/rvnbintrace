#pragma once

#include <string>
#include <istream>

#include <rvnmetadata/metadata-common.h>
#include <rvnmetadata/metadata-bin.h>
#include <rvnbinresource/reader.h>

#include "section_reader.h"
#include "cache_sections.h"
#include "reader_errors.h"

namespace reven {
namespace backend {
namespace plugins {
namespace file {
namespace libbintrace {

/**
 * This is the base class for reading the cache of a file trace.
 * See readme.md for general information, and cache-format.md for a detailed description of the binary format.
 *
 * This object can help you find the closest cache point to a context_id and give you the corresponding register dump.
 *
 * It cannot help you directly with memory dumps though, because usage is different: instead, it gives you access to its
 * underlying index via @ref index(), from which you can build your own index to quickly retrieve the latest known page
 * version from a context id.
 */
class CacheReader
{
public:
	using ConstIterator = CacheIndex::CacheOffsetsType::const_iterator;

	CacheReader(std::unique_ptr<std::istream>&& input_stream, const MachineDescription& machine);

	//! find the closest cache point that is strictly before context_id. If none can be found, resulting iterator will
	//! be equal to @ref none()
	ConstIterator find_closest(std::uint64_t context_id) const;

	//! Deferencing none() is undefined behavior.
	ConstIterator none() const { return index_.cache_points.end(); }

	//! Will return a comprehensive register dump at specified cache point. See @ref find_closest
	RegisterContainer read_cache_point(ConstIterator it);

	//! This cache stream's header, which notably contains the page size.
	const CacheHeader& header() const { return header_; }

	//! The cache's index, which you will probably need to get to build your own index to manage memory.
	//! This index is comprehensive and immutable as soon as the CacheReader object is created.
	//! Also see @ref cache_points_section_start_pos
	const CacheIndex& index() const { return index_; }

	//! Returns the metadata of the resource
	metadata::Metadata metadata() const { return metadata::from_raw_metadata(reader_.metadata()); }

	static metadata::Version resource_version();

	static metadata::ResourceType resource_type();

	//! The starting position of cache points section.
	//! Useful for converting relative cache_stream_offset of @ref index() into absolute stream offset
	std::ios::pos_type cache_points_section_start_pos() const { return cache_points_reader_->section_stream_pos(); }

private:
	binresource::Reader reader_;
	std::unique_ptr<SectionReader> cache_points_reader_;
	CacheHeader header_;
	CacheIndex index_;
	MachineDescription machine_;
};

}}}}}
