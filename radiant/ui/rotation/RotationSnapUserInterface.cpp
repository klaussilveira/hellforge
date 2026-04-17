#include "RotationSnapUserInterface.h"

#include "ui/ieventmanager.h"
#include "registry/registry.h"
#include "module/StaticModule.h"

#include <cmath>
#include <functional>
#include <string>

namespace ui
{

namespace
{
	const char* const RKEY_ROTATION_SNAP_DEGREES = "user/ui/rotation/snapDegrees";
	constexpr float DEFAULT_ROTATION_SNAP_DEGREES = 15.0f;

	const float SNAP_OPTIONS[] = { 0.0f, 1.0f, 5.0f, 10.0f, 15.0f, 30.0f, 45.0f, 90.0f };

	inline std::string toggleNameFor(float degrees)
	{
		int intPart = static_cast<int>(degrees);
		return "SetRotationSnap" + std::to_string(intPart);
	}
}

std::string RotationSnapUserInterface::getName() const
{
	static std::string _name("RotationSnapUserInterface");
	return _name;
}

StringSet RotationSnapUserInterface::getDependencies() const
{
	static StringSet _dependencies;

	if (_dependencies.empty())
	{
		_dependencies.insert(MODULE_EVENTMANAGER);
		_dependencies.insert(MODULE_XMLREGISTRY);
	}

	return _dependencies;
}

void RotationSnapUserInterface::initialiseModule(const IApplicationContext& ctx)
{
	registry::setDefault(RKEY_ROTATION_SNAP_DEGREES, DEFAULT_ROTATION_SNAP_DEGREES);

	for (float degrees : SNAP_OPTIONS)
	{
		std::string toggleName = toggleNameFor(degrees);
		GlobalEventManager().addToggle(toggleName,
			std::bind(&RotationSnapUserInterface::onToggle, this, degrees, std::placeholders::_1));

		_toggleItemNames.emplace(degrees, toggleName);
	}

	_registryConn = GlobalRegistry().signalForKey(RKEY_ROTATION_SNAP_DEGREES).connect(
		sigc::mem_fun(*this, &RotationSnapUserInterface::updateToggleStates)
	);

	updateToggleStates();
}

void RotationSnapUserInterface::shutdownModule()
{
	_registryConn.disconnect();
}

void RotationSnapUserInterface::onToggle(float degrees, bool /*newState*/)
{
	registry::setValue(RKEY_ROTATION_SNAP_DEGREES, degrees);
}

void RotationSnapUserInterface::updateToggleStates()
{
	float current = registry::getValue<float>(RKEY_ROTATION_SNAP_DEGREES, DEFAULT_ROTATION_SNAP_DEGREES);

	for (const auto& item : _toggleItemNames)
	{
		GlobalEventManager().setToggled(item.second, std::abs(item.first - current) < 0.01f);
	}
}

module::StaticModuleRegistration<RotationSnapUserInterface> rotationSnapUiModule;

}
