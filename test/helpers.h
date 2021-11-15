#pragma once

#include <iostream>
#include <iomanip>
#include <vector>

#include <rvnmetadata/metadata-common.h>
#include <rvnbinresource/writer.h>
#include <rvnbinresource/reader.h>

using namespace std;

class TestMDWriter : reven::binresource::MetadataWriter {
public:
	static constexpr const char* format_version = "1.0.0-dummy";
	static constexpr const char* tool_name = "TestMetaDataWriter";
	static constexpr const char* tool_version = "1.0.0";
	static constexpr const char* tool_info = "Tests version 1.0.0";
	static constexpr std::uint64_t generation_date = 42424242;

	static reven::binresource::Metadata dummy_bin_md(const char* format_version = TestMDWriter::format_version) {
		return write(
			static_cast<std::uint32_t>(reven::metadata::ResourceType::TraceBin),
			format_version,
			tool_name,
			tool_version,
			tool_info,
			generation_date
		);
	}

	static reven::binresource::Metadata dummy_cache_md(const char* format_version = TestMDWriter::format_version) {
		return write(
			static_cast<std::uint32_t>(reven::metadata::ResourceType::TraceCache),
			format_version,
			tool_name,
			tool_version,
			tool_info,
			generation_date
		);
	}
};

struct StreamWrapper {
	StreamWrapper& reset()
	{
		s.clear();
		s.seekg(0);
		s.seekp(0);
		return *this;
	}

	template <typename T>
	StreamWrapper& write(const T& value, std::size_t max = 0)
	{
		if (max == 0)
			max = sizeof(T);
		s.write(reinterpret_cast<const char*>(&value), std::min(max, sizeof(value)));
		return *this;
	}

	StreamWrapper& write(const vector<unsigned char> value)
	{
		s.write(reinterpret_cast<const char*>(value.data()), value.size());
		return *this;
	}

	template <typename T>
	T read(std::size_t max = 0)
	{
		if (max == 0)
			max = sizeof(T);
		T value = 0;
		s.read(reinterpret_cast<char*>(&value), std::min(max, sizeof(value)));
		return value;
	}

	std::string read_data(std::size_t count)
	{
		std::string v;
		for (; count > 0; --count)
			v.push_back(read<char>());
		return v;
	}

	operator stringstream&() {
		return s;
	}

	void dump_stream()
	{
		auto restore = s.tellg();
		auto string = s.str();
		s.seekg(restore);

		cout << "full dump - size is " << string.size() << ", currently at " << restore << endl;
		int i = 1;
		for (auto c : string) {
			if (restore == i - 1)
				cout << ">";
			else
				cout << " ";

			cout << hex << std::setw(2) << setfill('0') << (+c & 0xff);

			if (i % 16 == 0)
				cout << endl;
			i++;
		}
		if (restore == i - 1)
			cout << ">";
		cout << endl;
	}

	void skip_section()
	{
		auto pos = read<std::uint64_t>();
		BOOST_CHECK(pos != 0);
		s.seekg(pos, std::ios_base::cur);
	}

	std::unique_ptr<stringstream> to_stream_with_bin_metadata(const char* format_version = TestMDWriter::format_version) {
		auto writer = reven::binresource::Writer::create(make_unique<stringstream>(), TestMDWriter::dummy_bin_md(format_version));
		auto stream = std::unique_ptr<stringstream>(static_cast<stringstream*>(std::move(writer).finalize().release()));

		*stream << s.rdbuf();

		return stream;
	}

	std::unique_ptr<stringstream> to_stream_with_cache_metadata(const char* format_version = TestMDWriter::format_version) {
		auto writer = reven::binresource::Writer::create(make_unique<stringstream>(), TestMDWriter::dummy_cache_md(format_version));
		auto stream = std::unique_ptr<stringstream>(static_cast<stringstream*>(std::move(writer).finalize().release()));

		*stream << s.rdbuf();

		return stream;
	}

	reven::binresource::Reader to_reader(const char* format_version = TestMDWriter::format_version) {
		return reven::binresource::Reader::open(to_stream_with_bin_metadata(format_version));
	}

	stringstream s;
};

struct WriterWrapper {
	WriterWrapper() {
		reset();
	}

	StreamWrapper stream() {
		return StreamWrapper{
			stringstream(static_cast<stringstream*>(&writer->stream())->str().substr(writer->md_size()))
		};
	}

	operator reven::binresource::Writer*() {
		return writer.get();
	}

	operator reven::binresource::Writer&() {
		return *writer;
	}

	operator std::unique_ptr<reven::binresource::Writer>&() {
		return writer;
	}

	WriterWrapper& reset() {
		writer = std::make_unique<reven::binresource::Writer>(
			reven::binresource::Writer::create(make_unique<stringstream>(), TestMDWriter::dummy_bin_md())
		);

		return *this;
	}

	std::unique_ptr<reven::binresource::Writer> writer;
};
