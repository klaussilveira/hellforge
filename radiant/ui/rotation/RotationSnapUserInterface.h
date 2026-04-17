#pragma once

#include "imodule.h"
#include <sigc++/connection.h>
#include <map>
#include <string>

namespace ui
{

class RotationSnapUserInterface :
	public RegisterableModule
{
private:
	std::map<float, std::string> _toggleItemNames;
	sigc::connection _registryConn;

public:
	std::string getName() const override;
	StringSet getDependencies() const override;
	void initialiseModule(const IApplicationContext& ctx) override;
	void shutdownModule() override;

private:
	void onToggle(float degrees, bool newState);
	void updateToggleStates();
};

}
