#pragma once

#include <map>
#include <string>
#include <utility>

namespace map
{

class ConversionMap
{
	static std::map<std::string, std::string> _textureMap;
	static std::map<std::string, std::pair<double, double>> _textureSizes;
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
		_textureSizes.clear();
		_scale = 1.0;
	}

	static void setTextureSizes(const std::map<std::string, std::pair<double, double>>& sizes)
	{
		_textureSizes = sizes;
	}

	static const std::pair<double, double>* findTextureSize(const std::string& upperCaseName)
	{
		auto it = _textureSizes.find(upperCaseName);
		return (it != _textureSizes.end()) ? &it->second : nullptr;
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
