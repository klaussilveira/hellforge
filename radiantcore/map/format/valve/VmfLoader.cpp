#include "VmfLoader.h"

#include "map/format/goldsrc/GoldSrcConversion.h"
#include "imapformat.h"
#include "i18n.h"
#include "itextstream.h"
#include "os/fs.h"
#include "math/pi.h"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <system_error>

namespace map
{

namespace vmf
{

using namespace goldsrc;

namespace
{

const std::size_t MAX_INSTANCE_DEPTH = 16;
const double CORNER_EPSILON = 0.01;

const std::set<std::string> AREAPORTAL_CLASSES = { "func_areaportal", "func_areaportalwindow" };

const std::set<std::string> DROPPED_CLASSES = {
	"func_occluder", "func_viscluster", "env_cubemap", "func_instance_parms", "func_instance_io_proxy"
};

const std::set<std::string> FIXUP_KEYS = { "targetname", "target", "parentname", "filtername", "damagefilter" };

struct Node
{
	std::string name;
	std::vector<std::pair<std::string, std::string>> keyValues;
	std::vector<Node> children;

	const std::string* find(const std::string& key) const
	{
		for (const auto& pair : keyValues)
		{
			if (pair.first == key) return &pair.second;
		}

		return nullptr;
	}

	const Node* child(const std::string& childName) const
	{
		for (const auto& node : children)
		{
			if (node.name == childName) return &node;
		}

		return nullptr;
	}
};

class Tokeniser
{
	const std::string& _data;
	std::size_t _pos = 0;

public:
	explicit Tokeniser(const std::string& data) :
		_data(data)
	{}

	bool next(std::string& token, bool& quoted)
	{
		while (_pos < _data.size() && std::isspace(static_cast<unsigned char>(_data[_pos])))
		{
			++_pos;
		}

		if (_pos >= _data.size())
		{
			return false;
		}

		auto c = _data[_pos];
		quoted = c == '"';

		if (quoted)
		{
			auto end = _data.find('"', _pos + 1);

			if (end == std::string::npos)
			{
				throw IMapReader::FailureException(_("Unterminated string"));
			}

			token = _data.substr(_pos + 1, end - _pos - 1);
			_pos = end + 1;
			return true;
		}

		if (c == '{' || c == '}')
		{
			token = c;
			++_pos;
			return true;
		}

		auto start = _pos;

		while (_pos < _data.size() && !std::isspace(static_cast<unsigned char>(_data[_pos])) &&
			_data[_pos] != '{' && _data[_pos] != '}' && _data[_pos] != '"')
		{
			++_pos;
		}

		token = _data.substr(start, _pos - start);
		return true;
	}
};

void parseBlock(Tokeniser& tok, Node& node)
{
	std::string key;
	std::string value;
	bool keyQuoted;
	bool valueQuoted;

	while (tok.next(key, keyQuoted))
	{
		if (!keyQuoted && key == "}")
		{
			return;
		}

		if (!tok.next(value, valueQuoted))
		{
			break;
		}

		if (!valueQuoted && value == "{")
		{
			node.children.emplace_back();
			node.children.back().name = key;
			parseBlock(tok, node.children.back());
		}
		else
		{
			node.keyValues.emplace_back(key, value);
		}
	}

	throw IMapReader::FailureException(_("Unexpected end of file"));
}

Node parseDocument(const std::string& contents)
{
	Node root;
	Tokeniser tok(contents);

	std::string name;
	std::string brace;
	bool nameQuoted;
	bool braceQuoted;

	while (tok.next(name, nameQuoted))
	{
		if (!tok.next(brace, braceQuoted) || braceQuoted || brace != "{")
		{
			throw IMapReader::FailureException(fmt::format(_("Expected a block after {0}"), name));
		}

		root.children.emplace_back();
		root.children.back().name = name;
		parseBlock(tok, root.children.back());
	}

	return root;
}

std::vector<double> parseDoubles(const std::string& text)
{
	std::vector<double> numbers;
	const char* cursor = text.c_str();

	while (*cursor)
	{
		if (std::strchr("[]() \t", *cursor))
		{
			++cursor;
			continue;
		}

		char* end;
		auto number = std::strtod(cursor, &end);

		if (end == cursor)
		{
			++cursor;
			continue;
		}

		numbers.push_back(number);
		cursor = end;
	}

	return numbers;
}

std::vector<double> parseRows(const Node* node, std::size_t rows)
{
	std::vector<double> numbers;

	if (!node)
	{
		return numbers;
	}

	for (std::size_t row = 0; row < rows; ++row)
	{
		auto value = node->find(fmt::format("row{0}", row));

		if (!value)
		{
			return {};
		}

		auto rowNumbers = parseDoubles(*value);
		numbers.insert(numbers.end(), rowNumbers.begin(), rowNumbers.end());
	}

	return numbers;
}

std::vector<Vector3> toVectors(const std::vector<double>& numbers)
{
	std::vector<Vector3> vectors;

	for (std::size_t i = 0; i + 2 < numbers.size(); i += 3)
	{
		vectors.emplace_back(numbers[i], numbers[i + 1], numbers[i + 2]);
	}

	return vectors;
}

int toInt(const std::string& value)
{
	return static_cast<int>(std::strtol(value.c_str(), nullptr, 10));
}

std::optional<Displacement> parseDisplacement(const Node& node, Vector3& start)
{
	auto power = node.find("power");
	auto startPosition = node.find("startposition");

	if (!power || !startPosition)
	{
		return std::nullopt;
	}

	Displacement displacement;
	displacement.power = static_cast<std::size_t>(std::max(toInt(*power), 0));

	if (displacement.power < 1 || displacement.power > 4)
	{
		return std::nullopt;
	}

	auto size = (std::size_t(1) << displacement.power) + 1;
	auto count = size * size;

	auto startNumbers = parseDoubles(*startPosition);

	if (startNumbers.size() != 3)
	{
		return std::nullopt;
	}

	start = Vector3(startNumbers[0], startNumbers[1], startNumbers[2]);

	if (auto elevation = node.find("elevation"))
	{
		displacement.elevation = std::strtod(elevation->c_str(), nullptr);
	}

	displacement.normals = toVectors(parseRows(node.child("normals"), size));
	displacement.distances = parseRows(node.child("distances"), size);
	displacement.offsets = toVectors(parseRows(node.child("offsets"), size));

	if (displacement.normals.size() != count || displacement.distances.size() != count || displacement.offsets.size() != count)
	{
		return std::nullopt;
	}

	return displacement;
}

bool parseSide(const Node& side, Face& face, AABB& bounds, Vector3& start)
{
	auto plane = side.find("plane");
	auto uAxis = side.find("uaxis");
	auto vAxis = side.find("vaxis");

	if (!plane || !uAxis || !vAxis)
	{
		return false;
	}

	auto points = parseDoubles(*plane);
	auto u = parseDoubles(*uAxis);
	auto v = parseDoubles(*vAxis);

	if (points.size() != 9 || u.size() != 5 || v.size() != 5)
	{
		throw IMapReader::FailureException(fmt::format(_("Invalid side {0}"), *plane));
	}

	Vector3 p1(points[0], points[1], points[2]);
	Vector3 p2(points[3], points[4], points[5]);
	Vector3 p3(points[6], points[7], points[8]);

	bounds.includePoint(p1);
	bounds.includePoint(p2);
	bounds.includePoint(p3);

	if (auto material = side.find("material"))
	{
		face.texture = *material;
	}

	face.plane = Plane3(p3, p2, p1);
	face.valveAxes = true;
	face.uAxis = Vector3(u[0], u[1], u[2]);
	face.shiftU = u[3];
	face.scaleU = u[4] == 0 ? 1 : u[4];
	face.vAxis = Vector3(v[0], v[1], v[2]);
	face.shiftV = v[3];
	face.scaleV = v[4] == 0 ? 1 : v[4];

	if (auto dispinfo = side.child("dispinfo"))
	{
		face.displacement = parseDisplacement(*dispinfo, start);
	}

	return true;
}

bool computeCorners(const std::vector<Face>& faces, std::size_t index, const Vector3& start, Vector3 corners[4])
{
	const auto& base = faces[index].plane;
	std::vector<Vector3> points;

	for (std::size_t a = 0; a < faces.size(); ++a)
	{
		for (std::size_t b = a + 1; b < faces.size(); ++b)
		{
			if (a == index || b == index)
			{
				continue;
			}

			const auto& first = faces[a].plane;
			const auto& second = faces[b].plane;

			auto cross = first.normal().cross(second.normal());
			auto determinant = base.normal().dot(cross);

			if (std::abs(determinant) < 1e-9)
			{
				continue;
			}

			auto point = (cross * base.dist() + second.normal().cross(base.normal()) * first.dist() +
				base.normal().cross(first.normal()) * second.dist()) / determinant;

			bool inside = std::all_of(faces.begin(), faces.end(), [&](const Face& face)
			{
				return face.plane.normal().dot(point) - face.plane.dist() <= CORNER_EPSILON;
			});

			bool known = std::any_of(points.begin(), points.end(), [&](const Vector3& existing)
			{
				return (existing - point).getLength() <= CORNER_EPSILON;
			});

			if (inside && !known)
			{
				points.push_back(point);
			}
		}
	}

	if (points.size() != 4)
	{
		return false;
	}

	auto centre = (points[0] + points[1] + points[2] + points[3]) / 4;
	auto reference = (points[0] - centre).getNormalised();
	auto side = base.normal().cross(reference);

	auto angle = [&](const Vector3& point)
	{
		return std::atan2((point - centre).dot(side), (point - centre).dot(reference));
	};

	std::sort(points.begin(), points.end(), [&](const Vector3& a, const Vector3& b)
	{
		return angle(a) > angle(b);
	});

	std::size_t first = 0;

	for (std::size_t i = 1; i < 4; ++i)
	{
		if ((points[i] - start).getLength() < (points[first] - start).getLength())
		{
			first = i;
		}
	}

	for (std::size_t i = 0; i < 4; ++i)
	{
		corners[i] = points[(first + i) % 4];
	}

	return true;
}

std::vector<int> groupChain(int id, const std::map<int, int>& groupParents)
{
	std::vector<int> chain;

	while (id > 0 && std::find(chain.begin(), chain.end(), id) == chain.end())
	{
		chain.push_back(id);

		auto parent = groupParents.find(id);
		id = parent != groupParents.end() ? parent->second : 0;
	}

	std::reverse(chain.begin(), chain.end());
	return chain;
}

void readEditor(const Node& editor, std::vector<int>& visgroups, std::vector<int>& groups, const std::map<int, int>& groupParents)
{
	for (const auto& [key, value] : editor.keyValues)
	{
		if (key == "visgroupid")
		{
			visgroups.push_back(toInt(value));
		}
		else if (key == "groupid")
		{
			groups = groupChain(toInt(value), groupParents);
		}
	}
}

Solid parseSolid(const Node& node, const std::map<int, int>& groupParents)
{
	Solid solid;
	std::vector<Vector3> starts;

	for (const auto& child : node.children)
	{
		if (child.name == "side")
		{
			Face face;
			Vector3 start(0, 0, 0);

			if (parseSide(child, face, solid.bounds, start))
			{
				solid.faces.push_back(face);
				starts.push_back(start);
			}
		}
		else if (child.name == "editor")
		{
			readEditor(child, solid.visgroups, solid.groups, groupParents);
		}
	}

	for (std::size_t i = 0; i < solid.faces.size(); ++i)
	{
		auto& displacement = solid.faces[i].displacement;

		if (displacement && !computeCorners(solid.faces, i, starts[i], displacement->corners))
		{
			displacement.reset();
		}
	}

	return solid;
}

Entity parseEntity(const Node& node, const std::map<int, int>& groupParents, Statistics& statistics)
{
	Entity entity;

	for (const auto& pair : node.keyValues)
	{
		if (pair.first == "id")
		{
			continue;
		}

		if (pair.first == "classname")
		{
			entity.keyValues.insert(entity.keyValues.begin(), pair);
		}
		else
		{
			entity.keyValues.push_back(pair);
		}
	}

	for (const auto& child : node.children)
	{
		if (child.name == "solid")
		{
			entity.solids.push_back(parseSolid(child, groupParents));
		}
		else if (child.name == "hidden")
		{
			statistics.hiddenObjects += child.children.size();
		}
		else if (child.name == "connections")
		{
			statistics.connections += child.keyValues.size();
		}
		else if (child.name == "editor")
		{
			readEditor(child, entity.visgroups, entity.groups, groupParents);
		}
	}

	return entity;
}

bool hasClassName(const Entity& entity)
{
	return !entity.keyValues.empty() && entity.keyValues.front().first == "classname";
}

void collectVisgroups(const Node& node, std::vector<Visgroup>& visgroups)
{
	for (const auto& child : node.children)
	{
		if (child.name != "visgroup")
		{
			continue;
		}

		auto name = child.find("name");
		auto id = child.find("visgroupid");

		if (name && id)
		{
			visgroups.push_back(Visgroup{ toInt(*id), *name, true });
		}

		collectVisgroups(child, visgroups);
	}
}

MapData parseMap(const std::string& contents, Statistics& statistics)
{
	auto root = parseDocument(contents);

	MapData data;
	const Node* world = nullptr;

	for (const auto& child : root.children)
	{
		if (child.name == "visgroups")
		{
			collectVisgroups(child, data.visgroups);
		}
		else if (child.name == "world")
		{
			world = &child;
		}
		else if (child.name == "hidden")
		{
			statistics.hiddenObjects += child.children.size();
		}
	}

	if (!world)
	{
		throw IMapReader::FailureException(_("No world block found"));
	}

	std::map<int, int> groupParents;

	for (const auto& child : world->children)
	{
		auto id = child.find("id");
		auto editor = child.child("editor");
		auto parent = editor ? editor->find("groupid") : nullptr;

		if (child.name == "group" && id && parent)
		{
			groupParents[toInt(*id)] = toInt(*parent);
		}
	}

	data.world = parseEntity(*world, groupParents, statistics);

	if (!hasClassName(data.world))
	{
		data.world.keyValues.insert(data.world.keyValues.begin(), std::make_pair(std::string("classname"), std::string("worldspawn")));
	}

	for (const auto& child : root.children)
	{
		if (child.name != "entity")
		{
			continue;
		}

		auto entity = parseEntity(child, groupParents, statistics);

		if (hasClassName(entity))
		{
			data.entities.push_back(std::move(entity));
		}
	}

	return data;
}

double toDegrees(double radians)
{
	return radians * 180 / math::PI;
}

struct Transform
{
	Vector3 axes[3];
	Vector3 origin;

	Vector3 rotate(const Vector3& v) const
	{
		return axes[0] * v.x() + axes[1] * v.y() + axes[2] * v.z();
	}

	Vector3 apply(const Vector3& point) const
	{
		return rotate(point) + origin;
	}
};

void anglesToAxes(const Vector3& angles, Vector3 axes[3])
{
	double sp = std::sin(toRadians(angles.x())), cp = std::cos(toRadians(angles.x()));
	double sy = std::sin(toRadians(angles.y())), cy = std::cos(toRadians(angles.y()));
	double sr = std::sin(toRadians(angles.z())), cr = std::cos(toRadians(angles.z()));

	axes[0] = Vector3(cp * cy, cp * sy, -sp);
	axes[1] = Vector3(sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, sr * cp);
	axes[2] = Vector3(cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp);
}

Vector3 axesToAngles(const Vector3 axes[3])
{
	const auto& forward = axes[0];
	const auto& left = axes[1];
	const auto& up = axes[2];

	auto planar = std::sqrt(forward.x() * forward.x() + forward.y() * forward.y());

	if (planar > 0.001)
	{
		return Vector3(toDegrees(std::atan2(-forward.z(), planar)), toDegrees(std::atan2(forward.y(), forward.x())),
			toDegrees(std::atan2(left.z(), up.z())));
	}

	return Vector3(toDegrees(std::atan2(-forward.z(), planar)), toDegrees(std::atan2(-left.x(), left.y())), 0);
}

Vector3 rounded(const Vector3& v)
{
	auto round = [](double value) { return std::round(value * 10000) / 10000 + 0.0; };
	return Vector3(round(v.x()), round(v.y()), round(v.z()));
}

Vector3 toVector(const std::string* value)
{
	auto numbers = parseNumbers(value);
	return numbers.size() == 3 ? Vector3(numbers[0], numbers[1], numbers[2]) : Vector3(0, 0, 0);
}

void transformSolid(Solid& solid, const Transform& transform)
{
	for (auto& face : solid.faces)
	{
		auto normal = transform.rotate(face.plane.normal());
		face.plane = Plane3(normal, face.plane.dist() + normal.dot(transform.origin));

		face.uAxis = transform.rotate(face.uAxis);
		face.vAxis = transform.rotate(face.vAxis);
		face.shiftU -= face.uAxis.dot(transform.origin) / face.scaleU;
		face.shiftV -= face.vAxis.dot(transform.origin) / face.scaleV;

		if (face.displacement)
		{
			for (auto& corner : face.displacement->corners)
			{
				corner = transform.apply(corner);
			}

			for (auto& normalVector : face.displacement->normals)
			{
				normalVector = transform.rotate(normalVector);
			}

			for (auto& offset : face.displacement->offsets)
			{
				offset = transform.rotate(offset);
			}
		}
	}

	if (solid.bounds.isValid())
	{
		auto min = solid.bounds.origin - solid.bounds.extents;
		auto max = solid.bounds.origin + solid.bounds.extents;

		AABB bounds;

		for (int i = 0; i < 8; ++i)
		{
			bounds.includePoint(transform.apply(Vector3(i & 1 ? max.x() : min.x(), i & 2 ? max.y() : min.y(), i & 4 ? max.z() : min.z())));
		}

		solid.bounds = bounds;
	}
}

void transformEntity(Entity& entity, const Transform& transform)
{
	if (auto origin = findValue(entity, "origin"))
	{
		*origin = formatVector(rounded(transform.apply(toVector(origin))));
	}

	for (const char* key : { "angles", "movedir" })
	{
		auto value = findValue(entity, key);

		if (!value || parseNumbers(value).size() != 3)
		{
			continue;
		}

		Vector3 axes[3];
		anglesToAxes(toVector(value), axes);

		for (auto& axis : axes)
		{
			axis = transform.rotate(axis);
		}

		*value = formatVector(rounded(axesToAngles(axes)));
	}

	for (auto& solid : entity.solids)
	{
		transformSolid(solid, transform);
	}
}

void applyReplacements(MapData& instance, Entity& source)
{
	std::vector<std::pair<std::string, std::string>> replacements;

	for (const auto& [key, value] : source.keyValues)
	{
		auto space = value.find(' ');

		if (key.compare(0, 7, "replace") == 0 && space != std::string::npos && space > 0)
		{
			replacements.emplace_back(value.substr(0, space), value.substr(space + 1));
		}
	}

	if (replacements.empty())
	{
		return;
	}

	std::sort(replacements.begin(), replacements.end(), [](const auto& a, const auto& b)
	{
		return a.first.size() > b.first.size();
	});

	auto replace = [&](Entity& entity)
	{
		for (auto& pair : entity.keyValues)
		{
			for (const auto& [variable, replacement] : replacements)
			{
				for (auto pos = pair.second.find(variable); pos != std::string::npos;
					pos = pair.second.find(variable, pos + replacement.size()))
				{
					pair.second.replace(pos, variable.size(), replacement);
				}
			}
		}
	};

	replace(instance.world);

	for (auto& entity : instance.entities)
	{
		replace(entity);
	}
}

void fixupNames(MapData& instance, Entity& source, std::size_t& autoNames)
{
	auto style = parseNumbers(findValue(source, "fixup_style"));
	auto fixupStyle = style.empty() ? 0 : static_cast<int>(style[0]);

	if (fixupStyle == 2)
	{
		return;
	}

	auto targetName = findValue(source, "targetname");
	auto name = targetName && !targetName->empty() ? *targetName : fmt::format("InstanceAuto{0}", ++autoNames);

	for (auto& entity : instance.entities)
	{
		for (auto& [key, value] : entity.keyValues)
		{
			if (FIXUP_KEYS.count(key) == 0 || value.empty() || value[0] == '@' || value[0] == '!')
			{
				continue;
			}

			value = fixupStyle == 0 ? name + "-" + value : value + "-" + name;
		}
	}
}

fs::path resolveInstancePath(std::string file, const fs::path& directory, const fs::path& mapDirectory)
{
	std::replace(file.begin(), file.end(), '\\', '/');

	for (const auto& base : { directory, mapDirectory })
	{
		for (auto folder = base; !folder.empty(); folder = folder.parent_path())
		{
			std::error_code error;
			auto candidate = folder / file;

			if (fs::is_regular_file(candidate, error))
			{
				return candidate;
			}

			if (folder == folder.parent_path())
			{
				break;
			}
		}
	}

	return {};
}

void expandInstances(MapData& data, const fs::path& directory, const fs::path& mapDirectory, std::size_t depth,
	Statistics& statistics, std::size_t& autoNames)
{
	std::vector<Entity> entities;
	std::vector<Entity> merged;

	for (auto& entity : data.entities)
	{
		if (entity.keyValues.front().second != "func_instance")
		{
			entities.push_back(std::move(entity));
			continue;
		}

		statistics.instances++;

		auto file = findValue(entity, "file");

		if (!file || file->empty())
		{
			continue;
		}

		if (depth >= MAX_INSTANCE_DEPTH)
		{
			statistics.failedInstances.push_back(fmt::format("{0} (nested too deeply)", *file));
			continue;
		}

		auto path = resolveInstancePath(*file, directory, mapDirectory);
		std::ifstream stream(path, std::ios::in | std::ios::binary);

		if (path.empty() || !stream)
		{
			statistics.failedInstances.push_back(fmt::format("{0} (not found)", *file));
			continue;
		}

		std::string contents{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
		MapData instance;

		try
		{
			instance = parseMap(contents, statistics);
		}
		catch (const IMapReader::FailureException& ex)
		{
			statistics.failedInstances.push_back(fmt::format("{0} ({1})", path.string(), ex.what()));
			continue;
		}

		applyReplacements(instance, entity);
		expandInstances(instance, path.parent_path(), mapDirectory, depth + 1, statistics, autoNames);
		fixupNames(instance, entity, autoNames);

		Transform transform;
		anglesToAxes(toVector(findValue(entity, "angles")), transform.axes);
		transform.origin = toVector(findValue(entity, "origin"));

		for (auto& solid : instance.world.solids)
		{
			transformSolid(solid, transform);
			solid.visgroups = entity.visgroups;
			solid.groups.clear();
			data.world.solids.push_back(std::move(solid));
		}

		for (auto& instanceEntity : instance.entities)
		{
			transformEntity(instanceEntity, transform);
			instanceEntity.visgroups = entity.visgroups;
			instanceEntity.groups.clear();
			merged.push_back(std::move(instanceEntity));
		}
	}

	entities.insert(entities.end(), std::make_move_iterator(merged.begin()), std::make_move_iterator(merged.end()));
	data.entities = std::move(entities);
}

void finalise(MapData& data)
{
	std::vector<Entity> entities;

	for (auto& entity : data.entities)
	{
		const auto& className = entity.keyValues.front().second;

		if (AREAPORTAL_CLASSES.count(className) > 0)
		{
			for (auto& solid : entity.solids)
			{
				solid.visgroups = entity.visgroups;
				data.world.solids.push_back(std::move(solid));
			}
		}
		else if (DROPPED_CLASSES.count(className) == 0)
		{
			entities.push_back(std::move(entity));
		}
	}

	data.entities = std::move(entities);
}

}

MapData load(const std::string& contents, const std::string& mapPath, Statistics& statistics)
{
	auto data = parseMap(contents, statistics);
	auto directory = fs::path(mapPath).parent_path();
	std::size_t autoNames = 0;

	expandInstances(data, directory, directory, 0, statistics, autoNames);
	finalise(data);

	return data;
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
		Statistics statistics;
		collectNames(load(contents, path, statistics), result);
	}
	catch (const IMapReader::FailureException& ex)
	{
		rWarning() << "[vmf]: " << ex.what() << std::endl;
	}

	return result;
}

}

}
