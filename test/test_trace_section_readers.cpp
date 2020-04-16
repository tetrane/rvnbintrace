#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <reader_errors.h>
#include <trace_section_readers.h>
#include <section_reader.h>

#include "helpers.h"

using namespace std;
using namespace reven::backend::plugins::file::libbintrace;

BOOST_AUTO_TEST_CASE(test_section_reader_helper)
{
	StreamWrapper s;
	auto reader = s.reset().write({0, 0, 0, 0, 0, 0, 0}).to_reader();
	BOOST_CHECK_THROW(SectionReader("", reader), UnexpectedEndOfStream);

	reader = s.reset().write<uint64_t>(0).to_reader();
	BOOST_CHECK(not SectionReader("", reader).bytes_left());

	reader = s.reset().write<uint64_t>(1).to_reader();
	BOOST_CHECK(SectionReader("", reader).bytes_left());

	reader = s.reset().write<uint64_t>(16)
		.write<uint32_t>(0).write<uint32_t>(1).write<uint32_t>(2).write<uint32_t>(3).to_reader();

	SectionReader section("", reader);
	BOOST_CHECK_EQUAL(section.stream_pos(), 0);
	BOOST_CHECK_EQUAL(section.read<uint32_t>(), 0);
	BOOST_CHECK_EQUAL(section.read<uint32_t>(), 1);
	BOOST_CHECK_EQUAL(section.read<uint32_t>(), 2);
	BOOST_CHECK_EQUAL(section.stream_pos(), 12);
	section.seek(0);
	BOOST_CHECK_EQUAL(section.stream_pos(), 0);
	BOOST_CHECK_EQUAL(section.read<uint32_t>(), 0);
	section.seek(8);
	BOOST_CHECK_EQUAL(section.stream_pos(), 8);
	BOOST_CHECK_EQUAL(section.read<uint32_t>(), 2);
	BOOST_CHECK_EQUAL(section.read<uint32_t>(), 3);
}

BOOST_AUTO_TEST_CASE(test_header_reader)
{
	StreamWrapper s;
	uint64_t header_size = 1;

	auto reader = s.write<uint64_t>(header_size)
		.write<uint8_t>(0)
		.to_reader();
	auto header = read_trace_header(reader);

	BOOST_CHECK(header.compression == 0);

	reader = s.reset().write<uint64_t>(header_size)
		.write<uint8_t>(1)
		.to_reader();

	BOOST_CHECK_THROW(read_trace_header(reader),
		              UnsupportedFeature);
}

BOOST_AUTO_TEST_CASE(test_machine_description_reader)
{
	StreamWrapper s;
	auto reader = s.reset()
		.write<uint64_t>(0xff)
		.write({ 'x', '6', '4', '2' })
		.to_reader();
	BOOST_CHECK_THROW(read_trace_machine_description(reader), UnsupportedFeature);

	reader = s.reset()
		.write<uint64_t>(0xff)
		.write({ 'x', '6', '4', '1' })
		.write<uint8_t>(9)
		.to_reader();
	BOOST_CHECK_THROW(read_trace_machine_description(reader), MalformedSection);

	reader = s.reset()
		.write<uint64_t>(0xff)
		.write({ 'x', '6', '4', '1' })
		.write<uint8_t>(0)
		.to_reader();
	BOOST_CHECK_THROW(read_trace_machine_description(reader), MalformedSection);

	reader = s.reset()
		.write<uint64_t>(0xff).write({ 'x', '6', '4', '1' })
		.write<uint8_t>(8)
		.write<std::uint32_t>(0)
		.write<std::uint32_t>(0)
		.write<std::uint32_t>(0)
		.write<std::uint32_t>(0)
		.to_reader();
	BOOST_CHECK_NO_THROW(read_trace_machine_description(reader));

	reader = s.reset()
		.write<uint64_t>(0xff).write({ 'x', '6', '4', '1' }).write<uint8_t>(8)
		.write<std::uint32_t>(0)
		.write<std::uint32_t>(2)
			.write<std::uint16_t>(0).write<std::uint16_t>(4).write<std::uint8_t>(3).write({ 'e', 'a', 'x' })
			.write<std::uint16_t>(0).write<std::uint16_t>(4).write<std::uint8_t>(3).write({ 'e', 'b', 'x' })
		.to_reader();
	BOOST_CHECK_THROW(read_trace_machine_description(reader) ,MalformedSection);

	reader = s.reset()
		.write<uint64_t>(0xff).write({ 'x', '6', '4', '1' }).write<uint8_t>(8)
		.write<std::uint32_t>(0)
		.write<std::uint32_t>(2)
			.write<std::uint16_t>(0).write<std::uint16_t>(4).write<std::uint8_t>(3).write({ 'e', 'a', 'x' })
			.write<std::uint16_t>(1).write<std::uint16_t>(4).write<std::uint8_t>(3).write({ 'e', 'a', 'x' })
		.to_reader();
	BOOST_CHECK_THROW(read_trace_machine_description(reader) ,MalformedSection);

	reader = s.reset()
		.write<uint64_t>(0xff).write({ 'x', '6', '4', '1' }).write<uint8_t>(8)
		.write<std::uint32_t>(0)
		.write<std::uint32_t>(1)
			.write<std::uint16_t>(0).write<std::uint16_t>(4).write<std::uint8_t>(3).write({ 'e', 'a', 'x' })
		.write<std::uint32_t>(1)
			.write<std::uint16_t>(0).write<std::uint16_t>(0).write<std::uint8_t>(0).write<std::uint32_t>(0)
		.to_reader();
	BOOST_CHECK_THROW(read_trace_machine_description(reader),MalformedSection);

	reader = s.reset()
		.write<uint64_t>(0xff).write({ 'x', '6', '4', '1' }).write<uint8_t>(8)
		.write<std::uint32_t>(0)
		.write<std::uint32_t>(1)
			.write<std::uint16_t>(0).write<std::uint16_t>(4).write<std::uint8_t>(3).write({ 'e', 'a', 'x' })
		.write<std::uint32_t>(1)
			.write<std::uint16_t>(1).write<std::uint16_t>(1).write<std::uint8_t>(0).write<std::uint32_t>(0)
		.to_reader();
	BOOST_CHECK_THROW(read_trace_machine_description(reader),MalformedSection);

	reader = s.reset()
		.write<uint64_t>(0xff).write({ 'x', '6', '4', '1' }).write<uint8_t>(8)
		.write<std::uint32_t>(0)
		.write<std::uint32_t>(1)
			.write<std::uint16_t>(0).write<std::uint16_t>(4).write<std::uint8_t>(3).write({ 'e', 'a', 'x' })
		.write<std::uint32_t>(1)
			.write<std::uint16_t>(0xff).write<std::uint16_t>(0).write<std::uint8_t>(0).write<std::uint32_t>(0)
		.to_reader();
	BOOST_CHECK_THROW(read_trace_machine_description(reader),MalformedSection);

	reader = s.reset().write<uint64_t>(0xffff).write({ 'x', '6', '4', '1' }).write<std::uint8_t>(6)
		.write<std::uint32_t>(2) // memory regions
			.write<std::uint64_t>(0, 6).write<std::uint64_t>(0xff000000, 6)
			.write<std::uint64_t>(0xffff0000, 6).write<std::uint64_t>(0x000000ff, 6)
		.write<std::uint32_t>(3) // registers
			.write<std::uint16_t>(0).write<std::uint16_t>(4).write<std::uint8_t>(3).write({ 'e', 'a', 'x' })
			.write<std::uint16_t>(1).write<std::uint16_t>(8).write<std::uint8_t>(3).write({ 'r', 'a', 'x' })
			.write<std::uint16_t>(2).write<std::uint16_t>(4).write<std::uint8_t>(4).write({ 'p', 'k', 'r', 'u' })
		.write<std::uint32_t>(2) // actions
			.write<std::uint8_t>(0xfe).write<std::uint16_t>(0).write<std::uint8_t>(0).write<std::uint32_t>(0)
			.write<std::uint8_t>(0xfd).write<std::uint16_t>(0).write<std::uint8_t>(1).write<std::uint32_t>(1)
		.write<std::uint32_t>(2) // static register
		    .write<std::uint8_t>(3).write({ 'c', 'p', 'u' }).write<std::uint8_t>(2).write<std::uint16_t>(0x42)
		    .write<std::uint8_t>(3).write({ 'p', 's', 'e' }).write<std::uint8_t>(4).write<std::uint32_t>(0xffffffff)
		.to_reader()
	;

	auto desc = read_trace_machine_description(reader);
	BOOST_CHECK_EQUAL(desc.physical_address_size, 6);
	BOOST_CHECK_EQUAL(desc.memory_regions.size(), 2);
	BOOST_CHECK_EQUAL(desc.memory_regions[0].start, 0);
	BOOST_CHECK_EQUAL(desc.memory_regions[0].size, 0xff000000);
	BOOST_CHECK_EQUAL(desc.memory_regions[1].start, 0xffff0000);
	BOOST_CHECK_EQUAL(desc.memory_regions[1].size, 0xff);
	BOOST_CHECK_EQUAL(desc.total_physical_size(), 0xff0000ff);
	BOOST_CHECK_EQUAL(desc.registers.size(), 3);
	BOOST_CHECK_EQUAL(desc.registers[0].size, 4);
	BOOST_CHECK_EQUAL(desc.registers[0].name, "eax");
	BOOST_CHECK_EQUAL(desc.registers[1].size, 8);
	BOOST_CHECK_EQUAL(desc.registers[1].name, "rax");
	BOOST_CHECK_EQUAL(desc.registers[2].size, 4);
	BOOST_CHECK_EQUAL(desc.registers[2].name, "pkru");
	BOOST_CHECK_EQUAL(desc.register_operations.size(), 2);
	BOOST_CHECK_EQUAL(desc.register_operations[0xfe].register_id, 0);
	BOOST_CHECK(desc.register_operations[0xfe].operation == MachineDescription::RegisterOperator::Set);
	BOOST_CHECK_EQUAL(desc.register_operations[0xfe].value[0], 0);
	BOOST_CHECK_EQUAL(desc.register_operations[0xfd].register_id, 0);
	BOOST_CHECK(desc.register_operations[0xfd].operation == MachineDescription::RegisterOperator::Add);
	BOOST_CHECK_EQUAL(desc.static_registers["cpu"][0], 0x42);
	BOOST_CHECK_EQUAL(desc.static_registers["cpu"][1], 0x00);
	BOOST_CHECK_EQUAL(static_cast<std::uint8_t>(desc.static_registers["pse"][0]), 0xff);
	BOOST_CHECK_EQUAL(static_cast<std::uint8_t>(desc.static_registers["pse"][3]), 0xff);
}
