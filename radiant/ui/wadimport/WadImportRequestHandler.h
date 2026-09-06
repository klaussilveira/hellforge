#pragma once

#include "iradiant.h"
#include "messages/WadImportRequest.h"
#include "WadImportDialog.h"

namespace ui
{

class WadImportRequestHandler
{
private:
	std::size_t _msgSubscription;

public:
	WadImportRequestHandler()
	{
		_msgSubscription = GlobalRadiantCore().getMessageBus().addListener(
			radiant::IMessage::Type::WadImportRequest,
			radiant::TypeListener<radiant::WadImportRequest>(
				sigc::mem_fun(this, &WadImportRequestHandler::handleRequest)));
	}

	~WadImportRequestHandler()
	{
		GlobalRadiantCore().getMessageBus().removeListener(_msgSubscription);
	}

private:
	void handleRequest(radiant::WadImportRequest& request)
	{
		auto options = WadImportDialog::RunDialog(nullptr, request.getWadPath(),
			request.getMapNames(), request.getDefaultScale(), request.getDefaultLightSpacing());

		radiant::WadImportRequest::Result result;
		result.accepted = options.accepted;
		result.mapName = options.mapName;
		result.scale = options.scale;
		result.lightSpacing = options.lightSpacing;

		request.setResult(result);
		request.setHandled(true);
	}
};

}
