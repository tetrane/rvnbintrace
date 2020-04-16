#define BOOST_TEST_MODULE RVN_BINARY_TRACE_READER
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <cstdint>

#include <reader_errors.h>
#include <trace_reader.h>

#include "helpers.h"

using namespace std;
using namespace reven::backend::plugins::file::libbintrace;

class TraceReaderTester : public TraceReader
{
public:
	TraceReaderTester(std::unique_ptr<istream>&& s) : TraceReader(std::move(s)) {}
	TraceReaderTester(StreamWrapper& s) : TraceReader(s.to_stream_with_bin_metadata()) {}
	TraceReaderTester(StreamWrapper& s, const char* format_version) : TraceReader(s.to_stream_with_bin_metadata(format_version)) {}

	uint64_t last_value = 0;
	string last_register;
	uint8_t memory_buffer[2];
	std::map<std::uint64_t, std::uint16_t> memory;
	string last_event;
protected:
	std::pair<const std::uint8_t*, std::uint8_t*> do_register_rw_buffers(RegisterId reg_id) override
	{
		last_register = machine().registers.at(reg_id).name;

		// Ensure upper part of value is cleared so tests are valid regardless of size
		auto size = machine().registers.at(reg_id).size;
		if (size < sizeof(last_value)) {
			std::uint64_t mask = 1ul << size * 8;
			mask -= 1;
			last_value &= mask;
		}

		auto buffer = reinterpret_cast<uint8_t*>(&last_value);
		return { buffer, buffer };
	}
	std::pair<std::uint8_t*, std::uint64_t> do_memory_before_write(std::uint64_t, std::uint64_t size) override
	{
		return {memory_buffer, std::min(size, 2ul)};
	}
	void do_memory_after_write(std::uint64_t address, const std::uint8_t* buffer, std::uint64_t size) override
	{
		if (size == 1)
			memory[address] = *buffer;
		else if (size == 2)
			memory[address] = *reinterpret_cast<const std::uint16_t*>(buffer);
		else
			throw std::runtime_error("memory size should be less than 2!");
	}
	void do_event_instruction() override
	{
		last_event = "instruction";
		last_value = 0;
		last_register.clear();
		memory.clear();
	}

	void do_event_other(const std::string& description) override
	{
		last_event = description;
		last_value = 0;
		last_register.clear();
		memory.clear();
	}
};

BOOST_AUTO_TEST_CASE(test_reader_base)
{
	StreamWrapper s;
	s.reset();

	uint64_t header_size = 17;
	s.write<uint64_t>(header_size)
		.write<uint8_t>(0)

		.write<uint64_t>(0).write<uint64_t>(0) // artifical padding
	;

	// Machine description
	uint64_t description_size = 115;
	s.write<uint64_t>(description_size)
	    .write({ 'x', '6', '4', '1' }).write<uint8_t>(1)
		.write<uint32_t>(1) // memory regions
			.write<uint8_t>(0).write<uint8_t>(0x8)
		.write<uint32_t>(2) // registers
			.write<uint16_t>(0).write<uint16_t>(4).write<uint8_t>(3).write({ 'e', 'a', 'x' })
			.write<uint16_t>(1).write<uint16_t>(8).write<uint8_t>(3).write({ 'r', 'a', 'x' })
		.write<uint32_t>(5) // actions
			.write<uint8_t>(0xfa).write<uint16_t>(1).write<uint8_t>(1).write<std::uint64_t>(0x15)       // add rax
			.write<uint8_t>(0xfb).write<uint16_t>(0).write<uint8_t>(0).write<std::uint32_t>(0x11333377) // set eax
			.write<uint8_t>(0xfc).write<uint16_t>(0).write<uint8_t>(1).write<std::uint32_t>(0xfffefffe) // add
			.write<uint8_t>(0xfd).write<uint16_t>(0).write<uint8_t>(2).write<std::uint32_t>(0xf0f00f0f) // and
			.write<uint8_t>(0xfe).write<uint16_t>(0).write<uint8_t>(3).write<std::uint32_t>(0x0f0ff0f0) // or
		.write<uint32_t>(0) // static register

		.write<uint64_t>(0).write<uint64_t>(0) // artifical padding
		.write<uint64_t>(0).write<uint64_t>(0) // ...
	;

	// Memory regions
	s.write<uint64_t>(0x8).write<uint64_t>(0x0102030410121314);

	// Initial context
	s.write<uint64_t>(20).write<uint32_t>(2)
		.write<uint16_t>(0).write<uint32_t>(0x00000000)
		.write<uint16_t>(1).write<uint64_t>(0xff000000000000aa);

	// Packet section
	s.write<uint64_t>(0xffff)
	 .write<uint64_t>(13) // Number of events below
		.write<uint8_t>(0x20) // Simple registers with different sizes
			.write<uint8_t>(1).write<uint64_t>(0xffaabbccddeeffaa)
			.write<uint8_t>(0).write<uint32_t>(0xffaabbdd)
		.write<uint8_t>(0x10) // register byte is > 0xff, then Register ID is specified on 2 bytes
			.write<uint8_t>(0xff).write<uint16_t>(1).write<uint64_t>(0xffaabbccddeeffaa)
		.write<uint8_t>(0xff).write<uint8_t>(0xff) // simple event
			.write<uint8_t>(6).write({'e', 'v', 'e', 'n', 't', '!'})
			.write<uint8_t>(0x10) // with diff
				.write<uint8_t>(0).write<uint32_t>(0xeeffaa00)
		.write<uint8_t>(0x11) // Registers and Memory
			.write<uint8_t>(0x42).write<uint8_t>(4).write<uint32_t>(0xffaabbcc)
			.write<uint8_t>(0).write<uint32_t>(0xffaabbee)
		.write<uint8_t>(0x01) // Size is > 0xff, then size is on PHY bytes
			.write<uint8_t>(0x48).write<uint8_t>(0xff).write<uint8_t>(8).write<uint64_t>(0xaabbccddeeffaaffu)
		.write<uint8_t>(0x20) // Register operation
			.write<uint8_t>(1).write<uint64_t>(0xffddbbccddeeffaa) // Set
			.write<uint8_t>(0xfb)
		.write<uint8_t>(0x20)
			.write<uint8_t>(0).write<uint32_t>(0x10a02201) // add (wraps around)
			.write<uint8_t>(0xfc)
		.write<uint8_t>(0x20)
			.write<uint8_t>(0).write<uint32_t>(0xaa55aa55) // and
			.write<uint8_t>(0xfd)
		.write<uint8_t>(0x20)
			.write<uint8_t>(0).write<uint32_t>(0x55aa55aa) // or
			.write<uint8_t>(0xfe)
		.write<uint8_t>(0x20)
			.write<uint8_t>(1).write<uint64_t>(0x0000000082621635) // add rax
			.write<uint8_t>(0xfa)
		.write<uint8_t>(0x00) // Empty diff
	;

	// Packet with continuation for registers
	s.write<uint8_t>(0xf1);
		s.write<uint8_t>(0x42).write<uint8_t>(4).write<uint32_t>(0u);
	for (int i=0; i < 0xe; ++i)
		s.write<uint8_t>(0).write<uint32_t>(0xddeeffbb);
	s.write<uint8_t>(0x11);
		s.write<uint8_t>(0x46).write<uint8_t>(4).write<uint32_t>(0x11112222);
		s.write<uint8_t>(0).write<uint32_t>(0xddeeffcc);

	// Packet with continuation for memories
	s.write<uint8_t>(0x1f);
	for (int i=0; i < 0xe; ++i)
		s.write<uint8_t>(0x42).write<uint8_t>(4).write<uint32_t>(0u);
	s.write<uint8_t>(0).write<uint32_t>(0xddeeffcc);
	s.write<uint8_t>(0x11);
		s.write<uint8_t>(0x46).write<uint8_t>(4).write<uint32_t>(0x11112222);
		s.write<uint8_t>(0).write<uint32_t>(0xddeeffdd);

	auto trace = TraceReaderTester(s);

	BOOST_CHECK(trace.read_next_event());
	BOOST_CHECK_EQUAL(trace.last_event, "instruction");
	BOOST_CHECK_EQUAL(trace.last_register, "eax");
	BOOST_CHECK_EQUAL(trace.last_value, 0xffaabbdd);

	BOOST_CHECK(trace.read_next_event());
	BOOST_CHECK_EQUAL(trace.last_register, "rax");
	BOOST_CHECK_EQUAL(trace.last_value, 0xffaabbccddeeffaa);

	BOOST_CHECK(trace.read_next_event());
	BOOST_CHECK_EQUAL(trace.last_event, "event!");
	BOOST_CHECK_EQUAL(trace.last_register, "eax");
	BOOST_CHECK_EQUAL(trace.last_value, 0xeeffaa00);

	BOOST_CHECK(trace.read_next_event());
	BOOST_CHECK_EQUAL(trace.memory[0x42], 0xbbcc);
	BOOST_CHECK_EQUAL(trace.memory[0x44], 0xffaa);
	BOOST_CHECK_EQUAL(trace.last_register, "eax");
	BOOST_CHECK_EQUAL(trace.last_value, 0xffaabbee);

	BOOST_CHECK(trace.read_next_event());
	BOOST_CHECK_EQUAL(trace.memory[0x48+0], 0xaaff);
	BOOST_CHECK_EQUAL(trace.memory[0x48+2], 0xeeff);
	BOOST_CHECK_EQUAL(trace.memory[0x48+4], 0xccdd);
	BOOST_CHECK_EQUAL(trace.memory[0x48+6], 0xaabb);

	BOOST_CHECK(trace.read_next_event());
	BOOST_CHECK_EQUAL(trace.last_register, "eax");
	BOOST_CHECK_EQUAL(trace.last_value, 0x11333377);
	BOOST_CHECK(trace.read_next_event());
	BOOST_CHECK_EQUAL(trace.last_register, "eax");
	BOOST_CHECK_EQUAL(trace.last_value, static_cast<std::uint32_t>(0x10a02201 + 0xfffefffe));
	BOOST_CHECK(trace.read_next_event());
	BOOST_CHECK_EQUAL(trace.last_register, "eax");
	BOOST_CHECK_EQUAL(trace.last_value, 0xf0f00f0f & 0xaa55aa55);
	BOOST_CHECK(trace.read_next_event());
	BOOST_CHECK_EQUAL(trace.last_register, "eax");
	BOOST_CHECK_EQUAL(trace.last_value, 0x0f0ff0f0 | 0x55aa55aa);
	BOOST_CHECK(trace.read_next_event());
	BOOST_CHECK_EQUAL(trace.last_register, "rax");
	BOOST_CHECK_EQUAL(trace.last_value, 0x0000000082621635 + 0x15);

	BOOST_CHECK(trace.read_next_event());
	BOOST_CHECK_EQUAL(trace.last_register, "");

	BOOST_CHECK(trace.read_next_event());
	BOOST_CHECK_EQUAL(trace.memory[0x42], 0x0);
	BOOST_CHECK_EQUAL(trace.memory[0x46], 0x2222);
	BOOST_CHECK_EQUAL(trace.last_register, "eax");
	BOOST_CHECK_EQUAL(trace.last_value, 0xddeeffcc);

	BOOST_CHECK(trace.read_next_event());
	BOOST_CHECK_EQUAL(trace.memory[0x42], 0x0);
	BOOST_CHECK_EQUAL(trace.memory[0x46], 0x2222);
	BOOST_CHECK_EQUAL(trace.last_register, "eax");
	BOOST_CHECK_EQUAL(trace.last_value, 0xddeeffdd);

	// The number of events is fixed, it should report no more.
	// See header section to update the event count
	BOOST_CHECK(not trace.read_next_event());

	auto md = trace.metadata();

	BOOST_CHECK(md.type() == reven::metadata::ResourceType::TraceBin);
	BOOST_CHECK(md.format_version().to_string() == TestMDWriter::format_version);
	BOOST_CHECK(md.tool_name() == TestMDWriter::tool_name);
	BOOST_CHECK(md.tool_version().to_string() == TestMDWriter::tool_version);
	BOOST_CHECK(md.tool_info() == TestMDWriter::tool_info);
	BOOST_CHECK(md.generation_date() == std::chrono::system_clock::time_point(std::chrono::seconds(TestMDWriter::generation_date)));
}

BOOST_AUTO_TEST_CASE(test_incompatible_type_bin)
{
	StreamWrapper s;
	s.write<uint32_t>(0x46544252);

	BOOST_CHECK_THROW(TraceReaderTester(s.reset().to_stream_with_cache_metadata("1.0.0")), IncompatibleTypeException);
}

BOOST_AUTO_TEST_CASE(test_incompatible_version_bin)
{
	StreamWrapper s;
	s.write<uint32_t>(0x46544252);

	BOOST_CHECK_THROW(TraceReaderTester(s.reset(), "0.0.0"), IncompatibleVersionException);
	BOOST_CHECK_THROW(TraceReaderTester(s.reset(), "2.0.0"), IncompatibleVersionException);
}
