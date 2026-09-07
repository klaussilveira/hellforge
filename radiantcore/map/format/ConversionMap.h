#pragma once

#include <map>
#include <string>

namespace map
{

class ConversionMap
{
	static std::map<std::string, std::string> _textureMap;
	static double _scale;

public:
	static constexpr double TARGET_EYE_HEIGHT = 68.0;

	static void set(const std::map<std::string, std::string>& mapping)
	{
		_textureMap = mapping;
	}

	static void clear()
	{
		_textureMap.clear();
		_scale = 1.0;
	}

	static const std::string& lookup(const std::string& name)
	{
		static const std::string empty;
		auto it = _textureMap.find(name);
		return (it != _textureMap.end()) ? it->second : empty;
	}

	static bool hasMapping()
	{
		return !_textureMap.empty();
	}

	static void setScale(double scale)
	{
		_scale = scale;
	}

	static double getScale()
	{
		return _scale;
	}

	static double getDefaultScale(const std::string& formatName);

	static std::string scaleSpatialValue(const std::string& key, const std::string& value);
};

}
