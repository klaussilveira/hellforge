#include "RadiantTest.h"

#include "ibrush.h"
#include "icommandsystem.h"
#include "ilayer.h"
#include "imap.h"
#include "imapformat.h"
#include "ipatch.h"
#include "iselectiongroup.h"
#include "messages/MapConversionRequest.h"
#include "algorithm/Primitives.h"
#include "algorithm/Scene.h"
#include "testutil/FileSelectionHelper.h"
#include "testutil/TemporaryFile.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <sstream>

namespace test
{

namespace
{

struct BoxFace
{
	Vector3 a;
	Vector3 b;
	Vector3 c;
	Vector3 uAxis;
	Vector3 vAxis;
};

std::array<BoxFace, 6> boxFaces(const Vector3& min, const Vector3& max)
{
	double x0 = min.x(), y0 = min.y(), z0 = min.z();
	double x1 = max.x(), y1 = max.y(), z1 = max.z();

	return { {
		{ { x0, y0, z0 }, { x1, y0, z0 }, { x0, y1, z0 }, { 1, 0, 0 }, { 0, -1, 0 } },
		{ { x0, y0, z1 }, { x0, y1, z1 }, { x1, y0, z1 }, { 1, 0, 0 }, { 0, -1, 0 } },
		{ { x0, y0, z0 }, { x0, y1, z0 }, { x0, y0, z1 }, { 0, 1, 0 }, { 0, 0, -1 } },
		{ { x1, y0, z0 }, { x1, y0, z1 }, { x1, y1, z0 }, { 0, 1, 0 }, { 0, 0, -1 } },
		{ { x0, y0, z0 }, { x0, y0, z1 }, { x1, y0, z0 }, { 1, 0, 0 }, { 0, 0, -1 } },
		{ { x0, y1, z0 }, { x1, y1, z0 }, { x0, y1, z1 }, { 1, 0, 0 }, { 0, 0, -1 } },
	} };
}

std::string vector(const Vector3& v)
{
	std::ostringstream stream;
	stream << v.x() << " " << v.y() << " " << v.z();
	return stream.str();
}

std::string solid(const Vector3& min, const Vector3& max, const std::string& material,
	const std::string& editor = "", const std::string& topExtra = "")
{
	std::string text = "\tsolid\n\t{\n\t\t\"id\" \"1\"\n";
	auto faces = boxFaces(min, max);

	for (std::size_t i = 0; i < faces.size(); ++i)
	{
		const auto& face = faces[i];

		text += "\t\tside\n\t\t{\n";
		text += "\t\t\t\"plane\" \"(" + vector(face.a) + ") (" + vector(face.b) + ") (" + vector(face.c) + ")\"\n";
		text += "\t\t\t\"material\" \"" + material + "\"\n";
		text += "\t\t\t\"uaxis\" \"[" + vector(face.uAxis) + " 0] 0.25\"\n";
		text += "\t\t\t\"vaxis\" \"[" + vector(face.vAxis) + " 0] 0.25\"\n";

		if (i == 1)
		{
			text += topExtra;
		}

		text += "\t\t}\n";
	}

	if (!editor.empty())
	{
		text += "\t\teditor\n\t\t{\n" + editor + "\t\t}\n";
	}

	return text + "\t}\n";
}

std::string keyValues(const std::vector<std::pair<std::string, std::string>>& pairs)
{
	std::string text;

	for (const auto& [key, value] : pairs)
	{
		text += "\t\"" + key + "\" \"" + value + "\"\n";
	}

	return text;
}

std::string entity(const std::vector<std::pair<std::string, std::string>>& pairs, const std::string& body = "")
{
	return "entity\n{\n\t\"id\" \"2\"\n" + keyValues(pairs) + body + "}\n";
}

std::string world(const std::string& body, const std::string& header = "")
{
	return header + "world\n{\n\t\"id\" \"1\"\n\t\"classname\" \"worldspawn\"\n" + body + "}\n";
}

const char* const WALL = "BRICK/BRICKWALL001";

std::vector<double> numbers(const std::string& value)
{
	std::vector<double> result;
	std::stringstream stream(value);
	double number;

	while (stream >> number)
	{
		result.push_back(number);
	}

	return result;
}

std::size_t countBrushes(const scene::INodePtr& node)
{
	std::size_t count = 0;

	node->foreachNode([&](const scene::INodePtr& child)
	{
		if (Node_isBrush(child)) count++;
		return true;
	});

	return count;
}

std::optional<Vector2> textureCoordinateAt(const IFace* face, const Vector3& position)
{
	for (const auto& vertex : face->getWinding())
	{
		if (math::isNear(vertex.vertex, position, 0.01))
		{
			return vertex.texcoord;
		}
	}

	return std::nullopt;
}

bool hasEntityOfClass(const scene::INodePtr& root, const std::string& className)
{
	return static_cast<bool>(algorithm::findFirstEntity(root, [&](const EntityNodePtr& entity)
	{
		return entity->getEntity().getKeyValue("classname") == className;
	}));
}

}

class ValveVmfImportTest :
	public RadiantTest
{
protected:
	void openWithScale(const std::string& contents, const std::string& filename, double scale)
	{
		fs::path mapPath = _context.getTemporaryDataPath();
		mapPath /= filename;
		TemporaryFile tempFile(mapPath.string(), contents);

		auto format = GlobalMapFormatManager().getMapFormatByName("Valve VMF");
		ASSERT_TRUE(format);

		FileSelectionHelper responder(mapPath.string(), format);

		auto subscription = GlobalRadiantCore().getMessageBus().addListener(
			radiant::IMessage::Type::MapConversionRequest,
			radiant::TypeListener<radiant::MapConversionRequest>(
				[scale](radiant::MapConversionRequest& msg)
		{
			radiant::MapConversionRequest::Result result;
			result.accepted = true;
			result.scale = scale;

			msg.setResult(result);
			msg.setHandled(true);
		}));

		GlobalCommandSystem().executeCommand("OpenMap");

		GlobalRadiantCore().getMessageBus().removeListener(subscription);
	}
};

TEST_F(ValveVmfImportTest, PlacesBrushEntitiesAtTheirOrigin)
{
	auto map = world(solid({ 0, 0, 0 }, { 64, 64, 64 }, WALL)) +
		entity({ { "classname", "func_brush" }, { "targetname", "mover" }, { "origin", "96 32 32" } },
			solid({ 64, 0, 0 }, { 128, 64, 64 }, WALL));

	openWithScale(map, "vmf_origin.vmf", 1.0);

	auto mover = algorithm::getEntityByName(GlobalMapModule().getRoot(), "mover");
	ASSERT_TRUE(mover);
	EXPECT_EQ(mover->tryGetEntity()->getKeyValue("origin"), "96 32 32");

	auto brush = algorithm::findFirstBrush(mover, [](const IBrushNodePtr&) { return true; });
	ASSERT_TRUE(brush);
	EXPECT_TRUE(math::isNear(brush->worldAABB().getOrigin(), Vector3(96, 32, 32), 0.01));
	EXPECT_TRUE(math::isNear(brush->worldAABB().getExtents(), Vector3(32, 32, 32), 0.01));
}

TEST_F(ValveVmfImportTest, ScalesBrushEntityOrigins)
{
	auto map = world(solid({ 0, 0, 0 }, { 64, 64, 64 }, WALL)) +
		entity({ { "classname", "func_brush" }, { "targetname", "mover" }, { "origin", "96 32 32" } },
			solid({ 64, 0, 0 }, { 128, 64, 64 }, WALL));

	openWithScale(map, "vmf_origin_scaled.vmf", 2.0);

	auto mover = algorithm::getEntityByName(GlobalMapModule().getRoot(), "mover");
	ASSERT_TRUE(mover);
	EXPECT_EQ(mover->tryGetEntity()->getKeyValue("origin"), "192 64 64");

	auto brush = algorithm::findFirstBrush(mover, [](const IBrushNodePtr&) { return true; });
	ASSERT_TRUE(brush);
	EXPECT_TRUE(math::isNear(brush->worldAABB().getOrigin(), Vector3(192, 64, 64), 0.01));
}

TEST_F(ValveVmfImportTest, KeepsBrushEntityTexturesAlignedToTheWorld)
{
	auto map = world(solid({ 0, 0, 0 }, { 64, 64, 64 }, WALL)) +
		entity({ { "classname", "func_brush" }, { "targetname", "mover" }, { "origin", "37 13 5" } },
			solid({ 0, 0, 0 }, { 64, 64, 64 }, WALL));

	openWithScale(map, "vmf_alignment.vmf", 1.0);

	auto root = GlobalMapModule().getRoot();

	auto worldBrush = algorithm::findFirstBrush(algorithm::findWorldspawn(root), [](const IBrushNodePtr&) { return true; });
	auto entityBrush = algorithm::findFirstBrush(algorithm::getEntityByName(root, "mover"), [](const IBrushNodePtr&) { return true; });
	ASSERT_TRUE(worldBrush);
	ASSERT_TRUE(entityBrush);

	auto worldTop = algorithm::findBrushFaceWithNormal(Node_getIBrush(worldBrush), Vector3(0, 0, 1));
	auto entityTop = algorithm::findBrushFaceWithNormal(Node_getIBrush(entityBrush), Vector3(0, 0, 1));
	ASSERT_TRUE(worldTop != nullptr);
	ASSERT_TRUE(entityTop != nullptr);

	for (const auto& corner : { Vector3(0, 0, 64), Vector3(64, 64, 64) })
	{
		auto expected = textureCoordinateAt(worldTop, corner);
		auto actual = textureCoordinateAt(entityTop, corner);
		ASSERT_TRUE(expected && actual);

		auto difference = *actual - *expected;
		EXPECT_NEAR(difference.x(), std::round(difference.x()), 0.001);
		EXPECT_NEAR(difference.y(), std::round(difference.y()), 0.001);
	}
}

TEST_F(ValveVmfImportTest, ConvertsDisplacementsToPatchesThroughEveryVertex)
{
	std::string rows[3];

	for (int row = 0; row < 5; ++row)
	{
		std::string normals, distances, offsets;

		for (int col = 0; col < 5; ++col)
		{
			normals += "0 0 1 ";
			distances += std::to_string(4 * row + col) + " ";
			offsets += "0 0 0 ";
		}

		rows[0] += "\t\t\t\t\t\"row" + std::to_string(row) + "\" \"" + normals + "\"\n";
		rows[1] += "\t\t\t\t\t\"row" + std::to_string(row) + "\" \"" + distances + "\"\n";
		rows[2] += "\t\t\t\t\t\"row" + std::to_string(row) + "\" \"" + offsets + "\"\n";
	}

	std::string dispinfo = "\t\t\tdispinfo\n\t\t\t{\n"
		"\t\t\t\t\"power\" \"2\"\n\t\t\t\t\"startposition\" \"[0 0 16]\"\n\t\t\t\t\"elevation\" \"0\"\n"
		"\t\t\t\tnormals\n\t\t\t\t{\n" + rows[0] + "\t\t\t\t}\n"
		"\t\t\t\tdistances\n\t\t\t\t{\n" + rows[1] + "\t\t\t\t}\n"
		"\t\t\t\toffsets\n\t\t\t\t{\n" + rows[2] + "\t\t\t\t}\n"
		"\t\t\t}\n";

	auto map = world(solid({ 0, 0, -64 }, { 64, 64, -32 }, WALL) + solid({ 0, 0, 0 }, { 128, 128, 16 }, WALL, "", dispinfo));

	openWithScale(map, "vmf_displacement.vmf", 1.0);

	auto worldspawn = algorithm::findWorldspawn(GlobalMapModule().getRoot());
	ASSERT_TRUE(worldspawn);
	EXPECT_EQ(countBrushes(worldspawn), 1u);

	scene::INodePtr patchNode;

	worldspawn->foreachNode([&](const scene::INodePtr& node)
	{
		if (Node_isPatch(node)) patchNode = node;
		return true;
	});

	ASSERT_TRUE(patchNode);

	auto& patch = std::dynamic_pointer_cast<IPatchNode>(patchNode)->getPatch();
	EXPECT_EQ(patch.getWidth(), 5u);
	EXPECT_EQ(patch.getHeight(), 5u);

	auto mesh = patch.getTesselatedPatchMesh();

	for (int row = 0; row < 5; ++row)
	{
		for (int col = 0; col < 5; ++col)
		{
			Vector3 expected(32 * col, 32 * row, 16 + 4 * row + col);

			bool found = std::any_of(mesh.vertices.begin(), mesh.vertices.end(), [&](const VertexNT& vertex)
			{
				return math::isNear(vertex.vertex, expected, 0.01);
			});

			EXPECT_TRUE(found) << "row " << row << " col " << col;
		}
	}

	ASSERT_FALSE(mesh.vertices.empty());
	EXPECT_GT(mesh.vertices.front().normal.z(), 0);
}

TEST_F(ValveVmfImportTest, MergesInstancesWithTransformsNamesAndReplacements)
{
	fs::path instanceFolder = _context.getTemporaryDataPath();
	instanceFolder /= "instances";
	fs::create_directories(instanceFolder);

	auto instance = world(solid({ 0, 0, 0 }, { 32, 64, 16 }, WALL)) +
		entity({ { "classname", "info_target" }, { "targetname", "spot" }, { "origin", "16 0 0" },
			{ "angles", "0 0 0" }, { "message", "$text" } }) +
		entity({ { "classname", "info_target" }, { "targetname", "@global" }, { "origin", "0 0 0" } });

	TemporaryFile instanceFile((instanceFolder / "vmf_instance.vmf").string(), instance);

	auto map = world(solid({ 0, 0, -64 }, { 64, 64, -32 }, WALL)) +
		entity({ { "classname", "func_instance" }, { "targetname", "room" }, { "file", "instances\\vmf_instance.vmf" },
			{ "origin", "256 0 0" }, { "angles", "0 90 0" }, { "fixup_style", "0" }, { "replace01", "$text hello world" } });

	openWithScale(map, "vmf_instances.vmf", 1.0);

	auto root = GlobalMapModule().getRoot();

	EXPECT_FALSE(algorithm::getEntityByName(root, "room"));
	EXPECT_FALSE(hasEntityOfClass(root, "func_instance"));
	EXPECT_TRUE(algorithm::getEntityByName(root, "@global"));

	auto spot = algorithm::getEntityByName(root, "room-spot");
	ASSERT_TRUE(spot);
	EXPECT_EQ(spot->tryGetEntity()->getKeyValue("origin"), "256 16 0");
	EXPECT_EQ(spot->tryGetEntity()->getKeyValue("message"), "hello world");
	EXPECT_EQ(spot->tryGetEntity()->getKeyValue("angle"), "90");

	auto worldspawn = algorithm::findWorldspawn(root);
	ASSERT_EQ(countBrushes(worldspawn), 2u);

	auto merged = algorithm::findFirstBrush(worldspawn, [](const IBrushNodePtr& brush)
	{
		return std::dynamic_pointer_cast<scene::INode>(brush)->worldAABB().getOrigin().z() > 0;
	});
	ASSERT_TRUE(merged);
	EXPECT_TRUE(math::isNear(merged->worldAABB().getOrigin(), Vector3(224, 16, 8), 0.01));
	EXPECT_TRUE(math::isNear(merged->worldAABB().getExtents(), Vector3(32, 16, 8), 0.01));
}

TEST_F(ValveVmfImportTest, SkipsHiddenObjects)
{
	auto map = world(solid({ 0, 0, 0 }, { 64, 64, 64 }, WALL) +
			"\thidden\n\t{\n" + solid({ 128, 0, 0 }, { 192, 64, 64 }, WALL) + "\t}\n") +
		"hidden\n{\n" + entity({ { "classname", "info_target" }, { "targetname", "ghost" }, { "origin", "0 0 0" } }) + "}\n" +
		entity({ { "classname", "logic_relay" }, { "targetname", "relay" }, { "origin", "0 0 0" } },
			"\tconnections\n\t{\n\t\t\"OnTrigger\" \"door,Open,,0,-1\"\n\t}\n");

	openWithScale(map, "vmf_hidden.vmf", 1.0);

	auto root = GlobalMapModule().getRoot();

	EXPECT_EQ(countBrushes(algorithm::findWorldspawn(root)), 1u);
	EXPECT_FALSE(algorithm::getEntityByName(root, "ghost"));
	EXPECT_TRUE(algorithm::getEntityByName(root, "relay"));
}

TEST_F(ValveVmfImportTest, MovesAreaportalsToTheWorldAndDropsEditorOnlyEntities)
{
	auto map = world(solid({ 0, 0, 0 }, { 64, 64, 64 }, WALL)) +
		entity({ { "classname", "func_areaportal" }, { "portalnumber", "1" } }, solid({ 64, 0, 0 }, { 72, 64, 64 }, "TOOLS/TOOLSAREAPORTAL")) +
		entity({ { "classname", "func_occluder" } }, solid({ 128, 0, 0 }, { 136, 64, 64 }, "TOOLS/TOOLSOCCLUDER")) +
		entity({ { "classname", "env_cubemap" }, { "origin", "32 32 32" } });

	openWithScale(map, "vmf_portals.vmf", 1.0);

	auto root = GlobalMapModule().getRoot();

	EXPECT_EQ(countBrushes(algorithm::findWorldspawn(root)), 2u);

	for (const char* className : { "func_areaportal", "func_occluder", "env_cubemap" })
	{
		EXPECT_FALSE(hasEntityOfClass(root, className)) << className;
	}
}

TEST_F(ValveVmfImportTest, DropsHintSkipAndOriginBrushesFromTheWorld)
{
	auto hint = solid({ 128, 0, 0 }, { 192, 64, 64 }, "TOOLS/TOOLSSKIP");
	auto hintFace = hint.find("TOOLS/TOOLSSKIP");
	hint.replace(hintFace, std::string("TOOLS/TOOLSSKIP").size(), "TOOLS/TOOLSHINT");

	auto map = world(solid({ 0, 0, 0 }, { 64, 64, 64 }, WALL) + hint + solid({ 256, 0, 0 }, { 272, 16, 16 }, "TOOLS/TOOLSORIGIN"));

	openWithScale(map, "vmf_hint.vmf", 1.0);

	EXPECT_EQ(countBrushes(algorithm::findWorldspawn(GlobalMapModule().getRoot())), 1u);
}

TEST_F(ValveVmfImportTest, MakesNonSolidSourceEntitiesNonSolid)
{
	auto map = world(solid({ 0, 0, 0 }, { 64, 64, 64 }, WALL)) +
		entity({ { "classname", "func_brush" }, { "targetname", "ghostwall" }, { "Solidity", "1" }, { "origin", "96 32 32" } },
			solid({ 64, 0, 0 }, { 128, 64, 64 }, WALL)) +
		entity({ { "classname", "func_illusionary" }, { "targetname", "fake" }, { "origin", "160 32 32" } },
			solid({ 128, 0, 0 }, { 192, 64, 64 }, WALL)) +
		entity({ { "classname", "func_brush" }, { "targetname", "wall" }, { "Solidity", "2" }, { "origin", "224 32 32" } },
			solid({ 192, 0, 0 }, { 256, 64, 64 }, WALL));

	openWithScale(map, "vmf_solidity.vmf", 1.0);

	auto root = GlobalMapModule().getRoot();

	for (const char* name : { "ghostwall", "fake" })
	{
		auto node = algorithm::getEntityByName(root, name);
		ASSERT_TRUE(node) << name;
		EXPECT_EQ(node->tryGetEntity()->getKeyValue("solid"), "0") << name;
		EXPECT_EQ(node->tryGetEntity()->getKeyValue("Solidity"), "") << name;
	}

	auto wall = algorithm::getEntityByName(root, "wall");
	ASSERT_TRUE(wall);
	EXPECT_EQ(wall->tryGetEntity()->getKeyValue("solid"), "");
}

TEST_F(ValveVmfImportTest, ConvertsSourceLightsAndMoveDirections)
{
	auto map = world(solid({ 0, 0, 0 }, { 64, 64, 64 }, WALL)) +
		entity({ { "classname", "light" }, { "targetname", "lamp" }, { "origin", "32 32 128" },
			{ "_light", "255 128 0 200" }, { "_lightHDR", "-1 -1 -1 1" }, { "_zero_percent_distance", "300" } }) +
		entity({ { "classname", "light_spot" }, { "targetname", "spot" }, { "origin", "32 32 256" },
			{ "_light", "255 255 255 200" }, { "angles", "-90 0 0" }, { "_cone", "30" }, { "_inner_cone", "20" }, { "_distance", "400" } }) +
		entity({ { "classname", "func_door" }, { "targetname", "door" }, { "origin", "96 32 32" }, { "movedir", "-90 0 0" } },
			solid({ 64, 0, 0 }, { 128, 64, 64 }, WALL));

	openWithScale(map, "vmf_lights.vmf", 1.0);

	auto root = GlobalMapModule().getRoot();

	auto lamp = algorithm::getEntityByName(root, "lamp");
	ASSERT_TRUE(lamp);
	EXPECT_EQ(lamp->tryGetEntity()->getKeyValue("_color"), "1 0.501961 0");
	EXPECT_EQ(lamp->tryGetEntity()->getKeyValue("light_radius"), "300 300 300");
	EXPECT_EQ(lamp->tryGetEntity()->getKeyValue("_lightHDR"), "");
	EXPECT_EQ(lamp->tryGetEntity()->getKeyValue("_zero_percent_distance"), "");

	auto spot = algorithm::getEntityByName(root, "spot");
	ASSERT_TRUE(spot);

	auto target = numbers(spot->tryGetEntity()->getKeyValue("light_target"));
	auto right = numbers(spot->tryGetEntity()->getKeyValue("light_right"));
	ASSERT_EQ(target.size(), 3u);
	ASSERT_EQ(right.size(), 3u);
	EXPECT_NEAR(target[2], -400, 0.01);
	EXPECT_NEAR(Vector3(right[0], right[1], right[2]).getLength(), 400 * std::tan(30 * math::PI / 180), 0.01);
	EXPECT_EQ(spot->tryGetEntity()->getKeyValue("_inner_cone"), "");

	auto door = algorithm::getEntityByName(root, "door");
	ASSERT_TRUE(door);
	EXPECT_EQ(door->tryGetEntity()->getKeyValue("movedir"), "-1");
}

TEST_F(ValveVmfImportTest, MapsVisgroupsToLayersAndGroupsToSelectionGroups)
{
	std::string visgroups = "visgroups\n{\n\tvisgroup\n\t{\n\t\t\"name\" \"Walls\"\n\t\t\"visgroupid\" \"1\"\n"
		"\t\tvisgroup\n\t\t{\n\t\t\t\"name\" \"Doors\"\n\t\t\t\"visgroupid\" \"2\"\n\t\t}\n\t}\n}\n";

	std::string groups = "\tgroup\n\t{\n\t\t\"id\" \"4\"\n\t}\n"
		"\tgroup\n\t{\n\t\t\"id\" \"5\"\n\t\teditor\n\t\t{\n\t\t\t\"groupid\" \"4\"\n\t\t}\n\t}\n";

	auto map = world(solid({ 0, 0, 0 }, { 64, 64, 64 }, WALL, "\t\t\t\"visgroupid\" \"1\"\n\t\t\t\"groupid\" \"5\"\n") + groups, visgroups) +
		entity({ { "classname", "func_brush" }, { "targetname", "door" }, { "origin", "96 32 32" } },
			solid({ 64, 0, 0 }, { 128, 64, 64 }, WALL) + "\teditor\n\t{\n\t\t\"visgroupid\" \"2\"\n\t}\n");

	openWithScale(map, "vmf_layers.vmf", 1.0);

	auto root = GlobalMapModule().getRoot();
	auto& layers = root->getLayerManager();

	auto walls = layers.getLayerID("Walls");
	auto doors = layers.getLayerID("Doors");
	ASSERT_NE(walls, -1);
	ASSERT_NE(doors, -1);

	auto wall = algorithm::findFirstBrush(algorithm::findWorldspawn(root), [](const IBrushNodePtr&) { return true; });
	ASSERT_TRUE(wall);
	EXPECT_EQ(wall->getLayers(), scene::LayerList{ walls });
	EXPECT_EQ(std::dynamic_pointer_cast<IGroupSelectable>(wall)->getGroupIds().size(), 2u);

	auto door = algorithm::getEntityByName(root, "door");
	ASSERT_TRUE(door);
	EXPECT_EQ(door->getLayers(), scene::LayerList{ doors });
}

}
