#include "SweepDialog.h"
#include "SweepGeometry.h"

#include "i18n.h"
#include "ui/imainframe.h"
#include "imap.h"
#include "iscenegraph.h"
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

SweepDialog::SweepDialog(const std::vector<sweep::SourceBrushData>& sources,
                         const AABB& sourceBounds, int sweepAxis,
                         const std::vector<Vector3>& curvePoints,
                         const scene::INodePtr& parent)
    : Dialog(_(WINDOW_TITLE), GlobalMainFrame().getWxTopLevelWindow()),
      _sources(sources), _sourceBounds(sourceBounds), _sweepAxis(sweepAxis),
      _curvePoints(curvePoints), _parent(parent)
{
    _dialog->GetSizer()->Add(
        loadNamedPanel(_dialog, "SweepMainPanel"), 1, wxEXPAND | wxALL, 12);

    wxStaticText* topLabel = findNamedObject<wxStaticText>(_dialog, "SweepTopLabel");
    topLabel->SetFont(topLabel->GetFont().Bold());

    bindParameterEvents(_dialog, this, &SweepDialog::onParameterChanged);

    regenerate();
}

int SweepDialog::getSegments()
{
    return findNamedObject<wxSpinCtrl>(_dialog, "SweepSegments")->GetValue();
}

GeneratorPreview& SweepDialog::getPreview()
{
    return _preview;
}

void SweepDialog::onParameterChanged(wxCommandEvent& ev)
{
    regenerate();
}

void SweepDialog::generateInto()
{
    sweep::SweepParams params;
    params.segments = getSegments();

    if (params.segments < 1)
    {
        return;
    }

    sweep::sweepBrushesAlongPath(_sources, _sourceBounds, _sweepAxis,
        _curvePoints, params, _parent);
}

void SweepDialog::regenerate()
{
    _preview.update(_parent, [this]() { generateInto(); });
}

void SweepDialog::commitToMap()
{
    _preview.commit(_parent, "sweepBrushesAlongCurve", [this]() { generateInto(); });
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

    AABB sourceBounds;
    auto sources = sweep::extractSourceBrushes(input.brushNodes, sourceBounds);
    if (sources.empty()) return;

    int sweepAxis = sweep::detectSweepAxis(input.brushNodes, sourceBounds);

    scene::INodePtr worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    SweepDialog dialog(sources, sourceBounds, sweepAxis, input.curvePoints, worldspawn);

    if (dialog.run() == IDialog::RESULT_OK)
    {
        dialog.commitToMap();
    }
    else
    {
        dialog.getPreview().clear();
    }
}

} // namespace ui
