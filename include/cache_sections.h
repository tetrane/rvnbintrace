#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <map>

namespace reven {
namespace backend {
namespace plugins {
namespace file {
namespace libbintrace {

class CacheHeader
{
public:
	std::uint32_t page_size;
};

class CacheIndex
{
public:
	struct PageCacheOffsets {
		std::uint64_t page_address;
		std::uint64_t cache_stream_offset;
	};

	struct CacheOffsets {
		//! Offset in the event section of the corresponding trace's binary file (excluding section size) where the
		//! event right after this cache point is located.
		std::uint64_t trace_stream_offset;
		//! Offset in the cache file's cache point section where register dump starts
		std::uint64_t cpu_cache_stream_offset;
		//! Offsets in the cache file's cache point section for dumped pages
		std::vector<PageCacheOffsets> page_offsets;
	};

	using CacheOffsetsType = std::map<std::uint64_t, CacheOffsets, std::greater<std::uint64_t>>;
	CacheOffsetsType cache_points;
};

}}}}}
