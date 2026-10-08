#include "GoldSrcLoader.h"

#include "imapformat.h"
#include "i18n.h"
#include "itextstream.h"
#include "os/fs.h"
#include "string/case_conv.h"

#include <fmt/format.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <system_error>

namespace map
{

namespace goldsrc
{

namespace
{

const std::size_t JMF_BACKGROUND_IMAGE_SIZE = 8 + 6 * 4;
const std::size_t JMF_GROUP_SIZE = 5 * 4;
const std::size_t JMF_CAMERA_SIZE = 6 * 4 + 4 + 4;
const std::size_t JMF_ENTITY_PROPERTIES_SIZE = 12 + 4 + 4 + 4 + 4 + 2 + 2 + 4 + 4 + 4 + 4 + 28;
const std::size_t JMF_SURFACE_SIZE = 11 * 4 + 16 + 4 + 64;
const std::size_t JMF_CURVE_SIZE = 8 + JMF_SURFACE_SIZE + 4 + 32 * 32 * 36;
const std::size_t JMF_STRING_COUNT = 13;
const int WAD_MIPTEX_TYPE = 0x43;

class Reader
{
	const std::string& _data;
	std::size_t _pos = 0;

public:
	explicit Reader(const std::string& data) :
		_data(data)
	{}

	bool atEnd() const
	{
		return _pos >= _data.size();
	}

	void skip(std::size_t count)
	{
		require(count);
		_pos += count;
	}

	template<typename T>
	T read()
	{
		require(sizeof(T));
		T value;
		std::memcpy(&value, _data.data() + _pos, sizeof(T));
		_pos += sizeof(T);
		return value;
	}

	int readInt()
	{
		return read<int32_t>();
	}

	double readFloat()
	{
		return read<float>();
	}

	std::size_t readCount()
	{
		auto count = readInt();

		if (count < 0)
		{
			throw IMapReader::FailureException(fmt::format(_("Invalid element count {0}"), count));
		}

		return static_cast<std::size_t>(count);
	}

	Vector3 readVector()
	{
		auto x = readFloat();
		auto y = readFloat();
		auto z = readFloat();
		return Vector3(x, y, z);
	}

	std::string readFixedString(std::size_t length)
	{
		require(length);
		std::string value = _data.substr(_pos, length);
		_pos += length;
		return value.substr(0, value.find('\0'));
	}

	std::string readByteString()
	{
		return readFixedString(read<uint8_t>());
	}

	std::string readIntString()
	{
		auto length = readInt();
		return length > 0 ? readFixedString(static_cast<std::size_t>(length)) : std::string();
	}

private:
	void require(std::size_t count) const
	{
		if (count > _data.size() - _pos)
		{
			throw IMapReader::FailureException(_("Unexpected end of file"));
		}
	}
};

bool hasKey(const Entity& entity, const std::string& key)
{
	for (const auto& pair : entity.keyValues)
	{
		if (pair.first == key) return true;
	}

	return false;
}

void addMissingKey(Entity& entity, const std::string& key, const std::string& value)
{
	if (!hasKey(entity, key))
	{
		entity.keyValues.emplace_back(key, value);
	}
}

std::string formatOrigin(const Vector3& origin)
{
	return fmt::format("{0:g} {1:g} {2:g}", origin.x(), origin.y(), origin.z());
}

void finishEntity(Entity& entity, int spawnflags, const Vector3& origin)
{
	if (spawnflags != 0)
	{
		addMissingKey(entity, "spawnflags", std::to_string(spawnflags));
	}

	if (entity.solids.empty())
	{
		addMissingKey(entity, "origin", formatOrigin(origin));
	}
}

class RmfLoader
{
	Reader _reader;
	int _version = 0;
	int _nextGroupId = 1;
	MapData _map;

public:
	explicit RmfLoader(const std::string& contents) :
		_reader(contents)
	{}

	MapData load()
	{
		_version = static_cast<int>(std::lround(_reader.read<float>() * 10));

		if (_version != 8 && _version != 9 && _version != 14 && _version != 16 &&
			_version != 18 && _version != 22)
		{
			throw IMapReader::FailureException(fmt::format(_("Unsupported RMF version {0}"), _version / 10.0));
		}

		if (_reader.readFixedString(3) != "RMF")
		{
			throw IMapReader::FailureException(_("Missing RMF header"));
		}

		if (_version > 9)
		{
			auto count = _reader.readCount();

			for (std::size_t i = 0; i < count; ++i)
			{
				auto name = _reader.readFixedString(128);
				_reader.skip(4);
				auto id = _reader.readInt();
				bool visible = _reader.read<uint8_t>() != 0;
				_reader.skip(3);
				_map.visgroups.push_back({ id, name, visible });
			}
		}

		if (_reader.readByteString() != "CMapWorld")
		{
			throw IMapReader::FailureException(_("RMF file does not start with a world object"));
		}

		readBase(_map.world, {});
		readEntityData(_map.world);

		if (_version <= 9)
		{
			auto count = _reader.readCount();

			for (std::size_t i = 0; i < count; ++i)
			{
				auto name = _reader.readFixedString(128);
				_reader.skip(4);
				int id = _reader.read<uint8_t>();
				bool visible = _reader.read<uint8_t>() != 0;
				_reader.skip(2);
				_map.visgroups.push_back({ id, name, visible });
			}
		}

		if (_version >= 14)
		{
			auto pathCount = _reader.readCount();

			for (std::size_t i = 0; i < pathCount; ++i)
			{
				_reader.skip(128 + 128 + 4);

				auto nodeCount = _reader.readCount();

				for (std::size_t n = 0; n < nodeCount; ++n)
				{
					_reader.skip(12 + 4 + 128);

					auto propertyCount = _reader.readCount();

					for (std::size_t p = 0; p < propertyCount; ++p)
					{
						_reader.readByteString();
						_reader.readByteString();
					}
				}
			}

			_map.skippedPaths = pathCount;
		}

		return std::move(_map);
	}

private:
	std::vector<int> readBase(Entity& owner, const std::vector<int>& groups)
	{
		std::vector<int> visgroups;

		if (_version <= 9)
		{
			auto count = _reader.readCount();

			for (std::size_t i = 0; i < count; ++i)
			{
				int id = _reader.read<uint8_t>();
				if (id > 0) visgroups.push_back(id);
			}
		}
		else
		{
			auto id = _reader.readInt();
			if (id > 0) visgroups.push_back(id);
		}

		_reader.skip(3);

		auto childCount = _reader.readCount();

		for (std::size_t i = 0; i < childCount; ++i)
		{
			readObject(owner, groups);
		}

		return visgroups;
	}

	void readObject(Entity& owner, const std::vector<int>& groups)
	{
		auto type = _reader.readByteString();

		if (type == "CMapSolid")
		{
			Solid solid;
			solid.visgroups = readBase(owner, groups);
			solid.groups = groups;

			auto faceCount = _reader.readCount();

			for (std::size_t i = 0; i < faceCount; ++i)
			{
				readFace(solid);
			}

			owner.solids.push_back(std::move(solid));
		}
		else if (type == "CMapGroup")
		{
			auto nested = groups;
			nested.push_back(_nextGroupId++);
			readBase(owner, nested);
		}
		else if (type == "CMapEntity")
		{
			Entity entity;
			entity.visgroups = readBase(entity, {});
			entity.groups = groups;

			auto spawnflags = readEntityData(entity);
			_reader.skip(2);
			auto origin = _reader.readVector();
			_reader.skip(4);

			finishEntity(entity, spawnflags, origin);

			_map.entities.push_back(std::move(entity));
		}
		else
		{
			throw IMapReader::FailureException(fmt::format(_("Unknown RMF object type {0}"), type));
		}
	}

	int readEntityData(Entity& entity)
	{
		entity.keyValues.emplace_back("classname", _reader.readByteString());
		_reader.skip(4);
		auto spawnflags = _reader.readInt();

		auto count = _reader.readCount();

		for (std::size_t i = 0; i < count; ++i)
		{
			auto key = _reader.readByteString();
			auto value = _reader.readByteString();

			if (!key.empty() && key != "classname")
			{
				entity.keyValues.emplace_back(key, value);
			}
		}

		if (_version >= 16)
		{
			_reader.skip(12);
		}

		return spawnflags;
	}

	void readFace(Solid& solid)
	{
		Face face;
		face.texture = _reader.readFixedString(_version <= 8 ? 16 : _version <= 16 ? 40 : 260);

		if (_version <= 18)
		{
			face.rotation = _reader.readFloat();
			face.shiftU = _reader.readFloat();
			face.shiftV = _reader.readFloat();
		}
		else
		{
			face.valveAxes = true;
			face.uAxis = _reader.readVector();
			face.shiftU = _reader.readFloat();
			face.vAxis = _reader.readVector();
			face.shiftV = _reader.readFloat();
			face.rotation = _reader.readFloat();
		}

		face.scaleU = _reader.readFloat();
		face.scaleV = _reader.readFloat();

		if (_version > 16)
		{
			_reader.skip(16);
		}
		else if (_version > 9)
		{
			_reader.skip(4);
		}

		auto vertexCount = _reader.readCount();

		for (std::size_t i = 0; i < vertexCount; ++i)
		{
			solid.bounds.includePoint(_reader.readVector());
		}

		auto a = _reader.readVector();
		auto b = _reader.readVector();
		auto c = _reader.readVector();

		if (vertexCount < 3)
		{
			return;
		}

		face.plane = Plane3(c, b, a);
		solid.faces.push_back(std::move(face));
	}
};

class JmfLoader
{
	Reader _reader;
	std::map<int, int> _groupParents;
	MapData _map;
	bool _hasWorld = false;

public:
	explicit JmfLoader(const std::string& contents) :
		_reader(contents)
	{}

	MapData load()
	{
		if (_reader.readFixedString(4) != "JHMF")
		{
			throw IMapReader::FailureException(_("Missing JMF header"));
		}

		auto version = _reader.readInt();

		if (version != 121 && version != 122)
		{
			throw IMapReader::FailureException(fmt::format(_("Unsupported JMF version {0}"), version));
		}

		auto exportCount = _reader.readCount();

		for (std::size_t i = 0; i < exportCount; ++i)
		{
			_reader.readIntString();
		}

		if (version >= 122)
		{
			for (int i = 0; i < 3; ++i)
			{
				_reader.readIntString();
				_reader.skip(JMF_BACKGROUND_IMAGE_SIZE);
			}
		}

		auto groupCount = _reader.readCount();

		for (std::size_t i = 0; i < groupCount; ++i)
		{
			auto id = _reader.readInt();
			auto parent = _reader.readInt();
			_reader.skip(JMF_GROUP_SIZE - 8);
			_groupParents[id] = parent;
		}

		auto visgroupCount = _reader.readCount();

		for (std::size_t i = 0; i < visgroupCount; ++i)
		{
			auto name = _reader.readIntString();
			auto id = _reader.readInt();
			_reader.skip(4);
			bool visible = _reader.read<uint8_t>() != 0;
			_map.visgroups.push_back({ id, name, visible });
		}

		_reader.skip(24);
		_reader.skip(_reader.readCount() * JMF_CAMERA_SIZE);

		readPaths();

		while (!_reader.atEnd())
		{
			readEntity();
		}

		if (!_hasWorld)
		{
			_map.world.keyValues.emplace_back("classname", "worldspawn");
		}

		return std::move(_map);
	}

private:
	std::vector<int> groupChain(int id) const
	{
		std::vector<int> chain;

		while (id != 0 && chain.size() <= _groupParents.size())
		{
			auto found = _groupParents.find(id);

			if (found == _groupParents.end())
			{
				break;
			}

			chain.insert(chain.begin(), id);

			if (found->second == id)
			{
				break;
			}

			id = found->second;
		}

		return chain;
	}

	std::vector<int> readVisgroupIds()
	{
		std::vector<int> ids;
		auto count = _reader.readCount();

		for (std::size_t i = 0; i < count; ++i)
		{
			ids.push_back(_reader.readInt());
		}

		return ids;
	}

	void readKeyValues(Entity& entity)
	{
		auto count = _reader.readCount();

		for (std::size_t i = 0; i < count; ++i)
		{
			auto key = _reader.readIntString();
			auto value = _reader.readIntString();

			if (!key.empty() && key != "classname")
			{
				entity.keyValues.emplace_back(key, value);
			}
		}
	}

	void readPaths()
	{
		auto pathCount = _reader.readCount();

		for (std::size_t i = 0; i < pathCount; ++i)
		{
			_reader.readIntString();
			_reader.readIntString();
			_reader.skip(4 + 4 + 4);

			auto nodeCount = _reader.readCount();

			for (std::size_t n = 0; n < nodeCount; ++n)
			{
				_reader.readIntString();
				_reader.readIntString();
				_reader.skip(12 + 12 + 4 + 4);

				Entity ignored;
				readKeyValues(ignored);
			}
		}

		_map.skippedPaths = pathCount;
	}

	void readEntity()
	{
		Entity entity;
		auto classname = _reader.readIntString();
		entity.keyValues.emplace_back("classname", classname);

		auto origin = _reader.readVector();
		_reader.skip(4);
		entity.groups = groupChain(_reader.readInt());
		_reader.skip(4 + 4);

		for (std::size_t i = 0; i < JMF_STRING_COUNT; ++i)
		{
			_reader.readIntString();
		}

		auto spawnflags = _reader.readInt();
		_reader.skip(JMF_ENTITY_PROPERTIES_SIZE);

		readKeyValues(entity);
		entity.visgroups = readVisgroupIds();

		auto solidCount = _reader.readCount();

		for (std::size_t i = 0; i < solidCount; ++i)
		{
			readSolid(entity);
		}

		finishEntity(entity, spawnflags, origin);

		if (classname == "worldspawn" && !_hasWorld)
		{
			_hasWorld = true;
			_map.world = std::move(entity);
		}
		else
		{
			_map.entities.push_back(std::move(entity));
		}
	}

	void readSolid(Entity& owner)
	{
		Solid solid;

		auto curveCount = _reader.readCount();
		_reader.skip(4);
		solid.groups = groupChain(_reader.readInt());
		_reader.skip(4 + 4);
		solid.visgroups = readVisgroupIds();

		auto faceCount = _reader.readCount();

		for (std::size_t i = 0; i < faceCount; ++i)
		{
			readFace(solid);
		}

		_reader.skip(curveCount * JMF_CURVE_SIZE);
		_map.skippedCurves += curveCount;

		owner.solids.push_back(std::move(solid));
	}

	void readFace(Solid& solid)
	{
		Face face;
		face.valveAxes = true;

		_reader.skip(4);
		auto vertexCount = _reader.readCount();

		face.uAxis = _reader.readVector();
		face.shiftU = _reader.readFloat();
		face.vAxis = _reader.readVector();
		face.shiftV = _reader.readFloat();
		face.scaleU = _reader.readFloat();
		face.scaleV = _reader.readFloat();
		face.rotation = _reader.readFloat();
		_reader.skip(16 + 4);
		face.texture = _reader.readFixedString(64);

		auto normal = _reader.readVector();
		auto dist = _reader.readFloat();
		_reader.skip(4);

		for (std::size_t i = 0; i < vertexCount; ++i)
		{
			solid.bounds.includePoint(_reader.readVector());
			_reader.skip(12);
		}

		if (vertexCount < 3)
		{
			return;
		}

		face.plane = Plane3(normal, dist);
		solid.faces.push_back(std::move(face));
	}
};

}

MapData load(const std::string& contents)
{
	if (contents.compare(0, 4, "JHMF") == 0)
	{
		return JmfLoader(contents).load();
	}

	return RmfLoader(contents).load();
}

std::map<std::string, std::pair<double, double>> readWadTextureSizes(const std::string& mapPath)
{
	std::map<std::string, std::pair<double, double>> sizes;

	auto mapDir = fs::path(mapPath).parent_path();
	std::vector<fs::path> wadPaths;

	for (const auto& dir : { mapDir, mapDir.parent_path() })
	{
		std::error_code error;

		for (const auto& file : fs::directory_iterator(dir, error))
		{
			if (string::to_lower_copy(file.path().extension().string()) == ".wad")
			{
				wadPaths.push_back(file.path());
			}
		}
	}

	if (wadPaths.empty())
	{
		rWarning() << "[goldsrc]: No WAD files next to the map, texture sizes are taken from the mapped materials" << std::endl;
	}

	for (const auto& wadPath : wadPaths)
	{
		std::ifstream stream(wadPath.string(), std::ios::in | std::ios::binary);
		std::string contents{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };

		try
		{
			Reader header(contents);

			if (header.readFixedString(4) != "WAD3")
			{
				continue;
			}

			auto lumpCount = header.readCount();
			auto directoryOffset = header.readCount();

			Reader directory(contents);
			directory.skip(directoryOffset);

			for (std::size_t i = 0; i < lumpCount; ++i)
			{
				auto offset = directory.readCount();
				directory.skip(8);
				int type = directory.read<uint8_t>();
				int compression = directory.read<uint8_t>();
				directory.skip(2);
				auto name = string::to_upper_copy(directory.readFixedString(16));

				if (type != WAD_MIPTEX_TYPE || compression != 0) continue;

				Reader texture(contents);
				texture.skip(offset + 16);
				auto width = texture.readInt();
				auto height = texture.readInt();

				if (width > 0 && height > 0)
				{
					sizes.emplace(name, std::make_pair(width, height));
				}
			}
		}
		catch (const IMapReader::FailureException& ex)
		{
			rWarning() << "[goldsrc]: " << wadPath.string() << ": " << ex.what() << std::endl;
		}
	}

	return sizes;
}

void collectNames(const MapData& data, ScanResult& result)
{
	auto collect = [&](const Entity& entity)
	{
		const auto& className = entity.keyValues.front().second;
		result.entities.insert(className);

		if (&entity != &data.world && !entity.solids.empty())
		{
			result.brushEntities.insert(className);
		}

		for (const auto& solid : entity.solids)
		{
			for (const auto& face : solid.faces)
			{
				result.textures.insert(face.texture);
			}
		}
	};

	collect(data.world);

	for (const auto& entity : data.entities)
	{
		collect(entity);
	}
}

ScanResult scanMap(const std::string& path)
{
	ScanResult result;

	std::ifstream stream(path, std::ios::in | std::ios::binary);

	if (!stream)
	{
		return result;
	}

	std::string contents{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };

	try
	{
		collectNames(load(contents), result);

		result.textureSizes = readWadTextureSizes(path);
	}
	catch (const IMapReader::FailureException& ex)
	{
		rWarning() << "[goldsrc]: " << ex.what() << std::endl;
	}

	return result;
}

}

}
