#define SPECIALISE_STR_TO_FLOAT

#include "UdmfLoader.h"

#include "imapformat.h"
#include "itextstream.h"
#include "i18n.h"
#include "parser/DefTokeniser.h"
#include "string/case_conv.h"
#include "string/convert.h"

#include <fmt/format.h>

namespace map
{

namespace doom
{

namespace
{

const int DEFAULT_LIGHT_LEVEL = 160;

class Parser
{
	parser::BasicDefTokeniser<std::string> _tok;
	DoomMapData _data;

public:
	Parser(const std::string& contents) :
		_tok(contents, parser::WHITESPACE, "{}=;")
	{}

	DoomMapData parse()
	{
		while (_tok.hasMoreTokens())
		{
			auto identifier = string::to_lower_copy(_tok.nextToken());

			if (!_tok.hasMoreTokens())
			{
				break;
			}

			auto next = _tok.nextToken();

			if (next == "=")
			{
				skipValue();
			}
			else if (next == "{")
			{
				parseBlock(identifier);
			}
		}

		return std::move(_data);
	}

private:
	void skipValue()
	{
		while (_tok.hasMoreTokens())
		{
			if (_tok.nextToken() == ";")
			{
				return;
			}
		}
	}

	std::string readValue()
	{
		auto token = _tok.nextToken();

		if (token != "=")
		{
			throw IMapReader::FailureException(
				fmt::format(_("UDMF: expected '=' but found '{0}'"), token));
		}

		auto value = _tok.nextToken();

		if (_tok.hasMoreTokens() && _tok.peek() == ";")
		{
			_tok.nextToken();
		}

		return value;
	}

	static bool toBool(const std::string& value)
	{
		return string::to_lower_copy(value) == "true";
	}

	void parseBlock(const std::string& type)
	{
		if (type == "vertex")
		{
			parseVertex();
		}
		else if (type == "sector")
		{
			parseSector();
		}
		else if (type == "sidedef")
		{
			parseSideDef();
		}
		else if (type == "linedef")
		{
			parseLineDef();
		}
		else if (type == "thing")
		{
			parseThing();
		}
		else
		{
			skipBlock();
		}
	}

	void skipBlock()
	{
		std::size_t depth = 1;

		while (depth > 0 && _tok.hasMoreTokens())
		{
			auto token = _tok.nextToken();

			if (token == "{") depth++;
			else if (token == "}") depth--;
		}
	}

	std::string nextKey()
	{
		while (_tok.hasMoreTokens())
		{
			auto token = _tok.nextToken();

			if (token == "}")
			{
				return {};
			}

			if (token == ";")
			{
				continue;
			}

			return string::to_lower_copy(token);
		}

		return {};
	}

	void parseVertex()
	{
		DoomVertex vertex;

		for (auto key = nextKey(); !key.empty(); key = nextKey())
		{
			auto value = readValue();

			if (key == "x") vertex.x = string::to_float(value);
			else if (key == "y") vertex.y = string::to_float(value);
		}

		_data.vertices.push_back(vertex);
	}

	void parseSector()
	{
		DoomSector sector;
		sector.lightLevel = DEFAULT_LIGHT_LEVEL;

		for (auto key = nextKey(); !key.empty(); key = nextKey())
		{
			auto value = readValue();

			if (key == "heightfloor") sector.floorHeight = string::to_float(value);
			else if (key == "heightceiling") sector.ceilingHeight = string::to_float(value);
			else if (key == "texturefloor") sector.floorTexture = value;
			else if (key == "textureceiling") sector.ceilingTexture = value;
			else if (key == "lightlevel") sector.lightLevel = string::convert<int>(value, DEFAULT_LIGHT_LEVEL);
		}

		_data.sectors.push_back(sector);
	}

	void parseSideDef()
	{
		DoomSideDef side;
		side.upperTexture = NO_TEXTURE;
		side.lowerTexture = NO_TEXTURE;
		side.middleTexture = NO_TEXTURE;

		for (auto key = nextKey(); !key.empty(); key = nextKey())
		{
			auto value = readValue();

			if (key == "offsetx") side.offsetX = string::to_float(value);
			else if (key == "offsety") side.offsetY = string::to_float(value);
			else if (key == "texturetop") side.upperTexture = value;
			else if (key == "texturebottom") side.lowerTexture = value;
			else if (key == "texturemiddle") side.middleTexture = value;
			else if (key == "sector") side.sector = string::convert<int>(value, NO_INDEX);
		}

		_data.sideDefs.push_back(side);
	}

	void parseLineDef()
	{
		DoomLineDef line;

		for (auto key = nextKey(); !key.empty(); key = nextKey())
		{
			auto value = readValue();

			if (key == "v1") line.v1 = string::convert<int>(value, NO_INDEX);
			else if (key == "v2") line.v2 = string::convert<int>(value, NO_INDEX);
			else if (key == "sidefront") line.sideFront = string::convert<int>(value, NO_INDEX);
			else if (key == "sideback") line.sideBack = string::convert<int>(value, NO_INDEX);
			else if (key == "dontpegtop") line.dontPegTop = toBool(value);
			else if (key == "dontpegbottom") line.dontPegBottom = toBool(value);
		}

		_data.lineDefs.push_back(line);
	}

	void parseThing()
	{
		DoomThing thing;

		for (auto key = nextKey(); !key.empty(); key = nextKey())
		{
			auto value = readValue();

			if (key == "x") thing.x = string::to_float(value);
			else if (key == "y") thing.y = string::to_float(value);
			else if (key == "angle") thing.angle = string::to_float(value);
			else if (key == "type") thing.type = string::convert<int>(value, 0);
		}

		_data.things.push_back(thing);
	}
};

}

DoomMapData UdmfLoader::load(const std::vector<char>& textMap)
{
	std::string contents(textMap.begin(), textMap.end());

	Parser parser(contents);

	auto data = parser.parse();

	if (auto removed = removeInvalidLineDefs(data); removed > 0)
	{
		rWarning() << "[wad]: dropped " << removed << " linedefs with invalid vertices" << std::endl;
	}

	return data;
}

}

}
