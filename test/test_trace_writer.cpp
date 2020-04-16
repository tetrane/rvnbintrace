#define BOOST_TEST_MODULE RVN_BINARY_TRACE_WRITER
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <iomanip>
#include <cstdint>
#include <memory>

#include <writer_errors.h>
#include <trace_writer.h>
#include <section_writer.h>

#include "helpers.h"

using namespace std;
using namespace reven::backend::plugins::file::libbintrace;

class TraceWriterTester : public TraceWriter
{
public:
	TraceWriterTester(const MachineDescription& desc)
	  : TraceWriter(make_unique<stringstream>(), desc,
	                "TestTraceWriter", "1.0.0", "Tests version 1.0.0") {}

	StreamWrapper stream() {
		return StreamWrapper{
			stringstream(static_cast<stringstream*>(&writer_.stream())->str().substr(writer_.md_size()))
		};
	}
};

MachineDescription desc {
	MachineDescription::Archi::x64_1,
	5,
	{ {0, 16} },
	{ {0, {4, "eax"}}, {1, {4, "ebx"}}, {0xf00, {8, "rax"}} },
	{ {0xfe, {0, MachineDescription::RegisterOperator::Set, {'0', '0', '0', '0'}}} },
	{ { "cpuid", { 't', 'o', 't', 'o' } }, { "msr", { 'n', 'o', 's', 't', 'a', 't', 'i', 'c' } } },
};

BOOST_AUTO_TEST_CASE(test_writer_base)
{
	unsigned char memory[16+1] = "0123456789abcdef";
	auto trace = TraceWriterTester(desc);

	auto initial_memory_writer = trace.start_initial_memory_section();
	initial_memory_writer.write(memory, 16);

	auto initial_cpu_writer = trace.start_initial_registers_section(std::move(initial_memory_writer));
	initial_cpu_writer.write(0, memory, 4);
	initial_cpu_writer.write(0xf00, memory, 8);
	auto events_writer = trace.start_events_section(std::move(initial_cpu_writer));

	events_writer.start_event_other("event test");
	events_writer.write_memory(4, memory, 4);
	events_writer.write_register(0, memory, 4);
	events_writer.write_register_action(0xfe);
	events_writer.finish_event();

	events_writer.start_event_instruction();
	events_writer.write_register(0xf00, memory, 8);
	events_writer.finish_event();

	events_writer.start_event_instruction();
	for (int i=0; i < 0x12; ++i)
		events_writer.write_register(0x0, memory, 4);
	events_writer.finish_event();

	events_writer.start_event_instruction();
	events_writer.finish_event();

	events_writer.start_event_instruction();
	events_writer.finish_event();

	trace.finish_events_section(std::move(events_writer));

	auto s = trace.stream();

	s.skip_section(); // skip header, already tested
	s.skip_section(); // skip machine desc, already tested

	// memory
	BOOST_CHECK_EQUAL(s.read<std::uint64_t>(), 16);
	BOOST_CHECK_EQUAL(s.read_data(16), std::string((char*)memory));

	// initial context
	s.read<std::uint64_t>(); // section size
	BOOST_CHECK_EQUAL(s.read<std::uint32_t>(), 2);
	BOOST_CHECK_EQUAL(s.read<std::uint16_t>(), 0);
	BOOST_CHECK_EQUAL(s.read_data(4), std::string((char*)memory).substr(0, 4));
	BOOST_CHECK_EQUAL(s.read<std::uint16_t>(), 0xf00);
	BOOST_CHECK_EQUAL(s.read_data(8), std::string((char*)memory).substr(0, 8));

	// Packets
	s.read<std::uint64_t>(); // section size
	s.read<std::uint64_t>(); // packet count
	BOOST_CHECK_EQUAL(s.read<std::uint8_t>(), 0xff);
	BOOST_CHECK_EQUAL(s.read<std::uint8_t>(), 0xff);
	BOOST_CHECK_EQUAL(s.read<std::uint8_t>(), 10);
	BOOST_CHECK_EQUAL(s.read_data(10), "event test");
	BOOST_CHECK_EQUAL(s.read<std::uint8_t>(), 0x21);
	BOOST_CHECK_EQUAL(s.read<std::uint64_t>(5), 4);
	BOOST_CHECK_EQUAL(s.read<std::uint8_t>(), 4);
	BOOST_CHECK_EQUAL(s.read_data(4), std::string((char*)memory).substr(0, 4));
	BOOST_CHECK_EQUAL(s.read<std::uint8_t>(), 0);
	BOOST_CHECK_EQUAL(s.read_data(4), std::string((char*)memory).substr(0, 4));
	BOOST_CHECK_EQUAL(s.read<std::uint8_t>(), 0xfe);

	BOOST_CHECK_EQUAL(s.read<std::uint8_t>(), 0x10);
	BOOST_CHECK_EQUAL(s.read<std::uint8_t>(), 0xff);
	BOOST_CHECK_EQUAL(s.read<std::uint16_t>(), 0xf00);
	BOOST_CHECK_EQUAL(s.read_data(8), std::string((char*)memory).substr(0, 8));

	BOOST_CHECK_EQUAL(s.read<std::uint8_t>(), 0xf0);
	for (int i=0; i < 0xe; ++i) {
		BOOST_CHECK_EQUAL(s.read<std::uint8_t>(), 0);
		BOOST_CHECK_EQUAL(s.read_data(4), std::string((char*)memory).substr(0, 4));
	}
	BOOST_CHECK_EQUAL(s.read<std::uint8_t>(), 0x40);
	for (int i=0; i < 4; ++i) {
		BOOST_CHECK_EQUAL(s.read<std::uint8_t>(), 0);
		BOOST_CHECK_EQUAL(s.read_data(4), std::string((char*)memory).substr(0, 4));
	}

	BOOST_CHECK_EQUAL(s.read<std::uint8_t>(), 0);
	BOOST_CHECK_EQUAL(s.read<std::uint8_t>(), 0);
}
