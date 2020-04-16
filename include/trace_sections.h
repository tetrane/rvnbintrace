#pragma once

#include <cstdint>
#include <vector>
#include <map>

namespace reven {
namespace backend {
namespace plugins {
namespace file {
namespace libbintrace {

class Header
{
public:
	std::uint8_t compression;
};

using RegisterId = std::uint16_t;

class MachineDescription
{
public:
	enum class Archi {
		x64_1
	};

	enum class RegisterOperator {
		Set = 0,
		Add = 1,
		And = 2,
		Or  = 3,

		RegisterOperatorMax
	};

	struct MemoryRegion {
		std::uint64_t start;
		std::uint64_t size;
	};

	struct Register {
		std::uint16_t size;
		std::string name;
	};

	//! A register operation will be applied as following:
	//! register = register "operation" value
	//! for instance : eax = eax + value
	struct RegisterOperation {
		RegisterId register_id;
		RegisterOperator operation;
		std::vector<std::uint8_t> value;
	};

	Archi architecture;

	//! Physical addresses will be stored using that many bytes. This will impact trace's size. Must be > 0 and <= 8.
	std::uint8_t physical_address_size;

	//! What memory regions the trace will keep up-to-date.
	//! Order is important: The initial memory region content will be written and read in the same order as this vector.
	std::vector<MemoryRegion> memory_regions;

	//! Most recurrent RegisterId should be less than 0xff, because they will be refered to using 1 byte only, reducing
	//! file size.
	std::map<RegisterId, Register> registers;

	//! Register operations are not mandatory at all, and can apply to any register defined earlier.
	//! Their id space is shared with that or RegisterId, so no overlapping between `registers` and `register_operations`
	//! is allowed.
	//! The reason they are on 8-bit only is to reduce file size.
	std::map<std::uint8_t, RegisterOperation> register_operations;

	//! Static registers are values that describe the architecture and do not change during the trace.
	//! On X86, those are CPUID values for instance.
	std::map<std::string, std::vector<std::uint8_t>> static_registers;

	std::uint64_t total_physical_size() const {
		std::uint64_t result = 0;
		for (const auto& region : memory_regions)
			result += region.size;
		return result;
	}
};

using RegisterContainer = std::vector<std::pair<RegisterId, std::vector<std::uint8_t>>>;

}}}}}
