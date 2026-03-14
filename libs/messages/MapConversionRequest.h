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
		std::map<std::string, std::string> textureMappings;
		std::map<std::string, std::string> entityMappings;
	};

private:
	std::string _formatName;
	std::set<std::string> _sourceTextures;
	std::set<std::string> _sourceEntities;
	Result _result;

public:
	MapConversionRequest(const std::string& formatName,
						 const std::set<std::string>& sourceTextures,
						 const std::set<std::string>& sourceEntities) :
		_formatName(formatName),
		_sourceTextures(sourceTextures),
		_sourceEntities(sourceEntities)
	{}

	std::size_t getId() const override
	{
		return IMessage::Type::MapConversionRequest;
	}

	const std::string& getFormatName() const { return _formatName; }
	const std::set<std::string>& getSourceTextures() const { return _sourceTextures; }
	const std::set<std::string>& getSourceEntities() const { return _sourceEntities; }

	const Result& getResult() const { return _result; }
	void setResult(const Result& result) { _result = result; }
};

}
