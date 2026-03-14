#include "ArchGeneratorDialog.h"
#include "ArchGeometry.h"

#include "i18n.h"
#include "ui/imainframe.h"
#include "imap.h"
#include "iselection.h"
#include "icameraview.h"
#include "ishaderclipboard.h"
#include "iundo.h"
#include "igrid.h"

#include "string/convert.h"
#include "math/FloatTools.h"
#include "selectionlib.h"
#include "scenelib.h"
#include "shaderlib.h"
#include "math/Vector3.h"

#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/spinctrl.h>
#include <wx/button.h>

#include "ui/materials/MaterialChooser.h"
#include "ui/materials/MaterialSelector.h"

namespace
{
const char* const WINDOW_TITLE = N_("Arch Generator");

inline std::string getSelectedShader()
{
    auto selectedShader = GlobalShaderClipboard().getShaderName();
    if (selectedShader.empty())
        selectedShader = texdef_name_default();
    return selectedShader;
}

Vector3 getSpawnPosition()
{
    if (GlobalSelectionSystem().countSelected() > 0)
    {
        AABB bounds = GlobalSelectionSystem().getWorkZone().bounds;
        if (bounds.isValid())
            return bounds.getOrigin();
    }

    try
    {
        return GlobalCameraManager().getActiveView().getCameraOrigin();
    }
    catch (const std::runtime_error&) {}

    return Vector3(0, 0, 0);
}

} // anonymous namespace

namespace ui
{

ArchGeneratorDialog::ArchGeneratorDialog()
    : Dialog(_(WINDOW_TITLE), GlobalMainFrame().getWxTopLevelWindow())
{
    _dialog->GetSizer()->Add(
        loadNamedPanel(_dialog, "ArchGeneratorMainPanel"), 1, wxEXPAND | wxALL, 12);

    wxStaticText* topLabel = findNamedObject<wxStaticText>(_dialog, "ArchGeneratorTopLabel");
    topLabel->SetFont(topLabel->GetFont().Bold());

    findNamedObject<wxButton>(_dialog, "ArchGeneratorBrowseMaterial")
        ->Bind(wxEVT_BUTTON, &ArchGeneratorDialog::onBrowseMaterial, this);

    findNamedObject<wxTextCtrl>(_dialog, "ArchGeneratorMaterial")
        ->SetValue(getSelectedShader());
}

void ArchGeneratorDialog::onBrowseMaterial(wxCommandEvent& ev)
{
    wxTextCtrl* materialEntry = findNamedObject<wxTextCtrl>(_dialog, "ArchGeneratorMaterial");
    MaterialChooser chooser(_dialog, MaterialSelector::TextureFilter::Regular, materialEntry);
    chooser.ShowModal();
}

int ArchGeneratorDialog::getSegments()
{
    return findNamedObject<wxSpinCtrl>(_dialog, "ArchGeneratorSegments")->GetValue();
}

float ArchGeneratorDialog::getInnerRadius()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "ArchGeneratorInnerRadius")->GetValue().ToStdString(), 64.0f);
}

float ArchGeneratorDialog::getWallThickness()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "ArchGeneratorWallThickness")->GetValue().ToStdString(), 16.0f);
}

float ArchGeneratorDialog::getDepth()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "ArchGeneratorDepth")->GetValue().ToStdString(), 32.0f);
}

float ArchGeneratorDialog::getLength()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "ArchGeneratorLength")->GetValue().ToStdString(), 0.0f);
}

float ArchGeneratorDialog::getArcDegrees()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "ArchGeneratorArcDegrees")->GetValue().ToStdString(), 180.0f);
}

float ArchGeneratorDialog::getStartAngle()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "ArchGeneratorStartAngle")->GetValue().ToStdString(), 0.0f);
}

std::string ArchGeneratorDialog::getMaterial()
{
    return findNamedObject<wxTextCtrl>(_dialog, "ArchGeneratorMaterial")->GetValue().ToStdString();
}

void ArchGeneratorDialog::Show(const cmd::ArgumentList& args)
{
    ArchGeneratorDialog dialog;

    if (dialog.run() != IDialog::RESULT_OK)
        return;

    int segments = dialog.getSegments();
    float innerRadius = dialog.getInnerRadius();
    float wallThickness = dialog.getWallThickness();
    float depth = dialog.getDepth();
    float length = dialog.getLength();
    float arcDegrees = dialog.getArcDegrees();
    float startAngle = dialog.getStartAngle();
    std::string material = dialog.getMaterial();
    double gridSize = GlobalGrid().getGridSize();

    if (length > 0 && arcDegrees > 0)
    {
        double halfArc = (arcDegrees / 2.0) * arch::DEG2RAD;
        double sinHalf = std::sin(halfArc);
        if (sinHalf > 1e-6)
            innerRadius = static_cast<float>((length / 2.0) / sinHalf);
    }

    auto endpoints = arch::detectBridgeEndpoints();

    UndoableCommand undo("archGeneratorCreate");
    GlobalSelectionSystem().setSelectedAll(false);

    scene::INodePtr worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    if (endpoints.valid)
    {
        double bridgeDepth = endpoints.hasFaceDimensions ? endpoints.faceDepth : depth;
        arch::generateBridgeArch(endpoints,
            segments, wallThickness, bridgeDepth, arcDegrees,
            material, worldspawn);
    }
    else
    {
        Vector3 spawnPos = getSpawnPosition();
        spawnPos.x() = float_snapped(spawnPos.x(), gridSize);
        spawnPos.y() = float_snapped(spawnPos.y(), gridSize);
        spawnPos.z() = float_snapped(spawnPos.z(), gridSize);
        arch::generateArch(spawnPos, segments, innerRadius, wallThickness,
            depth, arcDegrees, startAngle, gridSize, material, worldspawn);
    }
}

} // namespace ui
