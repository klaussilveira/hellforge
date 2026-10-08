#include "ConversionMap.h"

#include "math/Vector3.h"

#include <fmt/format.h>
#include <set>
#include <sstream>

namespace map
{

namespace
{

const std::map<std::string, double> SOURCE_EYE_HEIGHTS = {
	{ "Quake 1", 46.0 },
	{ "Quake 2", 46.0 },
	{ "Quake 3", 50.0 },
	{ "Valve 220", 64.0 },
	{ "Valve VMF", 64.0 },
	{ "Worldcraft RMF", 64.0 },
	{ "Jackhammer JMF", 64.0 },
};

const std::set<std::string> SPATIAL_KEYS = {
	"origin", "light_radius", "light_center"
};

const std::set<std::string> SCALAR_KEYS = {
	"lip", "height"
};

}

std::map<std::string, std::string> ConversionMap::_textureMap;
std::map<std::string, std::pair<double, double>> ConversionMap::_textureSizes;
double ConversionMap::_scale = 1.0;

double ConversionMap::getDefaultScale(const std::string& formatName)
{
	auto found = SOURCE_EYE_HEIGHTS.find(formatName);

	if (found == SOURCE_EYE_HEIGHTS.end() || found->second <= 0)
	{
		return 1.0;
	}

	return TARGET_EYE_HEIGHT / found->second;
}

std::string ConversionMap::scaleSpatialValue(const std::string& key, const std::string& value)
{
	if (_scale == 1.0)
	{
		return value;
	}

	if (SCALAR_KEYS.find(key) != SCALAR_KEYS.end())
	{
		std::stringstream scalarStream(value);
		double scalar;

		scalarStream >> scalar;

		return scalarStream.fail() ? value : fmt::format("{0:g}", scalar * _scale);
	}

	if (SPATIAL_KEYS.find(key) == SPATIAL_KEYS.end())
	{
		return value;
	}

	std::stringstream stream(value);
	Vector3 parsed;

	stream >> parsed;

	if (stream.fail())
	{
		return value;
	}

	return fmt::format("{0:g} {1:g} {2:g}",
		parsed.x() * _scale, parsed.y() * _scale, parsed.z() * _scale);
}

}
