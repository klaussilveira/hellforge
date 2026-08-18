#pragma once

#include "icommandsystem.h"
#include "inode.h"
#include "ui/common/GeneratorPreview.h"
#include "wxutil/dialog/Dialog.h"
#include "wxutil/XmlResourceBasedWidget.h"

#include "CornerPatchGeometry.h"

namespace ui
{

class CornerPatchDialog : public wxutil::Dialog, private wxutil::XmlResourceBasedWidget
{
private:
    cornerpatch::CornerDetection _detection;
    scene::INodePtr _parent;
    GeneratorPreview _preview;

public:
    CornerPatchDialog(const cornerpatch::CornerDetection& detection, int defaultRadius,
                      const scene::INodePtr& parent);

    int getSegments();
    float getRadius();
    float getArcDegrees();
    bool getInvert();

    GeneratorPreview& getPreview();
    void commitToMap();

    static void Show(const cmd::ArgumentList& args);

private:
    void onParameterChanged(wxCommandEvent& ev);
    void generateInto();
    void regenerate();
};

} // namespace ui
