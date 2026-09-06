#pragma once

#include <string>

namespace map
{

class WadImportSettings
{
	static std::string _mapName;
	static double _scale;
	static double _lightSpacing;
	static bool _cancelled;

public:
	static constexpr double DEFAULT_SCALE = 68.0 / 41.0;

	static constexpr double DEFAULT_LIGHT_SPACING = 256.0;

	static void set(const std::string& mapName, double scale, double lightSpacing)
	{
		_mapName = mapName;
		_scale = scale;
		_lightSpacing = lightSpacing;
		_cancelled = false;
	}

	static void cancel()
	{
		clear();
		_cancelled = true;
	}

	static bool wasCancelled()
	{
		return _cancelled;
	}

	static void clear()
	{
		_mapName.clear();
		_scale = DEFAULT_SCALE;
		_lightSpacing = DEFAULT_LIGHT_SPACING;
		_cancelled = false;
	}

	static const std::string& getMapName()
	{
		return _mapName;
	}

	static double getScale()
	{
		return _scale;
	}

	static double getLightSpacing()
	{
		return _lightSpacing;
	}
};

}
