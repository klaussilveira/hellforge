#include "ArchGeneratorDialog.h"
#include "ArchGeometry.h"

#include "i18n.h"
#include "ui/imainframe.h"
#include "ui/common/GeneratorSpawn.h"
#include "imap.h"
#include "iscenegraph.h"
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
#include "wxutil/PickerButton.h"

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


} // anonymous namespace

namespace ui
{

ArchGeneratorDialog::ArchGeneratorDialog(const arch::BridgeEndpoints& endpoints,
                                         const Vector3& spawnPos, double gridSize,
                                         const scene::INodePtr& parent)
    : Dialog(_(WINDOW_TITLE), GlobalMainFrame().getWxTopLevelWindow()),
      _endpoints(endpoints), _spawnPos(spawnPos), _gridSize(gridSize), _parent(parent)
{
    _dialog->GetSizer()->Add(
        loadNamedPanel(_dialog, "ArchGeneratorMainPanel"), 1, wxEXPAND | wxALL, 12);

    wxStaticText* topLabel = findNamedObject<wxStaticText>(_dialog, "ArchGeneratorTopLabel");
    topLabel->SetFont(topLabel->GetFont().Bold());

    auto* browseMaterial = wxutil::ReplaceWithPickerButton(
        findNamedObject<wxButton>(_dialog, "ArchGeneratorBrowseMaterial"));
    browseMaterial->Bind(wxEVT_BUTTON, &ArchGeneratorDialog::onBrowseMaterial, this);

    findNamedObject<wxTextCtrl>(_dialog, "ArchGeneratorMaterial")
        ->SetValue(getSelectedShader());

    bindParameterEvents(_dialog, this, &ArchGeneratorDialog::onParameterChanged);

    regenerate();
}

GeneratorPreview& ArchGeneratorDialog::getPreview()
{
    return _preview;
}

void ArchGeneratorDialog::onParameterChanged(wxCommandEvent& ev)
{
    regenerate();
}

void ArchGeneratorDialog::generateInto()
{
    int segments = getSegments();
    float innerRadius = getInnerRadius();
    float wallThickness = getWallThickness();
    float depth = getDepth();
    float length = getLength();
    float arcDegrees = getArcDegrees();
    float startAngle = getStartAngle();
    std::string material = getMaterial();

    if (length > 0 && arcDegrees > 0)
    {
        double halfArc = (arcDegrees / 2.0) * arch::DEG2RAD;
        double sinHalf = std::sin(halfArc);
        if (sinHalf > 1e-6)
            innerRadius = static_cast<float>((length / 2.0) / sinHalf);
    }

    if (segments < 1 || arcDegrees <= 0 || wallThickness <= 0 ||
        (!_endpoints.valid && (innerRadius <= 0 || depth <= 0)))
    {
        return;
    }

    if (_endpoints.valid)
    {
        double bridgeDepth = _endpoints.hasFaceDimensions ? _endpoints.faceDepth : depth;
        arch::generateBridgeArch(_endpoints, segments, wallThickness, bridgeDepth,
            arcDegrees, material, _parent);
    }
    else
    {
        arch::generateArch(_spawnPos, segments, innerRadius, wallThickness,
            depth, arcDegrees, startAngle, _gridSize, material, _parent);
    }
}

void ArchGeneratorDialog::regenerate()
{
    _preview.update(_parent, [this]() { generateInto(); });
}

void ArchGeneratorDialog::commitToMap()
{
    _preview.commit(_parent, "archGeneratorCreate", [this]() { generateInto(); });
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
    auto endpoints = arch::detectBridgeEndpoints();
    double gridSize = GlobalGrid().getGridSize();

    Vector3 spawnPos = getGeneratorSpawnPosition();
    spawnPos.x() = float_snapped(spawnPos.x(), gridSize);
    spawnPos.y() = float_snapped(spawnPos.y(), gridSize);
    spawnPos.z() = float_snapped(spawnPos.z(), gridSize);

    scene::INodePtr worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    ArchGeneratorDialog dialog(endpoints, spawnPos, gridSize, worldspawn);

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
