#define SPECIALISE_STR_TO_FLOAT

#include "Quake1BrushDef.h"

#include "../Quake3Utils.h"
#include "string/convert.h"
#include "imap.h"
#include "ibrush.h"
#include "ishaders.h"
#include "parser/DefTokeniser.h"
#include "math/Matrix4.h"
#include "math/Plane3.h"
#include "math/Vector3.h"
#include "shaderlib.h"
#include "texturelib.h"
#include "i18n.h"
#include <fmt/format.h>

namespace map
{

namespace
{

Vector3 parsePoint(parser::DefTokeniser& tok)
{
	double x = string::to_float(tok.nextToken());
	double y = string::to_float(tok.nextToken());
	double z = string::to_float(tok.nextToken());
	return Vector3(x, y, z);
}

Plane3 parsePlane(parser::DefTokeniser& tok)
{
	Vector3 p1 = parsePoint(tok);
	tok.assertNextToken(")");
	tok.assertNextToken("(");

	Vector3 p2 = parsePoint(tok);
	tok.assertNextToken(")");
	tok.assertNextToken("(");

	Vector3 p3 = parsePoint(tok);
	tok.assertNextToken(")");

	return Plane3(p3, p2, p1);
}

std::string resolveTextureName(const std::string& rawName)
{
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

Matrix3 ssrToTextureMatrix(const std::string& shader, const Vector3& normal, const ShiftScaleRotation& ssr)
{
	auto [imageWidth, imageHeight] = getTextureDimensions(shader);

	auto transform = quake3::calculateTextureMatrix(normal, ssr, imageWidth, imageHeight);

	auto axisBase = getBasisTransformForNormal(normal);
	transform.multiplyBy(axisBase.getTransposed());

	return getTextureMatrixFromMatrix4(transform);
}

}

// Quake 1 standard format
const std::string& Quake1BrushDefParser::getKeyword() const
{
	static std::string _keyword("(");
	return _keyword;
}

scene::INodePtr Quake1BrushDefParser::parse(parser::DefTokeniser& tok) const
{
	scene::INodePtr node = GlobalBrushCreator().createBrush();
	IBrushNodePtr brushNode = std::dynamic_pointer_cast<IBrushNode>(node);
	assert(brushNode != NULL);

	IBrush& brush = brushNode->getIBrush();

	while (1)
	{
		std::string token = tok.nextToken();

		if (token == "}")
		{
			break;
		}
		else if (token == "(")
		{
			Plane3 plane = parsePlane(tok);

			std::string shader = resolveTextureName(tok.nextToken());

			ShiftScaleRotation ssr;
			ssr.shift[0] = string::to_float(tok.nextToken());
			ssr.shift[1] = string::to_float(tok.nextToken());
			ssr.rotate = string::to_float(tok.nextToken());
			ssr.scale[0] = string::to_float(tok.nextToken());
			ssr.scale[1] = string::to_float(tok.nextToken());

			if (ssr.scale[0] == 0) ssr.scale[0] = 1;
			if (ssr.scale[1] == 0) ssr.scale[1] = 1;

			auto texdef = ssrToTextureMatrix(shader, plane.normal(), ssr);

			brush.addFace(plane, texdef, shader);
		}
		else
		{
			std::string text = fmt::format(_("Quake1BrushDefParser: invalid token '{0}'"), token);
			throw parser::ParseException(text);
		}
	}

	brush.removeRedundantFaces();
	return node;
}

// Valve 220 format
const std::string& Valve220BrushDefParser::getKeyword() const
{
	static std::string _keyword("(");
	return _keyword;
}

scene::INodePtr Valve220BrushDefParser::parse(parser::DefTokeniser& tok) const
{
	scene::INodePtr node = GlobalBrushCreator().createBrush();
	IBrushNodePtr brushNode = std::dynamic_pointer_cast<IBrushNode>(node);
	assert(brushNode != NULL);

	IBrush& brush = brushNode->getIBrush();

	while (1)
	{
		std::string token = tok.nextToken();

		if (token == "}")
		{
			break;
		}
		else if (token == "(")
		{
			Plane3 plane = parsePlane(tok);

			std::string shader = resolveTextureName(tok.nextToken());

			tok.assertNextToken("[");
			double ux = string::to_float(tok.nextToken());
			double uy = string::to_float(tok.nextToken());
			double uz = string::to_float(tok.nextToken());
			double shiftU = string::to_float(tok.nextToken());
			tok.assertNextToken("]");

			tok.assertNextToken("[");
			double vx = string::to_float(tok.nextToken());
			double vy = string::to_float(tok.nextToken());
			double vz = string::to_float(tok.nextToken());
			double shiftV = string::to_float(tok.nextToken());
			tok.assertNextToken("]");

			tok.nextToken(); // rotation (not used in Valve 220 projection)

			double scaleU = string::to_float(tok.nextToken());
			double scaleV = string::to_float(tok.nextToken());

			if (scaleU == 0) scaleU = 1;
			if (scaleV == 0) scaleV = 1;

			Vector3 uAxis(ux, uy, uz);
			Vector3 vAxis(vx, vy, vz);

			auto texdef = calculateTextureMatrix(shader, plane.normal(),
				uAxis, vAxis, shiftU, shiftV, scaleU, scaleV);

			brush.addFace(plane, texdef, shader);
		}
		else
		{
			std::string text = fmt::format(_("Valve220BrushDefParser: invalid token '{0}'"), token);
			throw parser::ParseException(text);
		}
	}

	brush.removeRedundantFaces();
	return node;
}

Matrix3 Valve220BrushDefParser::calculateTextureMatrix(const std::string& shader, const Vector3& normal,
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

// Quake 2 standard format (same as Q1 but with 3 trailing surface flags)
const std::string& Quake2BrushDefParser::getKeyword() const
{
	static std::string _keyword("(");
	return _keyword;
}

scene::INodePtr Quake2BrushDefParser::parse(parser::DefTokeniser& tok) const
{
	scene::INodePtr node = GlobalBrushCreator().createBrush();
	IBrushNodePtr brushNode = std::dynamic_pointer_cast<IBrushNode>(node);
	assert(brushNode != NULL);

	IBrush& brush = brushNode->getIBrush();

	while (1)
	{
		std::string token = tok.nextToken();

		if (token == "}")
		{
			break;
		}
		else if (token == "(")
		{
			Plane3 plane = parsePlane(tok);

			std::string shader = resolveTextureName(tok.nextToken());

			ShiftScaleRotation ssr;
			ssr.shift[0] = string::to_float(tok.nextToken());
			ssr.shift[1] = string::to_float(tok.nextToken());
			ssr.rotate = string::to_float(tok.nextToken());
			ssr.scale[0] = string::to_float(tok.nextToken());
			ssr.scale[1] = string::to_float(tok.nextToken());

			if (ssr.scale[0] == 0) ssr.scale[0] = 1;
			if (ssr.scale[1] == 0) ssr.scale[1] = 1;

			std::string next = tok.peek();
			if (next != "(" && next != "}")
			{
				tok.skipTokens(3);
			}

			auto texdef = ssrToTextureMatrix(shader, plane.normal(), ssr);

			brush.addFace(plane, texdef, shader);
		}
		else
		{
			std::string text = fmt::format(_("Quake2BrushDefParser: invalid token '{0}'"), token);
			throw parser::ParseException(text);
		}
	}

	brush.removeRedundantFaces();
	return node;
}

} // namespace map
