#include "WadFile.h"

#include "imapformat.h"
#include "i18n.h"
#include "string/case_conv.h"

#include <array>
#include <fmt/format.h>

namespace map
{

namespace doom
{

namespace
{

const std::size_t HEADER_SIZE = 12;
const std::size_t DIRECTORY_ENTRY_SIZE = 16;
const std::size_t MAX_LUMPS = 65536;

int32_t readInt32(std::istream& stream)
{
	std::array<unsigned char, 4> bytes;
	stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());

	return static_cast<int32_t>(
		static_cast<uint32_t>(bytes[0]) |
		(static_cast<uint32_t>(bytes[1]) << 8) |
		(static_cast<uint32_t>(bytes[2]) << 16) |
		(static_cast<uint32_t>(bytes[3]) << 24));
}

std::string readLumpName(std::istream& stream)
{
	std::array<char, 8> name;
	stream.read(name.data(), name.size());

	std::size_t length = 0;
	while (length < name.size() && name[length] != '\0')
	{
		length++;
	}

	return string::to_upper_copy(std::string(name.data(), length));
}

bool isDoomBinaryLump(const std::string& name)
{
	return name == "THINGS" || name == "LINEDEFS" || name == "SIDEDEFS" ||
		name == "VERTEXES" || name == "SEGS" || name == "SSECTORS" ||
		name == "NODES" || name == "SECTORS" || name == "REJECT" ||
		name == "BLOCKMAP" || name == "BEHAVIOR" || name == "SCRIPTS";
}

}

WadFile::WadFile(std::istream& stream) :
	_stream(stream)
{
	readDirectory();
	collectMaps();
}

bool WadFile::hasWadSignature(std::istream& stream)
{
	std::array<char, 4> magic{};

	stream.seekg(0, std::ios_base::beg);
	stream.read(magic.data(), magic.size());

	if (!stream.good())
	{
		return false;
	}

	return (magic[0] == 'I' || magic[0] == 'P') &&
		magic[1] == 'W' && magic[2] == 'A' && magic[3] == 'D';
}

void WadFile::readDirectory()
{
	_stream.seekg(0, std::ios_base::end);
	auto fileSize = static_cast<std::size_t>(_stream.tellg());

	if (fileSize < HEADER_SIZE)
	{
		throw IMapReader::FailureException(_("File is too small to be a WAD"));
	}

	_stream.seekg(0, std::ios_base::beg);

	std::array<char, 4> magic{};
	_stream.read(magic.data(), magic.size());

	if ((magic[0] != 'I' && magic[0] != 'P') ||
		magic[1] != 'W' || magic[2] != 'A' || magic[3] != 'D')
	{
		throw IMapReader::FailureException(_("Not a WAD file, missing IWAD/PWAD signature"));
	}

	auto lumpCount = readInt32(_stream);
	auto directoryOffset = readInt32(_stream);

	if (lumpCount < 0 || static_cast<std::size_t>(lumpCount) > MAX_LUMPS)
	{
		throw IMapReader::FailureException(fmt::format(_("WAD declares an implausible lump count: {0:d}"), lumpCount));
	}

	if (directoryOffset < 0 ||
		static_cast<std::size_t>(directoryOffset) + static_cast<std::size_t>(lumpCount) * DIRECTORY_ENTRY_SIZE > fileSize)
	{
		throw IMapReader::FailureException(_("WAD directory is located outside the file"));
	}

	_stream.seekg(directoryOffset, std::ios_base::beg);

	_lumps.reserve(static_cast<std::size_t>(lumpCount));

	for (int32_t i = 0; i < lumpCount; ++i)
	{
		auto offset = readInt32(_stream);
		auto size = readInt32(_stream);
		auto name = readLumpName(_stream);

		if (!_stream.good())
		{
			throw IMapReader::FailureException(_("Unexpected end of file while reading the WAD directory"));
		}

		if (offset < 0 || size < 0 || static_cast<std::size_t>(offset) + static_cast<std::size_t>(size) > fileSize)
		{
			offset = 0;
			size = 0;
		}

		_lumps.push_back(WadLump{ name, static_cast<std::size_t>(offset), static_cast<std::size_t>(size) });
	}
}

void WadFile::collectMaps()
{
	for (std::size_t i = 0; i + 1 < _lumps.size(); ++i)
	{
		const auto& next = _lumps[i + 1];

		MapLumpKind kind;

		if (next.name == "TEXTMAP")
		{
			kind = MapLumpKind::Udmf;
		}
		else if (next.name == "THINGS")
		{
			kind = MapLumpKind::DoomBinary;
		}
		else
		{
			continue;
		}

		WadMapGroup group;
		group.name = _lumps[i].name;
		group.kind = kind;
		group.firstLump = i + 1;
		group.lastLump = i + 1;

		for (std::size_t j = i + 1; j < _lumps.size(); ++j)
		{
			const auto& lump = _lumps[j];

			if (kind == MapLumpKind::Udmf)
			{
				if (lump.name == "ENDMAP")
				{
					break;
				}

				group.lastLump = j;
			}
			else
			{
				if (!isDoomBinaryLump(lump.name))
				{
					break;
				}

				if (lump.name == "BEHAVIOR")
				{
					group.kind = MapLumpKind::HexenBinary;
				}

				group.lastLump = j;
			}
		}

		_maps.push_back(group);

		i = group.lastLump;
	}
}

const WadMapGroup* WadFile::findMap(const std::string& name) const
{
	auto upper = string::to_upper_copy(name);

	for (const auto& group : _maps)
	{
		if (group.name == upper)
		{
			return &group;
		}
	}

	return nullptr;
}

std::vector<char> WadFile::readMapLump(const WadMapGroup& group, const std::string& lumpName)
{
	for (std::size_t i = group.firstLump; i <= group.lastLump && i < _lumps.size(); ++i)
	{
		const auto& lump = _lumps[i];

		if (lump.name != lumpName)
		{
			continue;
		}

		std::vector<char> contents(lump.size);

		if (lump.size > 0)
		{
			_stream.seekg(lump.offset, std::ios_base::beg);
			_stream.read(contents.data(), lump.size);

			if (!_stream.good())
			{
				throw IMapReader::FailureException(
					fmt::format(_("Failed to read lump {0} of map {1}"), lumpName, group.name));
			}
		}

		return contents;
	}

	return {};
}

}

}
