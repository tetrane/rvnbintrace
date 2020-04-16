#pragma once

#include <rvnbinresource/reader.h>

#include "trace_sections.h"

namespace reven {
namespace backend {
namespace plugins {
namespace file {
namespace libbintrace {

Header read_trace_header(binresource::Reader& reader);
MachineDescription read_trace_machine_description(binresource::Reader& reader);
RegisterContainer read_initial_cpu_context(binresource::Reader& reader, const MachineDescription& machine);

}}}}}
