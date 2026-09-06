#pragma once

#include <cstdint>
#include <istream>
#include <string>
#include <vector>

namespace map
{

namespace doom
{

struct WadLump
{
	std::string name;
	std::size_t offset;
	std::size_t size;
};

enum class MapLumpKind
{
	DoomBinary,
	HexenBinary,
	Udmf
};

struct WadMapGroup
{
	std::string name;
	MapLumpKind kind;
	std::size_t firstLump;
	std::size_t lastLump;
};

class WadFile
{
	std::istream& _stream;
	std::vector<WadLump> _lumps;
	std::vector<WadMapGroup> _maps;

public:
	WadFile(std::istream& stream);

	const std::vector<WadMapGroup>& getMaps() const { return _maps; }

	const WadMapGroup* findMap(const std::string& name) const;

	std::vector<char> readMapLump(const WadMapGroup& group, const std::string& lumpName);

	static bool hasWadSignature(std::istream& stream);

private:
	void readDirectory();
	void collectMaps();
};

}

}
