#include "SweepDialog.h"
#include "SweepGeometry.h"

#include "i18n.h"
#include "ui/imainframe.h"
#include "imap.h"
#include "iselection.h"
#include "iundo.h"

#include "scenelib.h"
#include "scene/EntityNode.h"
#include "wxutil/dialog/MessageBox.h"

#include <wx/stattext.h>
#include <wx/spinctrl.h>

namespace
{
const char* const WINDOW_TITLE = N_("Sweep Brushes Along Curve");

enum class InputMode
{
    None,
    Valid
};

struct InputData
{
    InputMode mode = InputMode::None;
    std::vector<scene::INodePtr> brushNodes;
    std::vector<Vector3> curvePoints;
};

InputData detectInput()
{
    InputData data;
    auto& sel = GlobalSelectionSystem();

    scene::INodePtr curveNode;

    sel.foreachSelected([&](const scene::INodePtr& node) {
        if (Node_isBrush(node))
        {
            data.brushNodes.push_back(node);
            return;
        }

        auto entityNode = std::dynamic_pointer_cast<EntityNode>(node);
        if (!entityNode) return;

        Entity& entity = entityNode->getEntity();
        std::string curveStr = entity.getKeyValue("curve_CatmullRomSpline");
        if (curveStr.empty())
            curveStr = entity.getKeyValue("curve_Nurbs");

        if (!curveStr.empty())
        {
            auto points = cables::parseCurveString(curveStr);
            if (points.size() >= 2)
            {
                data.curvePoints = points;
                curveNode = node;
            }
        }
    });

    if (!data.brushNodes.empty() && !data.curvePoints.empty())
        data.mode = InputMode::Valid;

    return data;
}

} // anonymous namespace

namespace ui
{

SweepDialog::SweepDialog()
    : Dialog(_(WINDOW_TITLE), GlobalMainFrame().getWxTopLevelWindow())
{
    _dialog->GetSizer()->Add(
        loadNamedPanel(_dialog, "SweepMainPanel"), 1, wxEXPAND | wxALL, 12);

    wxStaticText* topLabel = findNamedObject<wxStaticText>(_dialog, "SweepTopLabel");
    topLabel->SetFont(topLabel->GetFont().Bold());
}

int SweepDialog::getSegments()
{
    return findNamedObject<wxSpinCtrl>(_dialog, "SweepSegments")->GetValue();
}

void SweepDialog::Show(const cmd::ArgumentList& args)
{
    InputData input = detectInput();

    if (input.mode == InputMode::None)
    {
        wxutil::Messagebox::ShowError(
            _("Select one or more brushes and a curve entity."),
            GlobalMainFrame().getWxTopLevelWindow());
        return;
    }

    SweepDialog dialog;

    if (dialog.run() != IDialog::RESULT_OK)
        return;

    sweep::SweepParams params;
    params.segments = dialog.getSegments();

    AABB sourceBounds;
    auto sources = sweep::extractSourceBrushes(input.brushNodes, sourceBounds);
    if (sources.empty()) return;

    int sweepAxis = sweep::detectSweepAxis(input.brushNodes, sourceBounds);

    UndoableCommand undo("sweepBrushesAlongCurve");
    GlobalSelectionSystem().setSelectedAll(false);
    GlobalSelectionSystem().setSelectedAllComponents(false);

    scene::INodePtr worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    sweep::sweepBrushesAlongPath(
        sources, sourceBounds, sweepAxis,
        input.curvePoints, params, worldspawn);
}

} // namespace ui
