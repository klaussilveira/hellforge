#include "TestOrthoViewManager.h"

#include "math/Vector3.h"

namespace test
{

void TestOrthoViewManager::updateAllViews(bool force)
{}

void TestOrthoViewManager::setOrigin(const Vector3& origin)
{}

Vector3 TestOrthoViewManager::getActiveViewOrigin()
{
    return Vector3(0, 0, 0);
}

ui::IOrthoView& TestOrthoViewManager::getActiveView()
{
    throw std::runtime_error("Not implemented");
}

ui::IOrthoView& TestOrthoViewManager::getViewByType(OrthoOrientation viewType)
{
    throw std::runtime_error("Not implemented");
}

void TestOrthoViewManager::setScale(float scale)
{}

void TestOrthoViewManager::positionAllViews(const Vector3& origin)
{}

void TestOrthoViewManager::positionActiveView(const Vector3& origin)
{}

OrthoOrientation TestOrthoViewManager::getActiveViewType() const
{
    return OrthoOrientation::XY;
}

void TestOrthoViewManager::setActiveViewType(OrthoOrientation viewType)
{}

bool TestOrthoViewManager::polygonMode() const
{
    return false;
}

std::string TestOrthoViewManager::getName() const
{
    static std::string _name(MODULE_ORTHOVIEWMANAGER);
    return _name;
}

}