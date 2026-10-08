#pragma once

#include "imapformat.h"
#include "math/Matrix3.h"
#include "math/Vector3.h"

namespace map
{

std::string resolveTextureName(const std::string& rawName);

std::pair<float, float> getTextureDimensions(const std::string& shader);

class Quake1BrushDefParser :
	public PrimitiveParser
{
public:
	const std::string& getKeyword() const override;
	scene::INodePtr parse(parser::DefTokeniser& tok) const override;
};

class Valve220BrushDefParser :
	public PrimitiveParser
{
public:
	const std::string& getKeyword() const override;
	scene::INodePtr parse(parser::DefTokeniser& tok) const override;

	static Matrix3 calculateTextureMatrix(double imageWidth, double imageHeight, const Vector3& normal,
		const Vector3& uAxis, const Vector3& vAxis,
		double shiftU, double shiftV, double scaleU, double scaleV);

private:
	static Matrix3 calculateTextureMatrix(const std::string& shader, const Vector3& normal,
		const Vector3& uAxis, const Vector3& vAxis,
		double shiftU, double shiftV, double scaleU, double scaleV);
};

class Quake2BrushDefParser :
	public PrimitiveParser
{
public:
	const std::string& getKeyword() const override;
	scene::INodePtr parse(parser::DefTokeniser& tok) const override;
};

}
