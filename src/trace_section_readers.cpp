#include <trace_section_readers.h>

#include <algorithm>

#include <reader_errors.h>
#include <section_reader.h>

namespace reven {
namespace backend {
namespace plugins {
namespace file {
namespace libbintrace {

Header read_trace_header(binresource::Reader& reader)
{
	SectionReader section_reader("trace header", reader);

	Header result;

	result.compression = section_reader.read<std::uint8_t>();

	if (result.compression)
		throw UnsupportedFeature("compression");

	section_reader.seek_to_end();
	return result;
}

MachineDescription read_trace_machine_description(binresource::Reader& reader)
{
	SectionReader section_reader("trace header", reader);

	MachineDescription result;

	auto magic_value = section_reader.read<std::uint32_t>();
	switch(magic_value) {
		case 0x31343678:
			result.architecture = MachineDescription::Archi::x64_1;
			break;
		default:
			std::string magic_string(reinterpret_cast<char*>(&magic_value), 4);
			throw UnsupportedFeature(std::string("architecture magic ") + magic_string);
	}

	result.physical_address_size = section_reader.read<std::uint8_t>();
	if (result.physical_address_size < 1 or result.physical_address_size > 8) {
		throw MalformedSection(section_reader.name(), "physical size must be between 1 and 8");
	}

	for (std::size_t count = section_reader.read<std::uint32_t>(); count > 0; --count) {
		MachineDescription::MemoryRegion region;
		region.start = section_reader.read<std::uint64_t>(result.physical_address_size);
		region.size = section_reader.read<std::uint64_t>(result.physical_address_size);
		result.memory_regions.push_back(region);
	}

	for (std::size_t count = section_reader.read<std::uint32_t>(); count > 0; --count) {
		auto id = section_reader.read<RegisterId>();
		if (result.registers.find(id) != result.registers.end())
			throw MalformedSection(section_reader.name(), std::string("register id ") + std::to_string(id) + " is defined twice");

		MachineDescription::Register reg;
		reg.size = section_reader.read<std::uint16_t>();
		reg.name = section_reader.read_string<std::uint8_t>();

		for (const auto& other : result.registers) {
			if (other.second.name == reg.name)
				throw MalformedSection(section_reader.name(), std::string("register ") + reg.name + " is defined twice");
		}

		result.registers.emplace(id, reg);
	}

	for (std::size_t count = section_reader.read<std::uint32_t>(); count > 0; --count) {
		auto id = section_reader.read<std::uint8_t>();
		if (result.register_operations.find(id) != result.register_operations.end())
			throw MalformedSection(section_reader.name(), std::string("register operation ") + std::to_string(id) + " is defined twice");
		if (result.registers.find(id) != result.registers.end())
			throw MalformedSection(section_reader.name(),
			                       std::string("register operation ") + std::to_string(id) + " overlaps with register id");
		if (id == 0xff)
			throw MalformedSection(section_reader.name(), "register operation id cannot be 0xff");

		MachineDescription::RegisterOperation reg_action;
		reg_action.register_id = section_reader.read<RegisterId>();
		auto reg = result.registers.find(reg_action.register_id);
		if (reg == result.registers.end())
			throw MalformedSection(section_reader.name(),
			                       std::string("register operation refered to unknown register id ") +
			                         std::to_string(reg_action.register_id));

		auto operation = section_reader.read<std::uint8_t>();
		if (operation >= static_cast<std::uint8_t>(MachineDescription::RegisterOperator::RegisterOperatorMax))
			throw MalformedSection(section_reader.name(),
			                       std::string("register operation operation unknown ") + std::to_string(operation));
		reg_action.operation = static_cast<MachineDescription::RegisterOperator>(operation);

		for (std::size_t i = 0; i < reg->second.size; ++i)
			reg_action.value.push_back(section_reader.read<std::uint8_t>());

		result.register_operations.emplace(id, reg_action);
	}

	for (std::size_t count = section_reader.read<std::uint32_t>(); count > 0; --count) {
		auto name = section_reader.read_string<std::uint8_t>();

		if (result.static_registers.find(name) != result.static_registers.end())
			throw MalformedSection(section_reader.name(), std::string("static register ") + name + " is defined twice");

		std::vector<std::uint8_t> value;
		for (auto size = section_reader.read<std::uint8_t>(); size > 0; --size) {
			value.push_back(section_reader.read<std::uint8_t>());
		}
		result.static_registers.emplace(name, value);
	}

	section_reader.seek_to_end();
	return result;
}

RegisterContainer read_initial_cpu_context(binresource::Reader& reader, const MachineDescription& machine)
{
	SectionReader section_reader("trace initial context", reader);

	RegisterContainer result;

	for (std::size_t count = section_reader.read<std::uint32_t>(); count > 0; --count) {
		auto reg_id = section_reader.read<RegisterId>();
		auto found = std::find_if(result.begin(), result.end(),
		                          [reg_id](const RegisterContainer::value_type& val) { return val.first == reg_id; });
		if (found != result.end())
			throw MalformedSection(section_reader.name(), std::string("Register ") + std::to_string(reg_id) +
			                                                    " contained twice in initial context");
		auto reg = machine.registers.find(reg_id);
		if (reg == machine.registers.end())
			throw MalformedSection(section_reader.name(), std::string("Register ") + std::to_string(reg_id) +
			                                        " is not defined in machine description section");
		result.emplace_back(reg_id, reg->second.size);
		section_reader.read(result.back().second.data(), reg->second.size);
	}

	if (machine.registers.size() != result.size())
		throw MalformedSection(section_reader.name(), std::string("Missing ") +
		                                        std::to_string(machine.registers.size() - result.size()) +
		                                        " register(s) in initial context");

	section_reader.seek_to_end();
	return result;
}

}}}}}
