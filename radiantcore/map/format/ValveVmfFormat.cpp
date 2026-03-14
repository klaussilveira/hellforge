#define SPECIALISE_STR_TO_FLOAT

#include "ValveVmfFormat.h"

#include "Quake3MapWriter.h"
#include "ConversionMap.h"
#include "module/StaticModule.h"
#include "string/convert.h"
#include "imap.h"
#include "ibrush.h"
#include "ishaders.h"
#include "ieclass.h"
#include "itextstream.h"
#include "parser/DefTokeniser.h"
#include "math/Matrix4.h"
#include "math/Plane3.h"
#include "math/Vector3.h"
#include "scene/EntityNode.h"
#include "shaderlib.h"
#include "texturelib.h"
#include "i18n.h"
#include <fmt/format.h>
#include <cstdio>

namespace map
{

namespace
{

std::string resolveTextureName(const std::string& rawName)
{
	const auto& mapped = ConversionMap::lookup(rawName);
	if (!mapped.empty())
	{
		return mapped;
	}

	std::string prefix = GlobalTexturePrefix_get();
	std::string fullName = prefix + rawName;

	if (GlobalMaterialManager().materialExists(fullName))
	{
		return fullName;
	}

	if (GlobalMaterialManager().materialExists(rawName))
	{
		return rawName;
	}

	return prefix + texdef_name_default();
}

std::pair<float, float> getTextureDimensions(const std::string& shader)
{
	float w = 128;
	float h = 128;

	auto texture = GlobalMaterialManager().getMaterial(shader)->getEditorImage();

	if (texture)
	{
		w = static_cast<float>(texture->getWidth());
		h = static_cast<float>(texture->getHeight());
	}

	if (w == 0) w = 128;
	if (h == 0) h = 128;

	return { w, h };
}

Plane3 parsePlaneString(const std::string& str)
{
	double x1, y1, z1, x2, y2, z2, x3, y3, z3;

	if (std::sscanf(str.c_str(), "(%lf %lf %lf) (%lf %lf %lf) (%lf %lf %lf)",
		&x1, &y1, &z1, &x2, &y2, &z2, &x3, &y3, &z3) != 9)
	{
		throw parser::ParseException("VMF: failed to parse plane: " + str);
	}

	Vector3 p1(x1, y1, z1);
	Vector3 p2(x2, y2, z2);
	Vector3 p3(x3, y3, z3);

	return Plane3(p3, p2, p1);
}

void parseAxisString(const std::string& str, Vector3& axis, double& shift, double& scale)
{
	double ax, ay, az;

	if (std::sscanf(str.c_str(), "[%lf %lf %lf %lf] %lf",
		&ax, &ay, &az, &shift, &scale) != 5)
	{
		throw parser::ParseException("VMF: failed to parse axis: " + str);
	}

	axis = Vector3(ax, ay, az);
}

Matrix3 calculateTextureMatrix(const std::string& shader, const Vector3& normal,
	const Vector3& uAxis, const Vector3& vAxis,
	double shiftU, double shiftV, double scaleU, double scaleV)
{
	auto [imageWidth, imageHeight] = getTextureDimensions(shader);

	auto transform = Matrix4::getIdentity();

	transform.xx() = uAxis.x() / scaleU / imageWidth;
	transform.yx() = uAxis.y() / scaleU / imageWidth;
	transform.zx() = uAxis.z() / scaleU / imageWidth;

	transform.xy() = vAxis.x() / scaleV / imageHeight;
	transform.yy() = vAxis.y() / scaleV / imageHeight;
	transform.zy() = vAxis.z() / scaleV / imageHeight;

	transform.tx() = shiftU / imageWidth;
	transform.ty() = shiftV / imageHeight;

	auto axisBase = getBasisTransformForNormal(normal);
	transform.multiplyBy(axisBase.getTransposed());

	return getTextureMatrixFromMatrix4(transform);
}

}

// Reader

ValveVmfReader::ValveVmfReader(IMapImportFilter& importFilter) :
	_importFilter(importFilter),
	_entityCount(0)
{}

void ValveVmfReader::readFromStream(std::istream& stream)
{
	parser::BasicDefTokeniser<std::istream> tok(stream);

	while (tok.hasMoreTokens())
	{
		std::string token = tok.nextToken();

		if (token == "world")
		{
			try
			{
				tok.assertNextToken("{");
				parseWorldOrEntity(tok, true);
				_entityCount++;
			}
			catch (FailureException& e)
			{
				std::string text = fmt::format(_("Failed parsing world entity:\n{0}"), e.what());
				throw FailureException(text);
			}
		}
		else if (token == "entity")
		{
			try
			{
				tok.assertNextToken("{");
				parseWorldOrEntity(tok, false);
				_entityCount++;
			}
			catch (FailureException& e)
			{
				std::string text = fmt::format(_("Failed parsing entity {0:d}:\n{1}"), _entityCount, e.what());
				throw FailureException(text);
			}
		}
		else if (token == "{")
		{
			skipBlock(tok);
		}
		else
		{
			if (tok.hasMoreTokens() && tok.peek() == "{")
			{
				tok.nextToken();
				skipBlock(tok);
			}
		}
	}
}

void ValveVmfReader::skipBlock(parser::DefTokeniser& tok)
{
	std::size_t depth = 1;
	while (depth > 0 && tok.hasMoreTokens())
	{
		std::string token = tok.nextToken();
		if (token == "{") depth++;
		else if (token == "}") depth--;
	}
}

void ValveVmfReader::parseWorldOrEntity(parser::DefTokeniser& tok, bool isWorld)
{
	EntityKeyValues keyValues;
	scene::INodePtr entity;
	std::size_t primitiveCount = 0;

	while (tok.hasMoreTokens())
	{
		std::string token = tok.nextToken();

		if (token == "}")
		{
			if (entity == nullptr)
			{
				if (isWorld)
				{
					keyValues["classname"] = "worldspawn";
				}
				entity = createEntity(keyValues);
			}
			break;
		}
		else if (tok.hasMoreTokens() && tok.peek() == "{")
		{
			tok.nextToken();

			if (token == "solid")
			{
				if (entity == nullptr)
				{
					if (isWorld)
					{
						keyValues["classname"] = "worldspawn";
					}
					entity = createEntity(keyValues);
				}

				try
				{
					primitiveCount++;
					scene::INodePtr brush = parseSolid(tok);

					if (brush)
					{
						_importFilter.addPrimitiveToEntity(brush, entity);
					}
				}
				catch (parser::ParseException& e)
				{
					std::string text = fmt::format(_("Primitive #{0:d}: parse exception {1}"), primitiveCount, e.what());
					throw FailureException(text);
				}
			}
			else
			{
				skipBlock(tok);
			}
		}
		else
		{
			std::string value = tok.nextToken();
			if (token != "id")
			{
				keyValues[token] = value;
			}
		}
	}

	if (entity)
	{
		_importFilter.addEntity(entity);
	}
}

scene::INodePtr ValveVmfReader::parseSolid(parser::DefTokeniser& tok)
{
	scene::INodePtr node = GlobalBrushCreator().createBrush();
	IBrushNodePtr brushNode = std::dynamic_pointer_cast<IBrushNode>(node);
	assert(brushNode != NULL);

	IBrush& brush = brushNode->getIBrush();

	while (tok.hasMoreTokens())
	{
		std::string token = tok.nextToken();

		if (token == "}")
		{
			break;
		}
		else if (tok.hasMoreTokens() && tok.peek() == "{")
		{
			tok.nextToken();

			if (token == "side")
			{
				std::string planeStr;
				std::string material;
				std::string uaxisStr;
				std::string vaxisStr;

				while (tok.hasMoreTokens())
				{
					std::string key = tok.nextToken();

					if (key == "}")
					{
						break;
					}
					else if (tok.hasMoreTokens() && tok.peek() == "{")
					{
						tok.nextToken();
						skipBlock(tok);
					}
					else
					{
						std::string value = tok.nextToken();

						if (key == "plane") planeStr = value;
						else if (key == "material") material = value;
						else if (key == "uaxis") uaxisStr = value;
						else if (key == "vaxis") vaxisStr = value;
					}
				}

				if (!planeStr.empty() && !uaxisStr.empty() && !vaxisStr.empty())
				{
					Plane3 plane = parsePlaneString(planeStr);

					std::string shader = resolveTextureName(material);

					Vector3 uAxis, vAxis;
					double shiftU, shiftV, scaleU, scaleV;

					parseAxisString(uaxisStr, uAxis, shiftU, scaleU);
					parseAxisString(vaxisStr, vAxis, shiftV, scaleV);

					if (scaleU == 0) scaleU = 1;
					if (scaleV == 0) scaleV = 1;

					auto texdef = calculateTextureMatrix(shader, plane.normal(),
						uAxis, vAxis, shiftU, shiftV, scaleU, scaleV);

					brush.addFace(plane, texdef, shader);
				}
			}
			else
			{
				skipBlock(tok);
			}
		}
		else if (tok.hasMoreTokens())
		{
			tok.nextToken();
		}
	}

	brush.removeRedundantFaces();
	return node;
}

scene::INodePtr ValveVmfReader::createEntity(const EntityKeyValues& keyValues)
{
	EntityKeyValues::const_iterator found = keyValues.find("classname");

	if (found == keyValues.end())
	{
		throw FailureException("ValveVmfReader::createEntity(): could not find classname.");
	}

	std::string className = found->second;
	scene::EntityClass::Ptr classPtr = GlobalEntityClassManager().findClass(className);

	if (classPtr == NULL)
	{
		rError() << "[vmf]: Could not find entity class: "
			<< className << std::endl;

		classPtr = GlobalEntityClassManager().findOrInsert(className, true);
	}

	EntityNodePtr node(GlobalEntityModule().createEntity(classPtr));

	for (EntityKeyValues::const_iterator i = keyValues.begin();
		i != keyValues.end();
		++i)
	{
		node->getEntity().setKeyValue(i->first, i->second);
	}

	return node;
}

// Format

const std::string& ValveVmfFormat::getMapFormatName() const
{
	static std::string _name = "Valve VMF";
	return _name;
}

const std::string& ValveVmfFormat::getGameType() const
{
	static std::string _gameType = "valvevmf";
	return _gameType;
}

std::string ValveVmfFormat::getName() const
{
	return "ValveVmfLoader";
}

StringSet ValveVmfFormat::getDependencies() const
{
	static StringSet _dependencies;
	if (_dependencies.empty())
	{
		_dependencies.insert(MODULE_MAPFORMATMANAGER);
	}
	return _dependencies;
}

void ValveVmfFormat::initialiseModule(const IApplicationContext& ctx)
{
	GlobalMapFormatManager().registerMapFormat("vmf", shared_from_this());
}

void ValveVmfFormat::shutdownModule()
{
	GlobalMapFormatManager().unregisterMapFormat(shared_from_this());
}

IMapReaderPtr ValveVmfFormat::getMapReader(IMapImportFilter& filter) const
{
	return std::make_shared<ValveVmfReader>(filter);
}

IMapWriterPtr ValveVmfFormat::getMapWriter() const
{
	return std::make_shared<Quake3MapWriter>();
}

bool ValveVmfFormat::allowInfoFileCreation() const
{
	return false;
}

bool ValveVmfFormat::canLoad(std::istream& stream) const
{
	return false;
}

module::StaticModuleRegistration<ValveVmfFormat> vmfMapModule;

} // namespace map
