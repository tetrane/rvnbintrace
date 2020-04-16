#pragma once

#include <rvnbinresource/reader.h>

#include "cache_sections.h"

namespace reven {
namespace backend {
namespace plugins {
namespace file {
namespace libbintrace {

CacheHeader read_cache_header(binresource::Reader& reader);
CacheIndex read_cache_index(binresource::Reader& reader);

}}}}}
