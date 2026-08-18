#pragma once

#include "icommandsystem.h"
#include "inode.h"
#include "math/AABB.h"
#include "math/Vector3.h"
#include "ui/common/GeneratorPreview.h"
#include "wxutil/dialog/Dialog.h"
#include "wxutil/XmlResourceBasedWidget.h"

#include "SweepGeometry.h"

#include <vector>

namespace ui
{

class SweepDialog : public wxutil::Dialog, private wxutil::XmlResourceBasedWidget
{
private:
    std::vector<sweep::SourceBrushData> _sources;
    AABB _sourceBounds;
    int _sweepAxis;
    std::vector<Vector3> _curvePoints;
    scene::INodePtr _parent;
    GeneratorPreview _preview;

public:
    SweepDialog(const std::vector<sweep::SourceBrushData>& sources, const AABB& sourceBounds,
                int sweepAxis, const std::vector<Vector3>& curvePoints,
                const scene::INodePtr& parent);

    int getSegments();

    GeneratorPreview& getPreview();
    void commitToMap();

    static void Show(const cmd::ArgumentList& args);

private:
    void onParameterChanged(wxCommandEvent& ev);
    void generateInto();
    void regenerate();
};

} // namespace ui
