#include <boost/test/unit_test.hpp>

#include <cstdint>

#include <cache_writer.h>

#include "helpers.h"

using namespace std;
using namespace reven::backend::plugins::file::libbintrace;

BOOST_AUTO_TEST_CASE(test_cache_write_header)
{
	WriterWrapper w;

	write_cache_header(w, CacheHeader{ 4*1024 });

	auto s = w.stream();

	s.read<uint64_t>();
	BOOST_CHECK_EQUAL(s.read<uint32_t>(), 4*1024);
}

BOOST_AUTO_TEST_CASE(test_cache_write_index)
{
	WriterWrapper w;

	write_cache_index(w, CacheIndex { {
		{ 42, { 65, 0xff00000000, { { 0xff00, 65 }, { 0xff00dd, 89 } } } },
		{ 44, { 72, 0xffdd00000000, {} } },
	} });

	auto s = w.stream();

	s.read<uint64_t>();
	BOOST_CHECK_EQUAL(s.read<uint64_t>(), 2); // count

	BOOST_CHECK_EQUAL(s.read<uint64_t>(), 44);
	BOOST_CHECK_EQUAL(s.read<uint64_t>(), 72);
	BOOST_CHECK_EQUAL(s.read<uint64_t>(), 0xffdd00000000);
	BOOST_CHECK_EQUAL(s.read<uint32_t>(), 0);

	BOOST_CHECK_EQUAL(s.read<uint64_t>(), 42);
	BOOST_CHECK_EQUAL(s.read<uint64_t>(), 65);
	BOOST_CHECK_EQUAL(s.read<uint64_t>(), 0xff00000000);
	BOOST_CHECK_EQUAL(s.read<uint32_t>(), 2);
	BOOST_CHECK_EQUAL(s.read<uint64_t>(), 0xff00);
	BOOST_CHECK_EQUAL(s.read<uint64_t>(), 65);
	BOOST_CHECK_EQUAL(s.read<uint64_t>(), 0xff00dd);
	BOOST_CHECK_EQUAL(s.read<uint64_t>(), 89);
}

class CacheWriterTester : public CacheWriter
{
public:
	CacheWriterTester(const MachineDescription& desc)
	  : CacheWriter(make_unique<stringstream>(), 4*1024, desc,
	                "TestTraceWriter", "1.0.0", "Tests version 1.0.0") {}

	StreamWrapper stream() {
		return StreamWrapper{
			stringstream(static_cast<stringstream*>(&writer_.stream())->str().substr(writer_.md_size()))
		};
	}
};

BOOST_AUTO_TEST_CASE(test_cache_writer)
{
	MachineDescription desc {
		MachineDescription::Archi::x64_1,
		5,
		{ {0, 0xff0000}, {0xf00000000, 10} },
		{ {0, {4, "eax"}}, {1, {4, "ebx"}}, {0xf00, {8, "rax"}} },
		{},
		{},
	};

	std::string data("01234567");
	data.resize(4*1024, '\x42');
	const std::uint8_t *buffer = reinterpret_cast<const std::uint8_t*>(data.data());

	CacheWriterTester cache(desc);
	auto cache_points_writer = cache.start_cache_points_section();

	cache_points_writer.start_cache_point(10, 165);
	cache_points_writer.write_register(0, buffer, 4);
	cache_points_writer.write_register(1, buffer+4, 4);
	cache_points_writer.write_register(0xf00, buffer, 8);
	cache_points_writer.finish_cache_point();

	cache_points_writer.start_cache_point(27, 1061);
	cache_points_writer.write_register(0, buffer, 4);
	cache_points_writer.write_memory_page(0, buffer);
	BOOST_CHECK_THROW(cache_points_writer.write_memory_page(1, buffer), NonsenseValue);
	BOOST_CHECK_THROW(cache_points_writer.write_register(0xf00, buffer, 0), std::logic_error);
	cache_points_writer.finish_cache_point();

	BOOST_CHECK_THROW(cache_points_writer.start_cache_point(27, 1061), NonsenseValue);

	cache.finish_cache_points_section(std::move(cache_points_writer));
	auto s = cache.stream();

	s.skip_section(); // skip header, already tested

	s.read<uint64_t>();
	BOOST_CHECK_EQUAL(s.read<uint16_t>(), 3);
	BOOST_CHECK_EQUAL(s.read<uint16_t>(), 0);
	BOOST_CHECK_EQUAL(s.read<uint16_t>(), 4);
	BOOST_CHECK_EQUAL(s.read_data(4), data.substr(0, 4));
	BOOST_CHECK_EQUAL(s.read<uint16_t>(), 1);
	BOOST_CHECK_EQUAL(s.read<uint16_t>(), 4);
	BOOST_CHECK_EQUAL(s.read_data(4), data.substr(4, 4));
	BOOST_CHECK_EQUAL(s.read<uint16_t>(), 0xf00);
	BOOST_CHECK_EQUAL(s.read<uint16_t>(), 8);
	BOOST_CHECK_EQUAL(s.read_data(8), data.substr(0, 8));

	BOOST_CHECK_EQUAL(s.read<uint16_t>(), 1);
	s.read_data(2+2+4); // ignore register
	BOOST_CHECK_EQUAL(s.read_data(4*1024), data.substr(0, 4*1024));

	s.read<uint64_t>(); // section index, already tested but still
	BOOST_CHECK_EQUAL(s.read<uint64_t>(), 2); // Cache point number
	BOOST_CHECK_EQUAL(s.read<uint64_t>(), 27);
	BOOST_CHECK_EQUAL(s.read<uint64_t>(), 1061);
	auto cpu_offset = s.read<uint64_t>(); // cpu stream offset, irrelevant
	BOOST_CHECK_EQUAL(s.read<uint32_t>(), 1); // page count
	BOOST_CHECK_EQUAL(s.read<uint64_t>(), 0);  // page 1 address
	BOOST_CHECK_EQUAL(s.read<uint64_t>(), cpu_offset + 2 + 8); // page 1 offset
	BOOST_CHECK_EQUAL(s.read<uint64_t>(), 10);
	BOOST_CHECK_EQUAL(s.read<uint64_t>(), 165);
}
