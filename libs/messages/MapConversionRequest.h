#pragma once

#include "imessagebus.h"

#include <map>
#include <set>
#include <string>

namespace radiant
{

class MapConversionRequest : public radiant::IMessage
{
public:
	struct Result
	{
		bool accepted = false;
		double scale = 1.0;
		std::map<std::string, std::string> textureMappings;
		std::map<std::string, std::string> entityMappings;
		std::set<std::string> entitiesToSkip;
	};

private:
	std::string _formatName;
	std::set<std::string> _sourceTextures;
	std::set<std::string> _sourceEntities;
	double _defaultScale;
	Result _result;

public:
	MapConversionRequest(const std::string& formatName,
						 const std::set<std::string>& sourceTextures,
						 const std::set<std::string>& sourceEntities,
						 double defaultScale) :
		_formatName(formatName),
		_sourceTextures(sourceTextures),
		_sourceEntities(sourceEntities),
		_defaultScale(defaultScale)
	{}

	std::size_t getId() const override
	{
		return IMessage::Type::MapConversionRequest;
	}

	const std::string& getFormatName() const { return _formatName; }
	const std::set<std::string>& getSourceTextures() const { return _sourceTextures; }
	const std::set<std::string>& getSourceEntities() const { return _sourceEntities; }

	double getDefaultScale() const { return _defaultScale; }

	const Result& getResult() const { return _result; }
	void setResult(const Result& result) { _result = result; }
};

}
