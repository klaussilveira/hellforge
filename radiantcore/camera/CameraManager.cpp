#include "CameraManager.h"

#include "module/StaticModule.h"
#include "Camera.h"
#include "command/ExecutionFailure.h"

namespace camera
{

std::string CameraManager::getName() const
{
	static std::string _name(MODULE_CAMERA_MANAGER);
	return _name;
}

StringSet CameraManager::getDependencies() const
{
	static StringSet _dependencies { MODULE_COMMANDSYSTEM };
	return _dependencies;
}

void CameraManager::initialiseModule(const IApplicationContext& ctx)
{
	GlobalCommandSystem().addCommand("SetActiveCameraPosition",
		std::bind(&CameraManager::setActiveCameraPosition, this, std::placeholders::_1), { cmd::ARGTYPE_VECTOR3 });
	GlobalCommandSystem().addCommand("SetActiveCameraAngles",
		std::bind(&CameraManager::setActiveCameraAngles, this, std::placeholders::_1), { cmd::ARGTYPE_VECTOR3 });
}

ICameraView::Ptr CameraManager::createCamera(render::IRenderView& view, const std::function<void(bool)>& requestRedraw)
{
	_cameras.emplace_back(std::make_shared<Camera>(view, requestRedraw));
	_activeCamera = _cameras.back();
	return _cameras.back();
}

void CameraManager::destroyCamera(const ICameraView::Ptr& camera)
{
	_cameras.remove(camera);
	if (_activeCamera == camera)
	{
		_activeCamera = _cameras.empty() ? nullptr : _cameras.back();
	}
}

void CameraManager::focusAllCameras(const Vector3& position, const Vector3& angles)
{
	for (const auto& camera : _cameras)
	{
		camera->setOriginAndAngles(position, angles);
	}
}

camera::ICameraView& CameraManager::getActiveView()
{
	if (_cameras.empty())
	{
		throw std::runtime_error("No active camera view present");
	}

	if (_activeCamera)
	{
		return *_activeCamera;
	}

	return *_cameras.front();
}

sigc::signal<void>& CameraManager::signal_cameraChanged()
{
	return _sigCameraChanged;
}

void CameraManager::onCameraViewChanged()
{
	_sigCameraChanged.emit();
}

CameraManager& CameraManager::GetInstanceInternal()
{
	return *std::static_pointer_cast<CameraManager>(
		module::GlobalModuleRegistry().getModule(MODULE_CAMERA_MANAGER)
	);
}

void CameraManager::setActiveCameraPosition(const cmd::ArgumentList& args)
{
	try
	{
		if (args.size() != 1)
		{
			rWarning() << "Usage: SetActiveCameraPosition <position:Vector3>" << std::endl;
			return;
		}

		auto& camera = getActiveView();
		camera.setCameraOrigin(args[0].getVector3());
	}
	catch (std::runtime_error & ex)
	{
		throw cmd::ExecutionFailure(ex.what());
	}
}

void CameraManager::setActiveCameraAngles(const cmd::ArgumentList& args)
{
	try
	{
		if (args.size() != 1)
		{
			rWarning() << "Usage: SetActiveCameraAngles <PitchYawRoll:Vector3>" << std::endl;
			return;
		}

		auto& camera = getActiveView();
		camera.setCameraAngles(args[0].getVector3());
	}
	catch (std::runtime_error & ex)
	{
		throw cmd::ExecutionFailure(ex.what());
	}
}

module::StaticModuleRegistration<CameraManager> cameraManagerModule;

}
