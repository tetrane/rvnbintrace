#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <writer_errors.h>
#include <trace_section_writers.h>
#include <section_writer.h>

#include "helpers.h"

using namespace std;
using namespace reven::backend::plugins::file::libbintrace;

BOOST_AUTO_TEST_CASE(test_section_writer_helper)
{
	WriterWrapper w;

	SectionWriter sw("", w);
	sw.write<uint16_t>(0xf0f0);
	sw.write_string<uint8_t>("a string");
	sw.finalize();

	auto s = w.stream();

	BOOST_CHECK_EQUAL(s.read<uint64_t>(), 11);
	BOOST_CHECK_EQUAL(s.read<uint16_t>(), 0xf0f0);
	auto size = s.read<uint8_t>();
	BOOST_CHECK_EQUAL(size, 8);
	BOOST_CHECK_EQUAL(s.read_data(size), "a string");

	std::string too_big;
	for (int i=0; i < 2000; ++i)
		too_big.push_back(' ');

	BOOST_CHECK_THROW(sw.write_string<std::uint8_t>(too_big), ValueTooBig);
	BOOST_CHECK_THROW(sw.write_sized_buffer<std::uint8_t>(nullptr, 2000), ValueTooBig);
	BOOST_CHECK_THROW(sw.write<std::uint8_t>(0x100u), ValueTooBig);
	BOOST_CHECK_THROW(sw.write<std::uint32_t>(0xff000001u, 3), ValueTooBig);
}

BOOST_AUTO_TEST_CASE(test_section_writers)
{
	WriterWrapper w;

	write_trace_header(w, Header{ 0u });

	auto s = w.stream();

	s.read<uint64_t>();
	BOOST_CHECK_EQUAL(s.read<uint8_t>(), 0u);

	MachineDescription desc{
		MachineDescription::Archi::x64_1,
		5,
		{ { 0, 0xff0000 }, { 0xf00000000, 10 } },
		{ { 0, { 4, "eax" } }, { 1, { 4, "ebx" } }, { 0xf00, { 8, "rax" } } },
		{ { 0xfe, { 0, MachineDescription::RegisterOperator::Or, { '0', '0', '0', '0' } } } },
		{ { "cpuid", { 't', 'o', 't', 'o' } }, { "msr", { 'n', 'o', 's', 't', 'a', 't', 'i', 'c' } } },
	};

	w.reset();

	write_trace_machine_description(w, desc);

	s = w.stream();

	s.read<uint64_t>();
	BOOST_CHECK_EQUAL(s.read<uint32_t>(), 0x31343678);
	BOOST_CHECK_EQUAL(s.read<uint8_t>(), desc.physical_address_size);

	BOOST_CHECK_EQUAL(s.read<uint32_t>(), 2);
	BOOST_CHECK_EQUAL(s.read<uint64_t>(desc.physical_address_size), 0);
	BOOST_CHECK_EQUAL(s.read<uint64_t>(desc.physical_address_size), 0xff0000u);
	BOOST_CHECK_EQUAL(s.read<uint64_t>(desc.physical_address_size), 0xf00000000u);
	BOOST_CHECK_EQUAL(s.read<uint64_t>(desc.physical_address_size), 10);

	BOOST_CHECK_EQUAL(s.read<uint32_t>(), 3);
	BOOST_CHECK_EQUAL(s.read<uint16_t>(), 0);
	BOOST_CHECK_EQUAL(s.read<uint16_t>(), 4);
	BOOST_CHECK_EQUAL(s.read<uint8_t>(), 3);
	BOOST_CHECK_EQUAL(s.read_data(3), "eax");
	BOOST_CHECK_EQUAL(s.read<uint16_t>(), 1);
	BOOST_CHECK_EQUAL(s.read<uint16_t>(), 4);
	BOOST_CHECK_EQUAL(s.read<uint8_t>(), 3);
	BOOST_CHECK_EQUAL(s.read_data(3), "ebx");
	BOOST_CHECK_EQUAL(s.read<uint16_t>(), 0xf00);
	BOOST_CHECK_EQUAL(s.read<uint16_t>(), 8);
	BOOST_CHECK_EQUAL(s.read<uint8_t>(), 3);
	BOOST_CHECK_EQUAL(s.read_data(3), "rax");

	BOOST_CHECK_EQUAL(s.read<uint32_t>(), 1);
	BOOST_CHECK_EQUAL(s.read<uint8_t>(), 0xfe);
	BOOST_CHECK_EQUAL(s.read<uint16_t>(), 0);
	BOOST_CHECK_EQUAL(s.read<uint8_t>(), 3);
	BOOST_CHECK_EQUAL(s.read_data(4), "0000");

	BOOST_CHECK_EQUAL(s.read<uint32_t>(), 2);
	BOOST_CHECK_EQUAL(s.read<uint8_t>(), 5);
	BOOST_CHECK_EQUAL(s.read_data(5), "cpuid");
	BOOST_CHECK_EQUAL(s.read<uint8_t>(), 4);
	BOOST_CHECK_EQUAL(s.read_data(4), "toto");
	BOOST_CHECK_EQUAL(s.read<uint8_t>(), 3);
	BOOST_CHECK_EQUAL(s.read_data(3), "msr");
	BOOST_CHECK_EQUAL(s.read<uint8_t>(), 8);
	BOOST_CHECK_EQUAL(s.read_data(8), "nostatic");

	MachineDescription other = desc;
	other.physical_address_size = 9;
	BOOST_CHECK_THROW(write_trace_machine_description(w, other), NonsenseValue);
	other = desc;
	other.physical_address_size = 0;
	BOOST_CHECK_THROW(write_trace_machine_description(w, other), NonsenseValue);
	other = desc;
	other.register_operations[0xff] = {0, MachineDescription::RegisterOperator::Set, {'0', '0', '0', '0'}}; // too big
	BOOST_CHECK_THROW(write_trace_machine_description(w, other), NonsenseValue);
	other = desc;
	other.register_operations[0] = {0, MachineDescription::RegisterOperator::Set, {'0', '0', '0', '0'}}; // overlap with register
	BOOST_CHECK_THROW(write_trace_machine_description(w, other), NonsenseValue);
	other = desc;
	other.register_operations[0xff] = {0xdd, MachineDescription::RegisterOperator::Set, {'0', '0'}}; // size too small
	BOOST_CHECK_THROW(write_trace_machine_description(w, other), NonsenseValue);
}
