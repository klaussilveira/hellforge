#pragma once

#include "imessagebus.h"

#include <string>
#include <vector>

namespace radiant
{

class WadImportRequest :
	public radiant::IMessage
{
public:
	struct Result
	{
		bool accepted = false;
		std::string mapName;
		double scale = 1.0;
		double lightSpacing = 0;
	};

private:
	std::string _wadPath;
	std::vector<std::string> _mapNames;
	double _defaultScale;
	double _defaultLightSpacing;
	Result _result;

public:
	WadImportRequest(const std::string& wadPath, const std::vector<std::string>& mapNames,
					 double defaultScale, double defaultLightSpacing) :
		_wadPath(wadPath),
		_mapNames(mapNames),
		_defaultScale(defaultScale),
		_defaultLightSpacing(defaultLightSpacing)
	{}

	std::size_t getId() const override
	{
		return IMessage::Type::WadImportRequest;
	}

	const std::string& getWadPath() const { return _wadPath; }
	const std::vector<std::string>& getMapNames() const { return _mapNames; }

	double getDefaultScale() const { return _defaultScale; }
	double getDefaultLightSpacing() const { return _defaultLightSpacing; }

	const Result& getResult() const { return _result; }
	void setResult(const Result& result) { _result = result; }
};

}
