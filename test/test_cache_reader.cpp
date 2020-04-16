#include <boost/test/unit_test.hpp>

#include <cstdint>

#include <cache_reader.h>
#include <cache_section_readers.h>

#include "helpers.h"

using namespace std;
using namespace reven::backend::plugins::file::libbintrace;

BOOST_AUTO_TEST_CASE(test_cache_read_header)
{
	StreamWrapper s;
	uint64_t header_size = 4;
	auto reader = s.write<uint64_t>(header_size).write<uint32_t>(4*1024).to_reader();
	auto header = read_cache_header(reader);
	BOOST_CHECK(header.page_size == 4*1024);
}

BOOST_AUTO_TEST_CASE(test_cache_read_index)
{
	StreamWrapper s;
	uint64_t cache_size = 12 * 8 + 2 * 4;

	s.write<std::uint64_t>(cache_size);
	s.write<std::uint64_t>(2);

	s.write<std::uint64_t>(20);
	s.write<std::uint64_t>(198);
	s.write<std::uint64_t>(246);
	s.write<std::uint32_t>(2);
	s.write<std::uint64_t>(0xfff000);
	s.write<std::uint64_t>(96846);
	s.write<std::uint64_t>(0xfff00fff);
	s.write<std::uint64_t>(968498);

	s.write<std::uint64_t>(30);
	s.write<std::uint64_t>(1098);
	s.write<std::uint64_t>(2046);
	s.write<std::uint32_t>(0);

	auto reader = s.to_reader();
	auto cache = read_cache_index(reader);
	BOOST_CHECK_EQUAL(cache.cache_points.size(), 2);
	BOOST_CHECK_EQUAL(cache.cache_points[20].trace_stream_offset, 198);
	BOOST_CHECK_EQUAL(cache.cache_points[20].cpu_cache_stream_offset, 246);
	BOOST_CHECK_EQUAL(cache.cache_points[20].page_offsets.size(), 2);
	BOOST_CHECK_EQUAL(cache.cache_points[20].page_offsets[0].page_address, 0xfff000);
	BOOST_CHECK_EQUAL(cache.cache_points[20].page_offsets[0].cache_stream_offset, 96846);
	BOOST_CHECK_EQUAL(cache.cache_points[20].page_offsets[1].page_address, 0xfff00fff);
	BOOST_CHECK_EQUAL(cache.cache_points[20].page_offsets[1].cache_stream_offset, 968498);
	BOOST_CHECK_EQUAL(cache.cache_points[30].trace_stream_offset, 1098);
	BOOST_CHECK_EQUAL(cache.cache_points[30].cpu_cache_stream_offset, 2046);
	BOOST_CHECK_EQUAL(cache.cache_points[30].page_offsets.size(), 0);
}

BOOST_AUTO_TEST_CASE(test_cache_reader)
{
	std::vector<std::uint8_t> buffer;
	std::string buffer_string;
	for (int i=0; i < 4*1024; ++i) {
		buffer.push_back('a' + (i % 27));
		buffer_string.push_back('a' + (i % 27));
	}

	MachineDescription desc{
		MachineDescription::Archi::x64_1,
		5,
		{ { 0, 0xff0000 }, { 0xf00000000, 10 } },
		{ { 0, { 4, "eax" } }, { 0xf00, { 8, "rax" } } },
		{},
		{},
	};

	StreamWrapper s;

	s.write<uint64_t>(4).write<uint32_t>(4*1024);

	s.write<uint64_t>(22 + 2 * 4 * 1024 + 22)
		// Packet 1
		.write<uint16_t>(2)
			.write<uint16_t>(0) // reg
			.write<uint16_t>(4)
			.write<uint32_t>(0xf0f0f0f0)
			.write<uint16_t>(0xf00) // reg
			.write<uint16_t>(8)
			.write<uint64_t>(0x0f0f0f0ff0f0f0fa)
		.write(buffer)  // mem 1
		.write(buffer)  // mem 2

		// Packet 2
		.write<uint16_t>(2)
			.write<uint16_t>(0) // reg
			.write<uint16_t>(4)
			.write<uint32_t>(0xfaf0f0fa)
			.write<uint16_t>(0xf00) // reg
			.write<uint16_t>(8)
			.write<uint64_t>(0x0f0f0f0ff0f0f0fa)
	;

	// Index
	s.write<std::uint64_t>(4+8*8+28)
		.write<std::uint64_t>(2)

		.write<std::uint64_t>(20)
		.write<std::uint64_t>(198)
		.write<std::uint64_t>(0) // cpu stream pos
		.write<std::uint32_t>(2) // memory count
		.write<std::uint64_t>(0) // address 1
		.write<std::uint64_t>(22) // stream pos
		.write<std::uint64_t>(4*1024*2) // address 2 (aligned)
		.write<std::uint64_t>(22+4*1024) // stream pos

		.write<std::uint64_t>(30)
		.write<std::uint64_t>(1098)
		.write<std::uint64_t>(22 + 2 * 4 * 1024) // index in cache points stream
		.write<std::uint32_t>(0) // memory count
	;

	auto reader = CacheReader(s.to_stream_with_cache_metadata(), desc);
	RegisterContainer cpu;

	BOOST_CHECK(reader.find_closest(0) == reader.none());
	BOOST_CHECK(reader.find_closest(20) == reader.none());
	auto cache_point = reader.find_closest(21);
	BOOST_CHECK(cache_point != reader.none());
	BOOST_CHECK(cache_point->first == 20);
	cpu = reader.read_cache_point(cache_point);
	BOOST_CHECK_EQUAL(cpu.size(), 2);
	BOOST_CHECK_EQUAL(*reinterpret_cast<std::uint32_t*>(cpu.at(0).second.data()), 0xf0f0f0f0);
	BOOST_CHECK_EQUAL(s.s.str().substr(16 + 4 + cache_point->second.page_offsets[0].cache_stream_offset, 4 * 1024),
	                  buffer_string);

	cache_point = reader.find_closest(60);
	BOOST_CHECK(cache_point->first == 30);
	cpu = reader.read_cache_point(cache_point);
	BOOST_CHECK_EQUAL(cpu.size(), 2);
	BOOST_CHECK_EQUAL(*reinterpret_cast<std::uint32_t*>(cpu.at(0).second.data()), 0xfaf0f0fa);
	BOOST_CHECK_EQUAL(*reinterpret_cast<std::uint64_t*>(cpu.at(1).second.data()), 0x0f0f0f0ff0f0f0fa);

	cache_point = reader.find_closest(30);
	BOOST_CHECK(cache_point->first == 20);
	cpu = reader.read_cache_point(cache_point);
	BOOST_CHECK_EQUAL(cpu.size(), 2);
	BOOST_CHECK_EQUAL(*reinterpret_cast<std::uint32_t*>(cpu.at(0).second.data()), 0xf0f0f0f0);

	auto md = reader.metadata();

	BOOST_CHECK(md.type() == reven::metadata::ResourceType::TraceCache);
	BOOST_CHECK(md.format_version().to_string() == TestMDWriter::format_version);
	BOOST_CHECK(md.tool_name() == TestMDWriter::tool_name);
	BOOST_CHECK(md.tool_version().to_string() == TestMDWriter::tool_version);
	BOOST_CHECK(md.tool_info() == TestMDWriter::tool_info);
	BOOST_CHECK(md.generation_date() == std::chrono::system_clock::time_point(std::chrono::seconds(TestMDWriter::generation_date)));
}

BOOST_AUTO_TEST_CASE(test_incompatible_type_cache)
{
	StreamWrapper s;
	s.write<uint32_t>(0x46434252);

	MachineDescription desc;

	BOOST_CHECK_THROW(CacheReader(s.reset().to_stream_with_bin_metadata("1.0.0"), desc), IncompatibleTypeException);
}

BOOST_AUTO_TEST_CASE(test_incompatible_version_cache)
{
	StreamWrapper s;
	s.write<uint32_t>(0x46434252);

	MachineDescription desc;

	BOOST_CHECK_THROW(CacheReader(s.reset().to_stream_with_cache_metadata("0.0.0"), desc), IncompatibleVersionException);
	BOOST_CHECK_THROW(CacheReader(s.reset().to_stream_with_cache_metadata("2.0.0"), desc), IncompatibleVersionException);
}
