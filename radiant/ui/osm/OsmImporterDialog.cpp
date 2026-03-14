#include "OsmImporterDialog.h"
#include "OsmGeometry.h"

#include "i18n.h"
#include "ui/imainframe.h"
#include "imap.h"
#include "iselection.h"
#include "ishaderclipboard.h"
#include "iundo.h"

#include "string/convert.h"
#include "selectionlib.h"
#include "scenelib.h"
#include "shaderlib.h"

#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>

#include "ui/materials/MaterialChooser.h"
#include "ui/materials/MaterialSelector.h"

namespace
{
const char* const WINDOW_TITLE = N_("Import OpenStreetMap");

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

OsmImporterDialog::OsmImporterDialog()
    : Dialog(_(WINDOW_TITLE), GlobalMainFrame().getWxTopLevelWindow())
{
    _dialog->GetSizer()->Add(
        loadNamedPanel(_dialog, "OsmImporterMainPanel"), 1, wxEXPAND | wxALL, 12);

    auto shader = getSelectedShader();
    findNamedObject<wxTextCtrl>(_dialog, "OsmImporterWallMaterial")->SetValue(shader);
    findNamedObject<wxTextCtrl>(_dialog, "OsmImporterRoofMaterial")->SetValue(shader);
    findNamedObject<wxTextCtrl>(_dialog, "OsmImporterFloorMaterial")->SetValue(shader);
    findNamedObject<wxTextCtrl>(_dialog, "OsmImporterRoadMaterial")->SetValue(shader);
    findNamedObject<wxTextCtrl>(_dialog, "OsmImporterSidewalkMaterial")->SetValue(shader);

    findNamedObject<wxButton>(_dialog, "OsmImporterBrowseFile")
        ->Bind(wxEVT_BUTTON, &OsmImporterDialog::onBrowseFile, this);
    findNamedObject<wxButton>(_dialog, "OsmImporterBrowseWallMaterial")
        ->Bind(wxEVT_BUTTON, &OsmImporterDialog::onBrowseWallMaterial, this);
    findNamedObject<wxButton>(_dialog, "OsmImporterBrowseRoofMaterial")
        ->Bind(wxEVT_BUTTON, &OsmImporterDialog::onBrowseRoofMaterial, this);
    findNamedObject<wxButton>(_dialog, "OsmImporterBrowseFloorMaterial")
        ->Bind(wxEVT_BUTTON, &OsmImporterDialog::onBrowseFloorMaterial, this);
    findNamedObject<wxButton>(_dialog, "OsmImporterBrowseRoadMaterial")
        ->Bind(wxEVT_BUTTON, &OsmImporterDialog::onBrowseRoadMaterial, this);
    findNamedObject<wxButton>(_dialog, "OsmImporterBrowseSidewalkMaterial")
        ->Bind(wxEVT_BUTTON, &OsmImporterDialog::onBrowseSidewalkMaterial, this);
}

std::string OsmImporterDialog::getFilePath()
{
    return findNamedObject<wxTextCtrl>(_dialog, "OsmImporterFilePath")->GetValue().ToStdString();
}

float OsmImporterDialog::getUnitsPerMeter()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "OsmImporterUnitsPerMeter")->GetValue().ToStdString(), 40.0f);
}

float OsmImporterDialog::getLevelHeight()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "OsmImporterLevelHeight")->GetValue().ToStdString(), 3.0f);
}

float OsmImporterDialog::getBaseZ()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "OsmImporterBaseZ")->GetValue().ToStdString(), 0.0f);
}

std::string OsmImporterDialog::getWallMaterial()
{
    return findNamedObject<wxTextCtrl>(_dialog, "OsmImporterWallMaterial")->GetValue().ToStdString();
}

std::string OsmImporterDialog::getRoofMaterial()
{
    return findNamedObject<wxTextCtrl>(_dialog, "OsmImporterRoofMaterial")->GetValue().ToStdString();
}

std::string OsmImporterDialog::getFloorMaterial()
{
    return findNamedObject<wxTextCtrl>(_dialog, "OsmImporterFloorMaterial")->GetValue().ToStdString();
}

std::string OsmImporterDialog::getRoadMaterial()
{
    return findNamedObject<wxTextCtrl>(_dialog, "OsmImporterRoadMaterial")->GetValue().ToStdString();
}

std::string OsmImporterDialog::getSidewalkMaterial()
{
    return findNamedObject<wxTextCtrl>(_dialog, "OsmImporterSidewalkMaterial")->GetValue().ToStdString();
}

float OsmImporterDialog::getDefaultLaneWidth()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "OsmImporterLaneWidth")->GetValue().ToStdString(), 3.5f);
}

float OsmImporterDialog::getSidewalkWidth()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "OsmImporterSidewalkWidth")->GetValue().ToStdString(), 2.0f);
}

float OsmImporterDialog::getCurbHeight()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "OsmImporterCurbHeight")->GetValue().ToStdString(), 0.2f);
}

void OsmImporterDialog::onBrowseFile(wxCommandEvent& ev)
{
    wxFileDialog dlg(_dialog, _("Select OSM File"), "", "",
                     "OpenStreetMap files (*.osm)|*.osm|All files (*.*)|*.*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK)
        findNamedObject<wxTextCtrl>(_dialog, "OsmImporterFilePath")->SetValue(dlg.GetPath());
}

void OsmImporterDialog::browseMaterial(const char* controlName)
{
    wxTextCtrl* entry = findNamedObject<wxTextCtrl>(_dialog, controlName);
    MaterialChooser chooser(_dialog, MaterialSelector::TextureFilter::Regular, entry);
    chooser.ShowModal();
}

void OsmImporterDialog::onBrowseWallMaterial(wxCommandEvent& ev)
{
    browseMaterial("OsmImporterWallMaterial");
}

void OsmImporterDialog::onBrowseRoofMaterial(wxCommandEvent& ev)
{
    browseMaterial("OsmImporterRoofMaterial");
}

void OsmImporterDialog::onBrowseFloorMaterial(wxCommandEvent& ev)
{
    browseMaterial("OsmImporterFloorMaterial");
}

void OsmImporterDialog::onBrowseRoadMaterial(wxCommandEvent& ev)
{
    browseMaterial("OsmImporterRoadMaterial");
}

void OsmImporterDialog::onBrowseSidewalkMaterial(wxCommandEvent& ev)
{
    browseMaterial("OsmImporterSidewalkMaterial");
}

void OsmImporterDialog::Show(const cmd::ArgumentList& args)
{
    OsmImporterDialog dialog;

    if (dialog.run() != IDialog::RESULT_OK)
        return;

    std::string filePath = dialog.getFilePath();
    if (filePath.empty())
    {
        wxMessageBox(_("Please select an OSM file."), _("Error"), wxOK | wxICON_ERROR);
        return;
    }

    osm::OsmImportParams params;
    params.unitsPerMeter = dialog.getUnitsPerMeter();
    params.levelHeight = dialog.getLevelHeight();
    params.baseZ = dialog.getBaseZ();
    params.wallMaterial = dialog.getWallMaterial();
    params.roofMaterial = dialog.getRoofMaterial();
    params.floorMaterial = dialog.getFloorMaterial();
    params.roadMaterial = dialog.getRoadMaterial();
    params.sidewalkMaterial = dialog.getSidewalkMaterial();
    params.defaultLaneWidth = dialog.getDefaultLaneWidth();
    params.sidewalkWidth = dialog.getSidewalkWidth();
    params.curbHeight = dialog.getCurbHeight();

    osm::OsmData data;
    if (!osm::parseOsmFile(filePath, data, params.levelHeight))
    {
        wxMessageBox(_("Failed to parse OSM file."), _("Error"), wxOK | wxICON_ERROR);
        return;
    }

    if (data.buildings.empty() && data.roads.empty())
    {
        wxMessageBox(_("No buildings or roads found in the OSM file."), _("Warning"), wxOK | wxICON_WARNING);
        return;
    }

    UndoableCommand undo("osmImport");
    GlobalSelectionSystem().setSelectedAll(false);

    scene::INodePtr worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    int brushCount = osm::generateOsmBuildings(data, params, worldspawn);
    int roadCount = osm::generateOsmRoads(data, params, worldspawn);

    wxMessageBox(wxString::Format("Imported %d building brushes, %d road/sidewalk patches.",
        brushCount, roadCount), _("OSM Import"), wxOK | wxICON_INFORMATION);
}

} // namespace ui
