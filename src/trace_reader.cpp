#include <trace_reader.h>

#include <cstdint>
#include <cstring>
#include <utility>
#include <set>
#include <tuple>
#include <algorithm>

#include <common.h>
#include <trace_section_readers.h>
#include <reader_errors.h>

namespace reven {
namespace backend {
namespace plugins {
namespace file {
namespace libbintrace {

TraceReader::TraceReader(std::unique_ptr<std::istream>&& input_stream)
	: reader_(binresource::Reader::open(std::move(input_stream)))
	, events_reader_(nullptr), current_event_id_(0)
{
	if (not reader_.stream())
		throw UnexpectedEndOfStream("trace magic");

	if (metadata().type() != metadata::ResourceType::TraceBin) {
		throw IncompatibleTypeException("Can't open a resource of type different from TraceBin");
	}

	const auto cmp = metadata().format_version().compare(metadata::Version::from_string(format_version));

	if (!cmp.is_compatible()) {
		if (cmp.detail < metadata::Version::Comparison::Current) {
			throw IncompatibleVersionException(
				("Incompatible version " + metadata().format_version().to_string() + ": Past version").c_str()
			);
		} else {
			throw IncompatibleVersionException(
				("Incompatible version " + metadata().format_version().to_string() + ": Future version").c_str()
			);
		}
	}

	header_ = read_trace_header(reader_);

	machine_description_ = read_trace_machine_description(reader_);

	build_quick_access_vectors(machine_description_);

	SectionReader regions_reader("trace memory", reader_);
	if (regions_reader.bytes_left() != machine().total_physical_size())
		throw MalformedSection(regions_reader.name(),
		                       std::string("Section size does not fit initial memory regions size: " +
		                                   std::to_string(regions_reader.bytes_left()) + " != " +
		                                   std::to_string(machine().total_physical_size())));

	for (const auto& region : machine().memory_regions) {
		memory_positions_.push_back(reader_.stream().tellg());
		reader_.stream().seekg(region.size, std::ios_base::cur);
		if (not reader_.stream())
			throw UnexpectedEndOfStream(regions_reader.name());
	}

	initial_cpu_ = read_initial_cpu_context(reader_, machine());

	events_reader_ = std::make_unique<SectionReader>("trace events", reader_);
	if (events_reader_->bytes_left() == 0)
		throw MalformedSection(events_reader_->name(), "Section cannot be of size 0");
	event_count_ = events_reader_->read<std::uint64_t>();
}

void TraceReader::build_quick_access_vectors(const MachineDescription& machine)
{
	auto max_reg_action = std::max_element(machine.register_operations.begin(), machine.register_operations.end(),
	                                       machine.register_operations.value_comp());
	if (max_reg_action != machine.register_operations.end()) {
		if (register_operations_.size() <= max_reg_action->first) {
			register_operations_.resize(max_reg_action->first + 1,
			                            std::make_pair(false, MachineDescription::RegisterOperation()));
		}
		for (const auto& action : machine.register_operations) {
			register_operations_[action.first].first = true;
			register_operations_[action.first].second = action.second;
		}
	}

	auto max_register =
	  std::max_element(machine.registers.begin(), machine.registers.end(), machine.registers.value_comp());
	if (max_register != machine.registers.end()) {
		registers_.resize(max_register->first + 1, std::make_pair(false, MachineDescription::Register()));
		for (const auto& reg : machine.registers) {
			registers_[reg.first].first = true;
			registers_[reg.first].second = reg.second;
		}
	}
}

std::uint64_t TraceReader::stream_pos()
{
	if (!events_reader_)
		throw std::logic_error("Called stream_pos before read_initial_context");
	return events_reader_->stream_pos();
}

void TraceReader::seek(std::uint64_t context_id, std::uint64_t stream_position)
{
	if (!events_reader_)
		throw std::logic_error("Called seek before read_initial_context");
	events_reader_->seek(stream_position);
	current_event_id_ = context_id;
}

bool TraceReader::read_next_event()
{
	if (current_event_id_ >= event_count_)
		return false;
	if (!events_reader_)
		throw std::logic_error("Called read_next_event before read_initial_context");

	auto diff_size = events_reader_->read<std::uint8_t>();
	std::uint16_t reg_count = 0;
	std::uint16_t mem_count = 0;

	if (diff_size < 0xff) {
		do_event_instruction();
	} else {
		auto type = events_reader_->read<std::uint8_t>();
		switch (type) {
			case 0xff:
				do_event_other(events_reader_->read_string<std::uint8_t>());
				break;
			default:
				throw MalformedSection(events_reader_->name(), std::to_string(type) + " is an unknown type of event");
		}
		diff_size = events_reader_->read<std::uint8_t>();
	}

	for(;;) {
		mem_count = (diff_size >> 0) & 0xf;
		reg_count = (diff_size >> 4) & 0xf;

		for (std::size_t i = 0; i < mem_count and i < 0xe; ++i) {
			auto address = events_reader_->read<std::uint64_t>(machine().physical_address_size);

			std::uint64_t size = events_reader_->read<std::uint8_t>();
			if (size == 0xff)
				size = events_reader_->read<std::uint64_t>(machine().physical_address_size);

			process_memory_write(events_reader_.get(), address, size);
		}

		for (std::size_t i = 0; i < reg_count and i < 0xe; ++i) {
			RegisterId reg_id = events_reader_->read<std::uint8_t>();
			if (reg_id == 0xff)
				reg_id = events_reader_->read<RegisterId>();

			process_register_write(events_reader_.get(), reg_id);
		}

		if (mem_count == 0xf or reg_count == 0xf) {
			// Read the continuation diff
			diff_size = events_reader_->read<std::uint8_t>();
			if (diff_size == 0xff)
				throw MalformedSection(events_reader_->name(), "Continuation diff with diff size 0xff is forbidden");
		} else {
			break;
		}
	}

	current_event_id_++;

	return true;
}

void TraceReader::process_register_write(SectionReader* reader, RegisterId reg_id)
{
	if (reg_id < register_operations_.size() and register_operations_[reg_id].first) {
		const auto& reg_operation = register_operations_[reg_id].second;
		auto dest_reg_id = reg_operation.register_id;
		const auto& content = reg_operation.value;

		const std::uint8_t* reg_read;
		std::uint8_t* reg_write;
		std::tie(reg_read, reg_write) = do_register_rw_buffers(dest_reg_id);
		switch (reg_operation.operation) {
			case MachineDescription::RegisterOperator::Set:
				std::memcpy(reg_write, content.data(), content.size());
				break;

			case MachineDescription::RegisterOperator::Add:
				switch (content.size()) {
					case 1:
						*reg_write = *reg_read + *content.data();
						break;
					case 2:
						*reinterpret_cast<std::uint16_t*>(reg_write) =
						  *reinterpret_cast<const std::uint16_t*>(reg_read) +
						  *reinterpret_cast<const std::uint16_t*>(content.data());
						break;
					case 4:
						*reinterpret_cast<std::uint32_t*>(reg_write) =
						  *reinterpret_cast<const std::uint32_t*>(reg_read) +
						  *reinterpret_cast<const std::uint32_t*>(content.data());
						break;
					case 8:
						*reinterpret_cast<std::uint64_t*>(reg_write) =
						  *reinterpret_cast<const std::uint64_t*>(reg_read) +
						  *reinterpret_cast<const std::uint64_t*>(content.data());
						break;
					default: {
						std::uint8_t carry = 0;
						for (std::size_t i = 0; i < content.size(); ++i) {
							auto result =
							  static_cast<std::uint16_t>(reg_read[i]) + static_cast<std::uint8_t>(content[i]) + carry;
							carry = result >> 8;
							reg_write[i] = result & 0xff;
						}
					}
				}
				break;

			case MachineDescription::RegisterOperator::And:
				switch (content.size()) {
					case 1:
						*reg_write = *reg_read & *content.data();
						break;
					case 2:
						*reinterpret_cast<std::uint16_t*>(reg_write) =
						  *reinterpret_cast<const std::uint16_t*>(reg_read) &
						  *reinterpret_cast<const std::uint16_t*>(content.data());
						break;
					case 4:
						*reinterpret_cast<std::uint32_t*>(reg_write) =
						  *reinterpret_cast<const std::uint32_t*>(reg_read) &
						  *reinterpret_cast<const std::uint32_t*>(content.data());
						break;
					case 8:
						*reinterpret_cast<std::uint64_t*>(reg_write) =
						  *reinterpret_cast<const std::uint64_t*>(reg_read) &
						  *reinterpret_cast<const std::uint64_t*>(content.data());
						break;
					default:
						for (std::size_t i = 0; i < content.size(); ++i)
							reg_write[i] = reg_read[i] & content[i];
				}
				break;

			case MachineDescription::RegisterOperator::Or:
				switch (content.size()) {
					case 1:
						*reg_write = *reg_read | *content.data();
						break;
					case 2:
						*reinterpret_cast<std::uint16_t*>(reg_write) =
						  *reinterpret_cast<const std::uint16_t*>(reg_read) |
						  *reinterpret_cast<const std::uint16_t*>(content.data());
						break;
					case 4:
						*reinterpret_cast<std::uint32_t*>(reg_write) =
						  *reinterpret_cast<const std::uint32_t*>(reg_read) |
						  *reinterpret_cast<const std::uint32_t*>(content.data());
						break;
					case 8:
						*reinterpret_cast<std::uint64_t*>(reg_write) =
						  *reinterpret_cast<const std::uint64_t*>(reg_read) |
						  *reinterpret_cast<const std::uint64_t*>(content.data());
						break;
					default:
						for (std::size_t i = 0; i < content.size(); ++i)
							reg_write[i] = reg_read[i] | content[i];
				}
				break;
			default:
				throw MalformedSection(
				  reader->name(), std::string("Operation ") +
				                    std::to_string(static_cast<std::uint8_t>(reg_operation.operation)) + " is unknown");
		}
		return;
	}

	if (reg_id >= registers_.size() or not registers_[reg_id].first)
		throw MalformedSection(reader->name(), std::string("Register or action ") + std::to_string(reg_id) +
		                                        " is not defined in machine description section");

	std::uint8_t* reg_write;
	std::tie(std::ignore, reg_write) = do_register_rw_buffers(reg_id);
	reader->read(reg_write, registers_[reg_id].second.size);
}

void TraceReader::process_memory_write(SectionReader* reader, std::uint64_t address, std::uint64_t size)
{
	for(; size > 0;) {
		auto buffer = do_memory_before_write(address, size);

		reader->read(buffer.first, buffer.second);
		do_memory_after_write(address, buffer.first, buffer.second);

		address += buffer.second;
		size -= buffer.second;
	}
}

metadata::Version TraceReader::resource_version()
{
	return metadata::Version::from_string(format_version);
}

metadata::ResourceType TraceReader::resource_type()
{
	return reven::metadata::ResourceType::TraceBin;
}

}}}}}
