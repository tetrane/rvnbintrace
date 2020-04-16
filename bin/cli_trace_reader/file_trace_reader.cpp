#include <cstdint>
#include <iomanip>
#include <iostream>
#include <set>
#include <unordered_map>
#include <algorithm>

#include <trace_reader.h>
#include <reader_errors.h>

using namespace reven::backend::plugins::file::libbintrace;

class TracePrinter : public TraceReader {
public:
	TracePrinter(const std::string& filename, bool show_initial)
	  : TraceReader(std::make_unique<std::ifstream>(filename))
	{
		for (const auto& reg : machine().registers) {
			context[reg.first] = std::vector<std::uint8_t>(reg.second.size, 0);
		}

		for (const auto& reg : initial_registers()) {
			if (show_initial) {
				std::cout << machine().registers.at(reg.first).name << "=";
				print_buffer_as_value(reg.second.data(), reg.second.size());
				std::cout << " ";
			}
			context[reg.first] = reg.second;
		}
		if (show_initial) {
			std::cout << std::endl;
		}
	}

	void start_reading()
	{
		registers_to_show_.clear();
		while (read_next_event()) {
			for (auto reg_id : registers_to_show_) {
				print_register(reg_id);
			}
			std::cout << std::endl;
			registers_to_show_.clear();
		}
	}

	void show_info()
	{
		std::cout << "Trace file information:" << std::endl;
		std::cout << "\t- version: " << metadata().format_version().to_string() << std::endl;
		std::cout << "\t- architecture: ";
		switch (machine().architecture) {
			case MachineDescription::Archi::x64_1:
				std::cout << "x64 (v1)";
				break;
			default:
				std::cout << "unknown";
		}
		std::cout << std::dec << std::endl;
		std::cout << "\t- event count: " << event_count() << std::endl;
		std::cout << "\t- pointer size: " << (+machine().physical_address_size & 0xff) << " bytes " << std::endl;
		std::cout << "\t- memory size: " << 1.0 * machine().total_physical_size() / (1024.0 * 1024.0) << " MiB"
		          << std::hex << std::endl;
		for (const auto& region : machine().memory_regions) {
			std::cout << "\t\t"
			          << "region 0x" << region.start << " - 0x" << region.start + region.size << std::endl;
		}
		std::cout << "\t- static registers: " << std::endl;
		for (const auto& static_reg : machine().static_registers) {
			std::cout << "\t\t" << static_reg.first << ": ";
			print_buffer_as_value(reinterpret_cast<const std::uint8_t*>(static_reg.second.data()),
			                      static_reg.second.size());
			std::cout << std::endl;
		}

		return;
	}

protected:
	void print_buffer_as_value(const std::uint8_t* buffer, std::uint64_t size)
	{
		std::cout << "0x";
		for (std::uint64_t i = size; i > 0; --i) {
			std::cout << std::hex << std::setw(2) << std::setfill('0') << (+buffer[i - 1] & 0xff);
		}
	}

	std::pair<const std::uint8_t*, std::uint8_t*> do_register_rw_buffers(RegisterId reg_id) override
	{
		if (std::find(registers_to_show_.begin(), registers_to_show_.end(), reg_id) == registers_to_show_.end())
			registers_to_show_.push_back(reg_id);
		return { context[reg_id].data(), context[reg_id].data() };
	}

	void print_register(RegisterId reg_id)
	{
		std::cout << machine().registers.at(reg_id).name << "=";
		print_buffer_as_value(context[reg_id].data(), machine().registers.at(reg_id).size);
		std::cout << " ";
	}

	std::pair<std::uint8_t*, std::uint64_t> do_memory_before_write(std::uint64_t, std::uint64_t size) override
	{
		return { buffer_, std::min<std::uint64_t>(size, sizeof(buffer_)) };
	}

	void do_memory_after_write(std::uint64_t address, const std::uint8_t* buffer, std::uint64_t size) override
	{
		std::cout << "0x" << std::hex << address << "=";
		if (size <= 8) {
			print_buffer_as_value(buffer, size);
			std::cout << " ";
		}
		else {
			for (std::uint64_t i = 0; i < size; ++i) {
				std::cout << std::setw(2) << std::setfill('0') << (+buffer[i] & 0xff) << " ";
			}
		}
	}

	void do_event_instruction() override { std::cout << "#" << std::dec << next_event_index() << ": "; }
	void do_event_other(const std::string& description) override
	{
		std::cout << "#" << std::dec << next_event_index() << " Other event (" << description << "): ";
	}

	std::array<std::vector<std::uint8_t>, 0xffff> context;
	std::vector<RegisterId> registers_to_show_;
	std::uint8_t buffer_[4096];
};

int
main(int argc, char* argv[])
{
	if (argc < 2) {
		std::cout << "Usage of trace tracer" << std::endl;
		std::cout << argv[0] << " [OPTIONS] <trace_file>" << std::endl;
		std::cout << "Options are:" << std::endl;
		std::cout << "\t--info: show basic information about trace and return" << std::endl;
		std::cout << "\t--initial: show initial cpu context along with trace" << std::endl;
		return 1;
	}

	bool show_info = false;
	bool show_initial = false;
	for (int i = 1; i < argc - 1; ++i) {
		if (std::string(argv[i]) == "--info")
			show_info = true;
		if (std::string(argv[i]) == "--initial")
			show_initial = true;
	}

	try {
		TracePrinter reader(argv[argc - 1], show_initial);

		if (show_info) {
			reader.show_info();
			return 0;
		}

		reader.start_reading();
	}
	catch (const std::exception& e) {
		std::cout << "Error reading: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
