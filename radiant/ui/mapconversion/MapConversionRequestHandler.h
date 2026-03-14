#pragma once

#include "iradiant.h"
#include "messages/MapConversionRequest.h"
#include "MapConversionDialog.h"

namespace ui
{

class MapConversionRequestHandler
{
private:
	std::size_t _msgSubscription;

public:
	MapConversionRequestHandler()
	{
		_msgSubscription = GlobalRadiantCore().getMessageBus().addListener(
			radiant::IMessage::Type::MapConversionRequest,
			radiant::TypeListener<radiant::MapConversionRequest>(
				sigc::mem_fun(this, &MapConversionRequestHandler::handleRequest)));
	}

	~MapConversionRequestHandler()
	{
		GlobalRadiantCore().getMessageBus().removeListener(_msgSubscription);
	}

private:
	void handleRequest(radiant::MapConversionRequest& request)
	{
		auto result = MapConversionDialog::RunDialog(
			nullptr,
			request.getFormatName(),
			request.getSourceTextures(),
			request.getSourceEntities());

		radiant::MapConversionRequest::Result msgResult;
		msgResult.accepted = !result.textureMappings.empty() || !result.entityMappings.empty();
		msgResult.textureMappings = std::move(result.textureMappings);
		msgResult.entityMappings = std::move(result.entityMappings);

		request.setResult(msgResult);
		request.setHandled(true);
	}
};

} // namespace ui
