#include <trace_section_writers.h>

#include <memory>

#include <writer_errors.h>
#include <section_writer.h>

namespace reven {
namespace backend {
namespace plugins {
namespace file {
namespace libbintrace {

void write_trace_header(binresource::Writer& writer, const Header& data)
{
	SectionWriter section_writer("trace header", &writer);

	section_writer.write<std::uint8_t>(data.compression);

	section_writer.finalize();
}

void write_trace_machine_description(binresource::Writer& writer, const MachineDescription& data)
{
	SectionWriter section_writer("trace header", &writer);

	std::uint32_t magic_value = 0;
	switch(data.architecture) {
		case MachineDescription::Archi::x64_1:
			magic_value = 0x31343678;
			break;
		case MachineDescription::Archi::arm64_1:
			magic_value = 0x316d7261;
			break;
	}
	section_writer.write<std::uint32_t>(magic_value);

	if (data.physical_address_size < 1 or data.physical_address_size > 8) {
		throw NonsenseValue(section_writer.name(), "physical size must be between 1 and 8");
	}
	section_writer.write<std::uint8_t>(data.physical_address_size);

	section_writer.write<std::uint32_t>(data.memory_regions.size());
	for (const auto& region : data.memory_regions) {
		section_writer.write(region.start, data.physical_address_size);
		section_writer.write(region.size, data.physical_address_size);
	}

	section_writer.write<std::uint32_t>(data.registers.size());
	for (const auto& reg : data.registers) {
		section_writer.write<RegisterId>(reg.first);
		section_writer.write<std::uint16_t>(reg.second.size);
		section_writer.write_string<std::uint8_t>(reg.second.name);
	}

	section_writer.write<std::uint32_t>(data.register_operations.size());
	for (const auto& reg_action : data.register_operations) {
		if (reg_action.first >= 0xff)
			throw NonsenseValue(section_writer.name(), "register operation id cannot be >= 0xff");
		if (data.registers.find(reg_action.first) != data.registers.end())
			throw NonsenseValue(section_writer.name(), std::string("register operation ") + std::to_string(reg_action.first) +
			                                     " overlaps with register id");
		auto reg = data.registers.find(reg_action.second.register_id);
		if (reg == data.registers.end())
			throw NonsenseValue(section_writer.name(), std::string("register operation refered to unknown register id ") +
			                                     std::to_string(reg_action.second.register_id));
		if (reg_action.second.value.size() != reg->second.size)
			throw NonsenseValue(
			  section_writer.name(), std::string("register operation content size doesn't match that of destination register ") +
			                   std::to_string(reg_action.second.register_id));

		section_writer.write<std::uint8_t>(reg_action.first);
		section_writer.write<RegisterId>(reg_action.second.register_id);
		section_writer.write<std::uint8_t>(static_cast<std::uint8_t>(reg_action.second.operation));
		section_writer.write_buffer(reinterpret_cast<const std::uint8_t*>(reg_action.second.value.data()), reg_action.second.value.size());
	}

	section_writer.write<std::uint32_t>(data.static_registers.size());
	for (const auto& static_reg : data.static_registers) {
		section_writer.write_string<std::uint8_t>(static_reg.first);
		section_writer.write_sized_buffer<std::uint8_t>(static_reg.second.data(), static_reg.second.size());
	}

	section_writer.finalize();
}

InitialMemorySectionWriter::InitialMemorySectionWriter(binresource::Writer&& writer, const MachineDescription& machine)
	: ExternalSectionTraceWriter(std::move(writer), machine, "trace memory") {}

void InitialMemorySectionWriter::write(const std::uint8_t* buffer, std::uint64_t size)
{
	section_writer_.write_buffer(buffer, size);
}

void  InitialMemorySectionWriter::do_finalize()
{
	if (section_writer_.bytes_written() != machine_.total_physical_size()) {
		throw MissingData(section_writer_.name(),
		                  std::string("Memory dump size " + std::to_string(section_writer_.bytes_written()) +
		                              " does not match declared physical size of ") +
		                    std::to_string(machine_.total_physical_size()));
	}
}

InitialRegistersSectionWriter::InitialRegistersSectionWriter(binresource::Writer&& writer, const MachineDescription& machine)
	: ExternalSectionTraceWriter(std::move(writer), machine, "trace initial context")
{
	initial_context_count_stream_pos_ = stream_pos();
	initial_context_count_ = 0;
	section_writer_.write<std::uint32_t>(0u);
}

void InitialRegistersSectionWriter::write(RegisterId reg_id, const std::uint8_t* buffer, std::uint64_t size)
{
	auto reg_size = find_register(reg_id).size;

	if (reg_size != size)
		throw NonsenseValue(section_writer_.name(), "Register size doesn't match definition");

	section_writer_.write<RegisterId>(reg_id);
	section_writer_.write_buffer(buffer, size);
	initial_context_count_++;
}

void InitialRegistersSectionWriter::do_finalize()
{
	write_at_position(initial_context_count_, initial_context_count_stream_pos_);
}

EventsSectionWriter::EventsSectionWriter(binresource::Writer&& writer, const MachineDescription& machine)
	: ExternalSectionTraceWriter(std::move(writer), machine, "trace events")
	, event_count_(0)
	, diff_element_count_stream_pos_(-1)
	, current_diff_mem_count_(0)
	, current_diff_reg_count_(0)
{
	event_count_stream_pos_ = stream_pos();
	section_writer_.write<std::uint64_t>(0u);
	event_count_ = 0;
}

void EventsSectionWriter::do_finalize()
{
	write_at_position(event_count_, event_count_stream_pos_);
}

bool EventsSectionWriter::is_event_started()
{
	return diff_element_count_stream_pos_ != -1;
}

void EventsSectionWriter::start_event_instruction()
{
	if (is_event_started())
		throw std::logic_error("Called start_event_instruction before finish_event");

	current_diff_mem_count_ = 0;
	current_diff_reg_count_ = 0;
	diff_element_count_stream_pos_ = stream_pos();
	section_writer_.write<std::uint8_t>(0u);
}

void EventsSectionWriter::start_event_other(const std::string& description)
{
	if (is_event_started())
		throw std::logic_error("Called start_event_other before finish_event");

	section_writer_.write<std::uint8_t>(0xff); // not instruction diff
	section_writer_.write<std::uint8_t>(0xff); // "other" type
	section_writer_.write_string<std::uint8_t>(description);
	start_event_instruction();
}

void EventsSectionWriter::finish_event()
{
	if (not is_event_started())
		throw std::logic_error("Called finish_event before start_event_instruction or other starting function");

	write_event_diff_size();
	event_count_++;
	diff_element_count_stream_pos_ = -1;
}

void EventsSectionWriter::write_event_diff_size()
{
	if (current_diff_mem_count_ > 0xf)
		throw std::logic_error("Cannot write diff size if memories > 0xf");
	if (current_diff_reg_count_ > 0xf)
		throw std::logic_error("Cannot write diff size if registers > 0xf");
	if (current_diff_mem_count_ ==  0xf and current_diff_reg_count_ == 0xf)
		throw std::logic_error("Cannot write diff size if both mem and ref == 0xf");

	std::uint8_t diff_size = current_diff_mem_count_ | (current_diff_reg_count_ << 4);
	write_at_position(diff_size, diff_element_count_stream_pos_);
}

void EventsSectionWriter::continue_on_next_event()
{
	write_event_diff_size();
	diff_element_count_stream_pos_ = -1;
	start_event_instruction();
}

void EventsSectionWriter::write_memory(std::uint64_t address, const std::uint8_t* buffer, std::uint64_t size)
{
	if (not is_event_started())
		throw std::logic_error("Called write_memory before start_event_instruction or other starting function");

	if (current_diff_reg_count_ > 0)
		throw std::logic_error("Called write_memory after write_packet_register");

	current_diff_mem_count_++;
	if (current_diff_mem_count_ == 0xf) {
		continue_on_next_event();
		current_diff_mem_count_ = 1;
	}

	section_writer_.write(address, machine_.physical_address_size);
	if (size < 0xff) {
		section_writer_.write<std::uint8_t>(size);
	} else {
		section_writer_.write<std::uint8_t>(0xff);
		section_writer_.write(size, machine_.physical_address_size);
	}
	section_writer_.write_buffer(buffer, size);
}

void EventsSectionWriter::write_register(RegisterId reg_id, const std::uint8_t* buffer, std::uint64_t size)
{
	if (not is_event_started())
		throw std::logic_error("Called write_register before start_event_instruction or other starting function");

	auto reg_size = find_register(reg_id).size;
	if (reg_size != size)
		throw NonsenseValue(section_writer_.name(), "Register size doesn't match definition");

	current_diff_reg_count_++;
	if (current_diff_reg_count_ == 0xf) {
		continue_on_next_event();
		current_diff_reg_count_ = 1;
	}

	if (reg_id < 0xff) {
		section_writer_.write<std::uint8_t>(reg_id);
	} else {
		section_writer_.write<std::uint8_t>(0xff);
		section_writer_.write<RegisterId>(reg_id);
	}
	section_writer_.write_buffer(buffer, size);
}

void EventsSectionWriter::write_register_action(RegisterId reg_id)
{
	if (not is_event_started())
		throw std::logic_error("Called write_register_action before start_event_instruction or other starting function");

	auto reg = machine_.register_operations.find(reg_id);
	if (reg == machine_.register_operations.end())
		throw NonsenseValue(section_writer_.name(), std::to_string(reg_id) + "in an invalid register operation id");

	current_diff_reg_count_++;
	if (current_diff_reg_count_ == 0xf) {
		continue_on_next_event();
		current_diff_reg_count_ = 1;
	}

	section_writer_.write<std::uint8_t>(reg_id);
}

}}}}}
