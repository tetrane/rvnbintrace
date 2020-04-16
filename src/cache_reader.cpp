#include <cache_reader.h>

#include <cstdint>
#include <algorithm>

#include <common.h>
#include <cache_section_readers.h>
#include <reader_errors.h>

namespace reven {
namespace backend {
namespace plugins {
namespace file {
namespace libbintrace {

CacheReader::CacheReader(std::unique_ptr<std::istream>&& input_stream, const MachineDescription& machine)
	: reader_(binresource::Reader::open(std::move(input_stream)))
	, cache_points_reader_(nullptr), machine_(machine)
{
	if (not reader_.stream())
		throw UnexpectedEndOfStream("magic");

	if (metadata().type() != metadata::ResourceType::TraceCache) {
		throw IncompatibleTypeException("Can't open a resource of type different from TraceCache");
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

	header_ = read_cache_header(reader_);

	auto restore_pos = reader_.stream().tellg();
	SectionReader skip_cache_points_section("cache points skip", reader_);
	skip_cache_points_section.seek_to_end();
	index_ = read_cache_index(reader_);
	reader_.stream().seekg(restore_pos);

	cache_points_reader_ = std::make_unique<SectionReader>("cache points", reader_);
}

CacheReader::ConstIterator CacheReader::find_closest(std::uint64_t context_id) const
{
	// Do not return the cache point if context_id is an exact match, because the caller does require executing at least
	// one instruction to get the events right.
	return index_.cache_points.upper_bound(context_id);
}

RegisterContainer CacheReader::read_cache_point(ConstIterator it)
{
	RegisterContainer result;

	cache_points_reader_->seek(it->second.cpu_cache_stream_offset);
	auto reg_count = cache_points_reader_->read<std::uint16_t>();

	if (machine_.registers.size() != reg_count)
		throw MalformedSection(cache_points_reader_->name(), "cache point does not contain enough registers");

	for (; reg_count > 0; --reg_count) {
		auto reg_id = cache_points_reader_->read<std::uint16_t>();
		auto size = cache_points_reader_->read<std::uint16_t>();

		auto found = std::find_if(result.begin(), result.end(),
		                          [reg_id](const RegisterContainer::value_type& val) { return val.first == reg_id; });
		if (found != result.end())
			throw MalformedSection(cache_points_reader_->name(), std::string("Register ") + std::to_string(reg_id) +
			                                                 " contained twice in cache point");
		auto reg = machine_.registers.find(reg_id);
		if (reg == machine_.registers.end())
			throw MalformedSection(cache_points_reader_->name(), std::to_string(reg_id) + " is an unknown register id");
		if (reg->second.size != size)
			throw MalformedSection(cache_points_reader_->name(), std::to_string(reg_id) + "'s size " + std::to_string(size) +
			                                                 " doesn't match declared " +
			                                                 std::to_string(reg->second.size));

		result.emplace_back(reg_id, size);
		cache_points_reader_->read(result.back().second.data(), size);
	}

	return result;
}

metadata::Version CacheReader::resource_version()
{
	return metadata::Version::from_string(format_version);
}

metadata::ResourceType CacheReader::resource_type()
{
	return reven::metadata::ResourceType::TraceCache;
}

}}}}}
