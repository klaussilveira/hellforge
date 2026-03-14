#pragma once

#include <map>
#include <string>

namespace map
{

class ConversionMap
{
	static std::map<std::string, std::string> _textureMap;

public:
	static void set(const std::map<std::string, std::string>& mapping)
	{
		_textureMap = mapping;
	}

	static void clear()
	{
		_textureMap.clear();
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
};

}
