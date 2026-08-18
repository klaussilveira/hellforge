#pragma once

#include "icommandsystem.h"
#include "inode.h"
#include "math/Vector3.h"
#include "ui/common/GeneratorPreview.h"
#include "wxutil/dialog/Dialog.h"
#include "wxutil/XmlResourceBasedWidget.h"

#include "ArchGeometry.h"

namespace ui
{

class ArchGeneratorDialog : public wxutil::Dialog, private wxutil::XmlResourceBasedWidget
{
private:
    arch::BridgeEndpoints _endpoints;
    Vector3 _spawnPos;
    double _gridSize;
    scene::INodePtr _parent;
    GeneratorPreview _preview;

public:
    ArchGeneratorDialog(const arch::BridgeEndpoints& endpoints, const Vector3& spawnPos,
                        double gridSize, const scene::INodePtr& parent);

    int getSegments();
    float getInnerRadius();
    float getWallThickness();
    float getDepth();
    float getLength();
    float getArcDegrees();
    float getStartAngle();
    std::string getMaterial();

    GeneratorPreview& getPreview();
    void commitToMap();

    static void Show(const cmd::ArgumentList& args);

private:
    void onBrowseMaterial(wxCommandEvent& ev);
    void onParameterChanged(wxCommandEvent& ev);
    void generateInto();
    void regenerate();
};

} // namespace ui
