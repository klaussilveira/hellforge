#include "CityPlannerDialog.h"
#include "CityPlannerModel.h"
#include "CityPlannerGenerators.h"

#include "ui/tilemap/TileMapGeometry.h"
#include "ui/building/BuildingGeometry.h"

#include "i18n.h"
#include "ui/imainframe.h"
#include "imap.h"
#include "iselection.h"
#include "icameraview.h"
#include "ishaderclipboard.h"
#include "iundo.h"
#include "iscenegraph.h"

#include "string/convert.h"
#include "selectionlib.h"
#include "scenelib.h"
#include "shaderlib.h"
#include "math/Vector3.h"

#include <wx/stattext.h>
#include <wx/statbox.h>
#include <wx/textctrl.h>
#include <wx/spinctrl.h>
#include <wx/choice.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/dcclient.h>
#include <wx/dcbuffer.h>
#include <wx/panel.h>

#include <random>

#include "ui/materials/MaterialChooser.h"
#include "ui/materials/MaterialSelector.h"

namespace
{
const char* const WINDOW_TITLE = N_("City Planner");
const int DEFAULT_GRID_COLS = 16;
const int DEFAULT_GRID_ROWS = 16;
const int CELL_SIZE = 28;

inline std::string getDefaultShader()
{
    auto s = GlobalShaderClipboard().getShaderName();
    if (s.empty())
        s = texdef_name_default();
    return s;
}

inline Vector3 getSpawnPosition()
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

wxColour cellColour(cityplanner::CellType t)
{
    switch (t)
    {
    case cityplanner::CellType::Floor:              return wxColour(80, 120, 80);
    case cityplanner::CellType::Wall:               return wxColour(160, 140, 80);
    case cityplanner::CellType::HalfSquareN:
    case cityplanner::CellType::HalfSquareS:
    case cityplanner::CellType::HalfSquareE:
    case cityplanner::CellType::HalfSquareW:        return wxColour(140, 100, 60);
    case cityplanner::CellType::Building:           return wxColour(120, 140, 200);
    case cityplanner::CellType::WindowlessBuilding: return wxColour(90, 100, 140);
    default: return wxColour(40, 40, 40);
    }
}

wxTextCtrl* addMaterialRow(wxWindow* parent, wxFlexGridSizer* sizer,
    const wxString& label, const std::string& value)
{
    sizer->Add(new wxStaticText(parent, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);

    auto* row = new wxBoxSizer(wxHORIZONTAL);
    auto* tc = new wxTextCtrl(parent, wxID_ANY, value);
    row->Add(tc, 1, wxEXPAND);

    auto* btn = new wxButton(parent, wxID_ANY, "...", wxDefaultPosition, wxSize(30, -1));
    btn->Bind(wxEVT_BUTTON, [parent, tc](wxCommandEvent&) {
        ui::MaterialChooser chooser(parent, ui::MaterialSelector::TextureFilter::Regular, tc);
        chooser.ShowModal();
    });
    row->Add(btn, 0, wxLEFT, 4);

    sizer->Add(row, 1, wxEXPAND);
    return tc;
}

} // anonymous namespace

namespace ui
{

CityPlannerDialog::CityPlannerDialog()
    : Dialog(_(WINDOW_TITLE), GlobalMainFrame().getWxTopLevelWindow()),
      _scrollWin(nullptr),
      _canvas(nullptr),
      _selectedGroup(-1),
      _activeBrush(Brush::Floor),
      _gridCols(nullptr), _gridRows(nullptr),
      _floorBrushSizer(nullptr), _wallBrushSizer(nullptr), _bldgBrushSizer(nullptr),
      _floorMat(nullptr), _wallMat(nullptr),
      _bldgHeaderLabel(nullptr),
      _bpFloorCount(nullptr), _bpFloorHeight(nullptr),
      _bpWallThickness(nullptr), _bpTrimHeight(nullptr),
      _bpWindowMode(nullptr), _bpWindowsPerFloor(nullptr),
      _bpWindowWidth(nullptr), _bpWindowHeight(nullptr),
      _bpSillHeight(nullptr),
      _bpCornerColumns(nullptr), _bpCornerExtrude(nullptr),
      _bpNoFirstFloorWindows(nullptr), _bpFirstFloorDoor(nullptr),
      _bpRoofType(nullptr), _bpRoofHeight(nullptr), _bpRoofBorderHeight(nullptr),
      _bpWallMaterial(nullptr), _bpTrimMaterial(nullptr),
      _suppressParamEvents(false)
{
    _dialog->SetMinSize(wxSize(760, 760));

    auto* mainSizer = new wxBoxSizer(wxHORIZONTAL);

    auto* controlPanel = loadNamedPanel(_dialog, "CityPlannerControlPanel");
    mainSizer->Add(controlPanel, 0, wxEXPAND | wxALL, 6);

    _gridCols = findNamedObject<wxSpinCtrl>(_dialog, "CityPlannerGridCols");
    _gridRows = findNamedObject<wxSpinCtrl>(_dialog, "CityPlannerGridRows");
    _gridCols->Bind(wxEVT_SPINCTRL, &CityPlannerDialog::onGridSizeChanged, this);
    _gridRows->Bind(wxEVT_SPINCTRL, &CityPlannerDialog::onGridSizeChanged, this);

    findNamedObject<wxChoice>(_dialog, "CityPlannerBrush")
        ->Bind(wxEVT_CHOICE, &CityPlannerDialog::onBrushChanged, this);

    auto* defaultsPanel = findNamedObject<wxPanel>(_dialog, "CityPlannerDefaultsPanel");
    buildDefaultsPanel(defaultsPanel);

    auto* rightSizer = new wxBoxSizer(wxVERTICAL);

    auto* toolbar = new wxPanel(_dialog, wxID_ANY);
    auto* tbSizer = new wxBoxSizer(wxHORIZONTAL);
    tbSizer->AddStretchSpacer();
    auto* randomBtn = new wxButton(toolbar, wxID_ANY, "Random...");
    randomBtn->Bind(wxEVT_BUTTON, &CityPlannerDialog::onRandom, this);
    tbSizer->Add(randomBtn, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
    toolbar->SetSizer(tbSizer);
    rightSizer->Add(toolbar, 0, wxEXPAND | wxALL, 6);

    _scrollWin = new wxScrolledWindow(_dialog, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxHSCROLL | wxVSCROLL);
    _scrollWin->SetScrollRate(CELL_SIZE, CELL_SIZE);

    _canvas = new CityGridCanvas(_scrollWin, this);
    int canvasW = DEFAULT_GRID_COLS * CELL_SIZE + 1;
    int canvasH = DEFAULT_GRID_ROWS * CELL_SIZE + 1;
    _canvas->SetMinSize(wxSize(canvasW, canvasH));
    _scrollWin->SetVirtualSize(canvasW, canvasH);

    auto* scrollSizer = new wxBoxSizer(wxVERTICAL);
    scrollSizer->Add(_canvas, 0, wxEXPAND);
    _scrollWin->SetSizer(scrollSizer);

    rightSizer->Add(_scrollWin, 1, wxEXPAND | wxALL, 6);
    mainSizer->Add(rightSizer, 1, wxEXPAND);

    _dialog->GetSizer()->Add(mainSizer, 1, wxEXPAND);

    resizeGrid();
    updateBrushPanelVisibility();
    updateSelectedGroupHeader();
}

void CityPlannerDialog::buildDefaultsPanel(wxPanel* parent)
{
    auto* defSizer = new wxBoxSizer(wxVERTICAL);
    std::string defMat = getDefaultShader();

    // Floor brush
    auto* floorBox = new wxStaticBoxSizer(wxVERTICAL, parent, "Floor Brush");
    _floorBrushSizer = floorBox;
    auto* floorGrid = new wxFlexGridSizer(2, 4, 6);
    floorGrid->AddGrowableCol(1, 1);
    _floorMat = addMaterialRow(parent, floorGrid, "Material:", defMat);
    floorBox->Add(floorGrid, 1, wxEXPAND | wxALL, 6);
    defSizer->Add(floorBox, 0, wxEXPAND);

    // Wall brush
    auto* wallBox = new wxStaticBoxSizer(wxVERTICAL, parent, "Wall Brush");
    _wallBrushSizer = wallBox;
    auto* wallGrid = new wxFlexGridSizer(2, 4, 6);
    wallGrid->AddGrowableCol(1, 1);
    _wallMat = addMaterialRow(parent, wallGrid, "Material:", defMat);
    wallBox->Add(wallGrid, 1, wxEXPAND | wxALL, 6);
    defSizer->Add(wallBox, 0, wxEXPAND | wxTOP, 6);

    // Building brush
    auto* bldgBox = new wxStaticBoxSizer(wxVERTICAL, parent, "Building");
    _bldgBrushSizer = bldgBox;

    _bldgHeaderLabel = new wxStaticText(parent, wxID_ANY, "Default (new groups)");
    _bldgHeaderLabel->SetForegroundColour(wxColour(200, 200, 120));
    bldgBox->Add(_bldgHeaderLabel, 0, wxLEFT | wxRIGHT | wxTOP, 6);

    auto* bp = buildBuildingParamsPanel(parent);
    bldgBox->Add(bp, 0, wxEXPAND | wxALL, 6);
    defSizer->Add(bldgBox, 0, wxEXPAND | wxTOP, 6);

    parent->SetSizer(defSizer);
}

wxPanel* CityPlannerDialog::buildBuildingParamsPanel(wxWindow* parent)
{
    auto* panel = new wxPanel(parent, wxID_ANY);
    auto* grid = new wxFlexGridSizer(2, 4, 6);
    grid->AddGrowableCol(1, 1);

    std::string defMat = getDefaultShader();

    auto addLabel = [&](const wxString& text) {
        grid->Add(new wxStaticText(panel, wxID_ANY, text), 0, wxALIGN_CENTER_VERTICAL);
    };

    addLabel("Floors:");
    _bpFloorCount = new wxSpinCtrl(panel, wxID_ANY, "3",
        wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 64, 3);
    grid->Add(_bpFloorCount, 1, wxEXPAND);

    addLabel("Floor Height:");
    _bpFloorHeight = new wxTextCtrl(panel, wxID_ANY, "128");
    grid->Add(_bpFloorHeight, 1, wxEXPAND);

    addLabel("Wall Thickness:");
    _bpWallThickness = new wxTextCtrl(panel, wxID_ANY, "8");
    grid->Add(_bpWallThickness, 1, wxEXPAND);

    addLabel("Trim Height:");
    _bpTrimHeight = new wxTextCtrl(panel, wxID_ANY, "8");
    grid->Add(_bpTrimHeight, 1, wxEXPAND);

    addLabel("Windows:");
    _bpWindowMode = new wxChoice(panel, wxID_ANY);
    _bpWindowMode->Append("None");
    _bpWindowMode->Append("Automatic");
    _bpWindowMode->Append("Fixed Count");
    _bpWindowMode->SetSelection(1);
    grid->Add(_bpWindowMode, 1, wxEXPAND);

    addLabel("Per Floor:");
    _bpWindowsPerFloor = new wxSpinCtrl(panel, wxID_ANY, "3",
        wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 32, 3);
    grid->Add(_bpWindowsPerFloor, 1, wxEXPAND);

    addLabel("Window W:");
    _bpWindowWidth = new wxTextCtrl(panel, wxID_ANY, "48");
    grid->Add(_bpWindowWidth, 1, wxEXPAND);

    addLabel("Window H:");
    _bpWindowHeight = new wxTextCtrl(panel, wxID_ANY, "56");
    grid->Add(_bpWindowHeight, 1, wxEXPAND);

    addLabel("Sill H:");
    _bpSillHeight = new wxTextCtrl(panel, wxID_ANY, "32");
    grid->Add(_bpSillHeight, 1, wxEXPAND);

    addLabel("Corner Cols:");
    _bpCornerColumns = new wxCheckBox(panel, wxID_ANY, "");
    grid->Add(_bpCornerColumns, 1, wxEXPAND);

    addLabel("Col Extrude:");
    _bpCornerExtrude = new wxTextCtrl(panel, wxID_ANY, "0");
    grid->Add(_bpCornerExtrude, 1, wxEXPAND);

    addLabel("No 1F Windows:");
    _bpNoFirstFloorWindows = new wxCheckBox(panel, wxID_ANY, "");
    grid->Add(_bpNoFirstFloorWindows, 1, wxEXPAND);

    addLabel("1F Door:");
    _bpFirstFloorDoor = new wxCheckBox(panel, wxID_ANY, "");
    grid->Add(_bpFirstFloorDoor, 1, wxEXPAND);

    addLabel("Roof:");
    _bpRoofType = new wxChoice(panel, wxID_ANY);
    _bpRoofType->Append("Flat");
    _bpRoofType->Append("Flat with Border");
    _bpRoofType->Append("Slanted");
    _bpRoofType->Append("A-Frame");
    _bpRoofType->SetSelection(0);
    grid->Add(_bpRoofType, 1, wxEXPAND);

    addLabel("Roof Height:");
    _bpRoofHeight = new wxTextCtrl(panel, wxID_ANY, "64");
    grid->Add(_bpRoofHeight, 1, wxEXPAND);

    addLabel("Border H:");
    _bpRoofBorderHeight = new wxTextCtrl(panel, wxID_ANY, "16");
    grid->Add(_bpRoofBorderHeight, 1, wxEXPAND);

    _bpWallMaterial = addMaterialRow(panel, grid, "Wall Mat:", defMat);
    _bpTrimMaterial = addMaterialRow(panel, grid, "Trim Mat:", defMat);

    _bpFloorCount->Bind(wxEVT_SPINCTRL, &CityPlannerDialog::onParamsSpin, this);
    _bpWindowsPerFloor->Bind(wxEVT_SPINCTRL, &CityPlannerDialog::onParamsSpin, this);
    _bpWindowMode->Bind(wxEVT_CHOICE, &CityPlannerDialog::onParamsChanged, this);
    _bpRoofType->Bind(wxEVT_CHOICE, &CityPlannerDialog::onParamsChanged, this);
    _bpCornerColumns->Bind(wxEVT_CHECKBOX, &CityPlannerDialog::onParamsChanged, this);
    _bpNoFirstFloorWindows->Bind(wxEVT_CHECKBOX, &CityPlannerDialog::onParamsChanged, this);
    _bpFirstFloorDoor->Bind(wxEVT_CHECKBOX, &CityPlannerDialog::onParamsChanged, this);

    _bpFloorHeight->Bind(wxEVT_TEXT, &CityPlannerDialog::onParamsText, this);
    _bpWallThickness->Bind(wxEVT_TEXT, &CityPlannerDialog::onParamsText, this);
    _bpTrimHeight->Bind(wxEVT_TEXT, &CityPlannerDialog::onParamsText, this);
    _bpWindowWidth->Bind(wxEVT_TEXT, &CityPlannerDialog::onParamsText, this);
    _bpWindowHeight->Bind(wxEVT_TEXT, &CityPlannerDialog::onParamsText, this);
    _bpSillHeight->Bind(wxEVT_TEXT, &CityPlannerDialog::onParamsText, this);
    _bpCornerExtrude->Bind(wxEVT_TEXT, &CityPlannerDialog::onParamsText, this);
    _bpRoofHeight->Bind(wxEVT_TEXT, &CityPlannerDialog::onParamsText, this);
    _bpRoofBorderHeight->Bind(wxEVT_TEXT, &CityPlannerDialog::onParamsText, this);
    _bpWallMaterial->Bind(wxEVT_TEXT, &CityPlannerDialog::onParamsText, this);
    _bpTrimMaterial->Bind(wxEVT_TEXT, &CityPlannerDialog::onParamsText, this);

    panel->SetSizer(grid);
    return panel;
}

int CityPlannerDialog::getGridCols()
{
    return _gridCols->GetValue();
}

int CityPlannerDialog::getGridRows()
{
    return _gridRows->GetValue();
}

float CityPlannerDialog::getTileWidth()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "CityPlannerTileWidth")->GetValue().ToStdString(), 128.0f);
}

float CityPlannerDialog::getTileHeight()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "CityPlannerTileHeight")->GetValue().ToStdString(), 128.0f);
}

float CityPlannerDialog::getFloorThickness()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "CityPlannerFloorThickness")->GetValue().ToStdString(), 16.0f);
}

float CityPlannerDialog::getWallHeight()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "CityPlannerWallHeight")->GetValue().ToStdString(), 128.0f);
}

bool CityPlannerDialog::getHeightVariation()
{
    return findNamedObject<wxCheckBox>(_dialog, "CityPlannerHeightVariation")->GetValue();
}

std::string CityPlannerDialog::getDefaultFloorMaterial()
{
    return _floorMat->GetValue().ToStdString();
}

std::string CityPlannerDialog::getDefaultWallMaterial()
{
    return _wallMat->GetValue().ToStdString();
}

building::BuildingParams CityPlannerDialog::readParamsFromPanel()
{
    building::BuildingParams p;
    p.floorCount = _bpFloorCount->GetValue();
    p.floorHeight = string::convert<float>(_bpFloorHeight->GetValue().ToStdString(), 128.0f);
    p.wallThickness = string::convert<float>(_bpWallThickness->GetValue().ToStdString(), 8.0f);
    p.trimHeight = string::convert<float>(_bpTrimHeight->GetValue().ToStdString(), 8.0f);

    int winMode = _bpWindowMode->GetSelection();
    if (winMode == 0)
        p.windowsPerFloor = -1;
    else if (winMode == 1)
        p.windowsPerFloor = 0;
    else
        p.windowsPerFloor = _bpWindowsPerFloor->GetValue();

    p.windowWidth = string::convert<float>(_bpWindowWidth->GetValue().ToStdString(), 48.0f);
    p.windowHeight = string::convert<float>(_bpWindowHeight->GetValue().ToStdString(), 56.0f);
    p.sillHeight = string::convert<float>(_bpSillHeight->GetValue().ToStdString(), 32.0f);
    p.cornerColumns = _bpCornerColumns->GetValue();
    p.cornerExtrude = string::convert<float>(_bpCornerExtrude->GetValue().ToStdString(), 0.0f);
    p.noFirstFloorWindows = _bpNoFirstFloorWindows->GetValue();
    p.firstFloorDoor = _bpFirstFloorDoor->GetValue();
    p.roofType = _bpRoofType->GetSelection();
    p.roofHeight = string::convert<float>(_bpRoofHeight->GetValue().ToStdString(), 64.0f);
    p.roofBorderHeight = string::convert<float>(_bpRoofBorderHeight->GetValue().ToStdString(), 16.0f);
    p.wallMaterial = _bpWallMaterial->GetValue().ToStdString();
    p.trimMaterial = _bpTrimMaterial->GetValue().ToStdString();
    return p;
}

void CityPlannerDialog::writeParamsToPanel(const building::BuildingParams& p)
{
    _suppressParamEvents = true;

    _bpFloorCount->SetValue(p.floorCount);
    _bpFloorHeight->SetValue(string::to_string(p.floorHeight));
    _bpWallThickness->SetValue(string::to_string(p.wallThickness));
    _bpTrimHeight->SetValue(string::to_string(p.trimHeight));

    if (p.windowsPerFloor < 0)
        _bpWindowMode->SetSelection(0);
    else if (p.windowsPerFloor == 0)
        _bpWindowMode->SetSelection(1);
    else
    {
        _bpWindowMode->SetSelection(2);
        _bpWindowsPerFloor->SetValue(p.windowsPerFloor);
    }

    _bpWindowWidth->SetValue(string::to_string(p.windowWidth));
    _bpWindowHeight->SetValue(string::to_string(p.windowHeight));
    _bpSillHeight->SetValue(string::to_string(p.sillHeight));
    _bpCornerColumns->SetValue(p.cornerColumns);
    _bpCornerExtrude->SetValue(string::to_string(p.cornerExtrude));
    _bpNoFirstFloorWindows->SetValue(p.noFirstFloorWindows);
    _bpFirstFloorDoor->SetValue(p.firstFloorDoor);
    _bpRoofType->SetSelection(p.roofType);
    _bpRoofHeight->SetValue(string::to_string(p.roofHeight));
    _bpRoofBorderHeight->SetValue(string::to_string(p.roofBorderHeight));
    _bpWallMaterial->SetValue(p.wallMaterial);
    _bpTrimMaterial->SetValue(p.trimMaterial);

    _suppressParamEvents = false;
}

void CityPlannerDialog::commitPanelToCurrentGroup()
{
    if (_selectedGroup < 0 || _selectedGroup >= static_cast<int>(_groups.size()))
        return;
    _groups[_selectedGroup].params = readParamsFromPanel();
}

void CityPlannerDialog::rebuildGroups()
{
    int cols = getGridCols();
    int rows = getGridRows();
    building::BuildingParams defaults = readParamsFromPanel();

    std::vector<std::pair<int, int>> priorCells;
    if (_selectedGroup >= 0 && _selectedGroup < static_cast<int>(_groups.size()))
        priorCells = _groups[_selectedGroup].cells;

    _groups = cityplanner::computeBuildingGroups(_grid, cols, rows, _groups, defaults);

    _selectedGroup = -1;
    if (!priorCells.empty())
    {
        for (const auto& cell : priorCells)
        {
            int cc = cell.first, rr = cell.second;
            if (rr < 0 || rr >= static_cast<int>(_grid.size())) continue;
            if (cc < 0 || cc >= static_cast<int>(_grid[rr].size())) continue;
            int id = _grid[rr][cc].buildingGroupId;
            if (id >= 0 && id < static_cast<int>(_groups.size()))
            {
                _selectedGroup = id;
                break;
            }
        }
    }

    if (_selectedGroup >= 0)
        writeParamsToPanel(_groups[_selectedGroup].params);
    updateSelectedGroupHeader();
}

void CityPlannerDialog::selectGroupAt(int gx, int gy)
{
    if (gy < 0 || gy >= static_cast<int>(_grid.size())) return;
    if (gx < 0 || gx >= static_cast<int>(_grid[gy].size())) return;

    const auto& cell = _grid[gy][gx];

    auto brushForCell = [](cityplanner::CellType t) -> int {
        switch (t)
        {
        case cityplanner::CellType::Floor:              return static_cast<int>(Brush::Floor);
        case cityplanner::CellType::Wall:               return static_cast<int>(Brush::Wall);
        case cityplanner::CellType::HalfSquareN:        return static_cast<int>(Brush::HalfSquareN);
        case cityplanner::CellType::HalfSquareS:        return static_cast<int>(Brush::HalfSquareS);
        case cityplanner::CellType::HalfSquareE:        return static_cast<int>(Brush::HalfSquareE);
        case cityplanner::CellType::HalfSquareW:        return static_cast<int>(Brush::HalfSquareW);
        case cityplanner::CellType::Building:           return static_cast<int>(Brush::Building);
        case cityplanner::CellType::WindowlessBuilding: return static_cast<int>(Brush::WindowlessBuilding);
        default: return -1;
        }
    };

    int brushIdx = brushForCell(cell.type);
    if (brushIdx >= 0)
    {
        auto* brushChoice = findNamedObject<wxChoice>(_dialog, "CityPlannerBrush");
        brushChoice->SetSelection(brushIdx);
        _activeBrush = static_cast<Brush>(brushIdx);
        updateBrushPanelVisibility();
    }

    if (!cityplanner::isBuildingType(cell.type)) return;

    int id = cell.buildingGroupId;
    if (id < 0 || id >= static_cast<int>(_groups.size())) return;

    _selectedGroup = id;
    writeParamsToPanel(_groups[id].params);
    updateSelectedGroupHeader();
}

void CityPlannerDialog::resizeGrid()
{
    int cols = getGridCols();
    int rows = getGridRows();

    _grid.resize(rows);
    for (auto& row : _grid)
        row.resize(cols);

    int canvasW = cols * CELL_SIZE + 1;
    int canvasH = rows * CELL_SIZE + 1;
    _canvas->SetMinSize(wxSize(canvasW, canvasH));
    _scrollWin->SetVirtualSize(canvasW, canvasH);
    _scrollWin->FitInside();
    _canvas->Refresh();
}

void CityPlannerDialog::updateBrushPanelVisibility()
{
    bool showFloor = (_activeBrush == Brush::Floor);
    bool showWall  = (_activeBrush == Brush::Wall ||
                      _activeBrush == Brush::HalfSquareN ||
                      _activeBrush == Brush::HalfSquareS ||
                      _activeBrush == Brush::HalfSquareE ||
                      _activeBrush == Brush::HalfSquareW);
    bool showBldg  = (_activeBrush == Brush::Building ||
                      _activeBrush == Brush::WindowlessBuilding);

    _floorBrushSizer->GetStaticBox()->Show(showFloor);
    _floorBrushSizer->ShowItems(showFloor);
    _wallBrushSizer->GetStaticBox()->Show(showWall);
    _wallBrushSizer->ShowItems(showWall);
    _bldgBrushSizer->GetStaticBox()->Show(showBldg);
    _bldgBrushSizer->ShowItems(showBldg);

    auto* defaultsPanel = _floorBrushSizer->GetStaticBox()->GetParent();
    defaultsPanel->Layout();
    defaultsPanel->GetParent()->Layout();
}

void CityPlannerDialog::updateSelectedGroupHeader()
{
    if (_selectedGroup >= 0 && _selectedGroup < static_cast<int>(_groups.size()))
    {
        const auto& g = _groups[_selectedGroup];
        const char* suffix = (g.type == cityplanner::CellType::WindowlessBuilding)
            ? " (windowless)" : "";
        _bldgHeaderLabel->SetLabel(
            wxString::Format("Editing Group #%d%s", _selectedGroup, suffix));
        _bldgHeaderLabel->SetForegroundColour(wxColour(160, 220, 160));
    }
    else
    {
        _bldgHeaderLabel->SetLabel("Default (new groups)");
        _bldgHeaderLabel->SetForegroundColour(wxColour(200, 200, 120));
    }
    _bldgHeaderLabel->Refresh();
    updateWindowControlsState();
}

void CityPlannerDialog::updateWindowControlsState()
{
    bool windowless = false;
    if (_selectedGroup >= 0 && _selectedGroup < static_cast<int>(_groups.size()))
        windowless = (_groups[_selectedGroup].type == cityplanner::CellType::WindowlessBuilding);

    bool enable = !windowless;
    _bpWindowMode->Enable(enable);
    _bpWindowsPerFloor->Enable(enable);
    _bpWindowWidth->Enable(enable);
    _bpWindowHeight->Enable(enable);
    _bpSillHeight->Enable(enable);
    _bpNoFirstFloorWindows->Enable(enable);
}

void CityPlannerDialog::onBrushChanged(wxCommandEvent&)
{
    _activeBrush = static_cast<Brush>(
        findNamedObject<wxChoice>(_dialog, "CityPlannerBrush")->GetSelection());
    updateBrushPanelVisibility();
}

void CityPlannerDialog::onGridSizeChanged(wxSpinEvent&)
{
    resizeGrid();
}

void CityPlannerDialog::onRandom(wxCommandEvent&)
{
    wxDialog dlg(_dialog, wxID_ANY, "City Generator",
        wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE);

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    auto* fg = new wxFlexGridSizer(2, 6, 8);
    fg->AddGrowableCol(1, 1);

    fg->Add(new wxStaticText(&dlg, wxID_ANY, "Algorithm:"), 0, wxALIGN_CENTER_VERTICAL);
    auto* algoChoice = new wxChoice(&dlg, wxID_ANY);
    algoChoice->Append("Grid City");
    algoChoice->Append("Medieval Village");
    algoChoice->Append("Downtown");
    algoChoice->Append("Industrial Park");
    algoChoice->Append("Suburban Sprawl");
    algoChoice->Append("Walled Compound");
    algoChoice->Append("Voronoi Districts");
    algoChoice->SetSelection(0);
    fg->Add(algoChoice, 1, wxEXPAND);

    fg->Add(new wxStaticText(&dlg, wxID_ANY, "Seed:"), 0, wxALIGN_CENTER_VERTICAL);
    auto* seedRow = new wxBoxSizer(wxHORIZONTAL);
    std::random_device rd;
    auto* seedCtrl = new wxSpinCtrl(&dlg, wxID_ANY, wxEmptyString,
        wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS,
        0, 999999, rd() % 1000000);
    seedRow->Add(seedCtrl, 1, wxEXPAND);
    auto* rndBtn = new wxButton(&dlg, wxID_ANY, "Rnd",
        wxDefaultPosition, wxSize(36, -1));
    rndBtn->Bind(wxEVT_BUTTON, [seedCtrl](wxCommandEvent&) {
        std::random_device r;
        seedCtrl->SetValue(r() % 1000000);
    });
    seedRow->Add(rndBtn, 0, wxLEFT, 4);
    fg->Add(seedRow, 1, wxEXPAND);

    auto* descLabel = new wxStaticText(&dlg, wxID_ANY, "");
    descLabel->SetForegroundColour(wxColour(140, 140, 140));

    auto updateDesc = [algoChoice, descLabel](wxCommandEvent&) {
        static const char* descs[] = {
            "Regular street grid: Floor avenues every few cells, Building blocks "
            "in between, occasional parks and windowless lots.",
            "Central Floor plaza, scattered Building clusters of varied sizes "
            "(some windowless), Floor paths between, half-square fence fragments.",
            "Dense grid with tight 3-cell blocks, mixed windowed and windowless, "
            "no parks. Tighter than Grid City.",
            "Perimeter Wall with a gate, large Windowless Buildings (warehouses) "
            "separated by Floor access roads.",
            "Floor backdrop with scattered small Buildings at minimum spacing. "
            "Occasional half-square fences facing the houses.",
            "Outer Wall ring with gate gaps, Floor courtyards inside, several "
            "Building rectangles scattered within.",
            "Voronoi partition: seeded regions with Building interiors and Floor "
            "borders between regions. Irregular but grid-aligned block shapes."
        };
        int sel = algoChoice->GetSelection();
        if (sel >= 0 && sel < 7)
            descLabel->SetLabel(descs[sel]);
        descLabel->Wrap(360);
        descLabel->GetParent()->Layout();
    };
    algoChoice->Bind(wxEVT_CHOICE, updateDesc);
    wxCommandEvent dummy;
    updateDesc(dummy);

    sizer->Add(fg, 0, wxEXPAND | wxALL, 12);
    sizer->Add(descLabel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    auto* btnSizer = dlg.CreateStdDialogButtonSizer(wxOK | wxCANCEL);
    sizer->Add(btnSizer, 0, wxEXPAND | wxBOTTOM | wxLEFT | wxRIGHT, 12);

    dlg.SetSizer(sizer);
    dlg.SetMinSize(wxSize(400, -1));
    dlg.Fit();

    if (dlg.ShowModal() != wxID_OK)
        return;

    int algo = algoChoice->GetSelection();
    uint32_t seed = static_cast<uint32_t>(seedCtrl->GetValue());

    cityplanner::GeneratorMaterials mats;
    mats.floor = getDefaultFloorMaterial();
    mats.wall = getDefaultWallMaterial();

    int cols = getGridCols();
    int rows = getGridRows();

    switch (algo)
    {
    case 0: cityplanner::generateGridCity(_grid, cols, rows, mats, seed); break;
    case 1: cityplanner::generateMedievalVillage(_grid, cols, rows, mats, seed); break;
    case 2: cityplanner::generateDowntown(_grid, cols, rows, mats, seed); break;
    case 3: cityplanner::generateIndustrialPark(_grid, cols, rows, mats, seed); break;
    case 4: cityplanner::generateSuburbanSprawl(_grid, cols, rows, mats, seed); break;
    case 5: cityplanner::generateWalledCompound(_grid, cols, rows, mats, seed); break;
    case 6: cityplanner::generateVoronoiDistricts(_grid, cols, rows, mats, seed); break;
    }

    _selectedGroup = -1;
    rebuildGroups();
    _canvas->Refresh();
}

void CityPlannerDialog::onParamsChanged(wxCommandEvent&)
{
    if (_suppressParamEvents) return;
    commitPanelToCurrentGroup();
    _canvas->Refresh();
}

void CityPlannerDialog::onParamsSpin(wxSpinEvent&)
{
    if (_suppressParamEvents) return;
    commitPanelToCurrentGroup();
}

void CityPlannerDialog::onParamsText(wxCommandEvent&)
{
    if (_suppressParamEvents) return;
    commitPanelToCurrentGroup();
}

void CityPlannerDialog::Show(const cmd::ArgumentList&)
{
    CityPlannerDialog dialog;

    if (dialog.run() != IDialog::RESULT_OK)
        return;

    dialog.commitPanelToCurrentGroup();
    dialog.rebuildGroups();

    int cols = dialog.getGridCols();
    int rows = dialog.getGridRows();
    float tileW = dialog.getTileWidth();
    float tileH = dialog.getTileHeight();
    float floorThickness = dialog.getFloorThickness();
    float wallHeight = dialog.getWallHeight();
    bool heightVariation = dialog.getHeightVariation();

    Vector3 spawnPos = getSpawnPosition();

    UndoableCommand undo("cityPlannerGenerate");
    GlobalSelectionSystem().setSelectedAll(false);

    scene::INodePtr worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    auto floorRects = cityplanner::mergeCellRects(
        dialog._grid, cols, rows, cityplanner::CellType::Floor);
    for (const auto& r : floorRects)
    {
        double x0 = spawnPos.x() + r.x0 * tileW;
        double y0 = spawnPos.y() + r.y0 * tileH;
        double x1 = spawnPos.x() + (r.x1 + 1) * tileW;
        double y1 = spawnPos.y() + (r.y1 + 1) * tileH;
        const std::string& mat = dialog._grid[r.y0][r.x0].material;
        auto node = tilemap::createBoxBrush(
            Vector3(x0, y0, spawnPos.z()),
            Vector3(x1, y1, spawnPos.z() + floorThickness),
            mat, worldspawn);
        if (node) Node_setSelected(node, true);
    }

    auto wallRects = cityplanner::mergeCellRects(
        dialog._grid, cols, rows, cityplanner::CellType::Wall);
    for (const auto& r : wallRects)
    {
        double x0 = spawnPos.x() + r.x0 * tileW;
        double y0 = spawnPos.y() + r.y0 * tileH;
        double x1 = spawnPos.x() + (r.x1 + 1) * tileW;
        double y1 = spawnPos.y() + (r.y1 + 1) * tileH;
        const std::string& mat = dialog._grid[r.y0][r.x0].material;
        auto node = tilemap::createBoxBrush(
            Vector3(x0, y0, spawnPos.z() + floorThickness),
            Vector3(x1, y1, spawnPos.z() + floorThickness + wallHeight),
            mat, worldspawn);
        if (node) Node_setSelected(node, true);
    }

    std::string defaultFloorMat = dialog.getDefaultFloorMaterial();

    auto emitHalfSquareRuns = [&](const std::vector<tilemap::Rect>& runs,
                                   cityplanner::CellType variant)
    {
        for (const auto& r : runs)
        {
            double cellX0 = spawnPos.x() + r.x0 * tileW;
            double cellX1 = spawnPos.x() + (r.x1 + 1) * tileW;
            double cellY0 = spawnPos.y() + r.y0 * tileH;
            double cellY1 = spawnPos.y() + (r.y1 + 1) * tileH;

            auto floorNode = tilemap::createBoxBrush(
                Vector3(cellX0, cellY0, spawnPos.z()),
                Vector3(cellX1, cellY1, spawnPos.z() + floorThickness),
                defaultFloorMat, worldspawn);
            if (floorNode) Node_setSelected(floorNode, true);

            double mnX = cellX0, mxX = cellX1, mnY = cellY0, mxY = cellY1;
            switch (variant)
            {
            case cityplanner::CellType::HalfSquareN: mnY = cellY1 - tileH * 0.5; break;
            case cityplanner::CellType::HalfSquareS: mxY = cellY0 + tileH * 0.5; break;
            case cityplanner::CellType::HalfSquareE: mnX = cellX1 - tileW * 0.5; break;
            case cityplanner::CellType::HalfSquareW: mxX = cellX0 + tileW * 0.5; break;
            default: break;
            }

            const std::string& mat = dialog._grid[r.y0][r.x0].material;
            auto node = tilemap::createBoxBrush(
                Vector3(mnX, mnY, spawnPos.z() + floorThickness),
                Vector3(mxX, mxY, spawnPos.z() + floorThickness + wallHeight),
                mat, worldspawn);
            if (node) Node_setSelected(node, true);
        }
    };

    emitHalfSquareRuns(
        cityplanner::mergeRowRuns(dialog._grid, cols, rows, cityplanner::CellType::HalfSquareN),
        cityplanner::CellType::HalfSquareN);
    emitHalfSquareRuns(
        cityplanner::mergeRowRuns(dialog._grid, cols, rows, cityplanner::CellType::HalfSquareS),
        cityplanner::CellType::HalfSquareS);
    emitHalfSquareRuns(
        cityplanner::mergeColumnRuns(dialog._grid, cols, rows, cityplanner::CellType::HalfSquareE),
        cityplanner::CellType::HalfSquareE);
    emitHalfSquareRuns(
        cityplanner::mergeColumnRuns(dialog._grid, cols, rows, cityplanner::CellType::HalfSquareW),
        cityplanner::CellType::HalfSquareW);

    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> dist(0.5, 1.5);

    const auto& cityGrid = dialog._grid;
    auto neighborType = [&](int gc, int gr) -> cityplanner::CellType {
        if (gr < 0 || gr >= static_cast<int>(cityGrid.size()))
            return cityplanner::CellType::Empty;
        if (gc < 0 || gc >= static_cast<int>(cityGrid[gr].size()))
            return cityplanner::CellType::Empty;
        return cityGrid[gr][gc].type;
    };

    auto isSolidNeighbor = [](cityplanner::CellType t) {
        return t == cityplanner::CellType::Wall ||
               cityplanner::isBuildingType(t);
    };

    for (const auto& g : dialog._groups)
    {
        building::BuildingParams params = g.params;

        if (g.type == cityplanner::CellType::WindowlessBuilding)
            params.windowsPerFloor = -1;

        if (heightVariation)
        {
            int varied = static_cast<int>(std::round(params.floorCount * dist(rng)));
            params.floorCount = std::max(1, varied);
        }

        int mw = g.x1 - g.x0 + 1;
        int mh = g.y1 - g.y0 + 1;

        building::MaskedFootprint fp;
        fp.cols = mw;
        fp.rows = mh;
        fp.tileW = tileW;
        fp.tileH = tileH;
        fp.origin = Vector3(
            spawnPos.x() + g.x0 * tileW,
            spawnPos.y() + g.y0 * tileH,
            spawnPos.z() + floorThickness);
        fp.mask.assign(mh, std::vector<bool>(mw, false));
        for (const auto& cell : g.cells)
            fp.mask[cell.second - g.y0][cell.first - g.x0] = true;

        fp.occlusion.assign(mh, std::vector<uint8_t>(mw, 0));
        for (int lr = 0; lr < mh; ++lr)
        {
            for (int lc = 0; lc < mw; ++lc)
            {
                if (!fp.mask[lr][lc]) continue;
                int gc = lc + g.x0;
                int gr = lr + g.y0;
                uint8_t bits = 0;

                auto nN = neighborType(gc, gr + 1);
                if (isSolidNeighbor(nN) || nN == cityplanner::CellType::HalfSquareS)
                    bits |= 1 << 0;

                auto nS = neighborType(gc, gr - 1);
                if (isSolidNeighbor(nS) || nS == cityplanner::CellType::HalfSquareN)
                    bits |= 1 << 1;

                auto nE = neighborType(gc + 1, gr);
                if (isSolidNeighbor(nE) || nE == cityplanner::CellType::HalfSquareW)
                    bits |= 1 << 2;

                auto nW = neighborType(gc - 1, gr);
                if (isSolidNeighbor(nW) || nW == cityplanner::CellType::HalfSquareE)
                    bits |= 1 << 3;

                fp.occlusion[lr][lc] = bits;
            }
        }

        building::generateBuilding(fp, params, worldspawn);
    }
}

// --- CityGridCanvas ---

CityGridCanvas::CityGridCanvas(wxWindow* parent, CityPlannerDialog* owner)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
              wxFULL_REPAINT_ON_RESIZE | wxBORDER_NONE),
      _owner(owner),
      _hoveredX(-1), _hoveredY(-1),
      _painting(false), _erasing(false)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);

    Bind(wxEVT_PAINT, &CityGridCanvas::onPaint, this);
    Bind(wxEVT_LEFT_DOWN, &CityGridCanvas::onMouseDown, this);
    Bind(wxEVT_LEFT_UP, &CityGridCanvas::onMouseUp, this);
    Bind(wxEVT_RIGHT_DOWN, &CityGridCanvas::onMouseDown, this);
    Bind(wxEVT_RIGHT_UP, &CityGridCanvas::onMouseUp, this);
    Bind(wxEVT_MIDDLE_DOWN, &CityGridCanvas::onMiddleDown, this);
    Bind(wxEVT_MOTION, &CityGridCanvas::onMouseMove, this);
    Bind(wxEVT_LEAVE_WINDOW, &CityGridCanvas::onMouseLeave, this);
}

int CityGridCanvas::cellSize() const
{
    return CELL_SIZE;
}

void CityGridCanvas::gridFromMouse(int mx, int my, int& gx, int& gy)
{
    gx = mx / cellSize();
    gy = my / cellSize();
}

void CityGridCanvas::onPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(wxColour(30, 30, 30)));
    dc.Clear();

    int cols = _owner->getGridCols();
    int rows = _owner->getGridRows();
    int cs = cellSize();
    auto& grid = _owner->grid();
    int selected = _owner->selectedGroup();

    for (int y = 0; y < rows && y < static_cast<int>(grid.size()); ++y)
    {
        for (int x = 0; x < cols && x < static_cast<int>(grid[y].size()); ++x)
        {
            const auto& cell = grid[y][x];
            if (cell.type == cityplanner::CellType::Empty)
                continue;

            wxColour col = cellColour(cell.type);
            if (cityplanner::isBuildingType(cell.type) &&
                cell.buildingGroupId == selected && selected >= 0)
            {
                col = wxColour(
                    std::min(255, col.Red() + 60),
                    std::min(255, col.Green() + 60),
                    std::min(255, col.Blue() + 40));
            }

            dc.SetBrush(wxBrush(col));
            dc.SetPen(*wxTRANSPARENT_PEN);

            switch (cell.type)
            {
            case cityplanner::CellType::HalfSquareN:
                dc.DrawRectangle(x * cs + 1, y * cs + 1, cs - 1, cs / 2);
                break;
            case cityplanner::CellType::HalfSquareS:
                dc.DrawRectangle(x * cs + 1, y * cs + cs / 2, cs - 1, cs / 2);
                break;
            case cityplanner::CellType::HalfSquareE:
                dc.DrawRectangle(x * cs + cs / 2, y * cs + 1, cs / 2, cs - 1);
                break;
            case cityplanner::CellType::HalfSquareW:
                dc.DrawRectangle(x * cs + 1, y * cs + 1, cs / 2, cs - 1);
                break;
            default:
                dc.DrawRectangle(x * cs + 1, y * cs + 1, cs - 1, cs - 1);
                break;
            }

            if (cityplanner::isBuildingType(cell.type))
            {
                dc.SetTextForeground(*wxWHITE);
                dc.SetFont(wxFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
                wxString s = wxString::Format("%d", cell.buildingGroupId);
                dc.DrawText(s, x * cs + 3, y * cs + 3);
            }
        }
    }

    dc.SetPen(wxPen(wxColour(60, 60, 60)));
    for (int x = 0; x <= cols; ++x)
        dc.DrawLine(x * cs, 0, x * cs, rows * cs);
    for (int y = 0; y <= rows; ++y)
        dc.DrawLine(0, y * cs, cols * cs, y * cs);

    if (_hoveredX >= 0 && _hoveredX < cols && _hoveredY >= 0 && _hoveredY < rows)
    {
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(wxColour(255, 255, 0), 2));
        dc.DrawRectangle(_hoveredX * cs, _hoveredY * cs, cs + 1, cs + 1);
    }
}

void CityGridCanvas::applyBrush(int gx, int gy)
{
    auto& grid = _owner->grid();
    if (gy < 0 || gy >= static_cast<int>(grid.size())) return;
    if (gx < 0 || gx >= static_cast<int>(grid[gy].size())) return;

    auto& cell = grid[gy][gx];
    Brush brush = _owner->activeBrush();

    switch (brush)
    {
    case Brush::Floor:
        cell.type = cityplanner::CellType::Floor;
        cell.material = _owner->getDefaultFloorMaterial();
        cell.buildingGroupId = -1;
        break;
    case Brush::Wall:
        cell.type = cityplanner::CellType::Wall;
        cell.material = _owner->getDefaultWallMaterial();
        cell.buildingGroupId = -1;
        break;
    case Brush::HalfSquareN:
    case Brush::HalfSquareS:
    case Brush::HalfSquareE:
    case Brush::HalfSquareW:
        cell.type =
            brush == Brush::HalfSquareN ? cityplanner::CellType::HalfSquareN :
            brush == Brush::HalfSquareS ? cityplanner::CellType::HalfSquareS :
            brush == Brush::HalfSquareE ? cityplanner::CellType::HalfSquareE :
                                          cityplanner::CellType::HalfSquareW;
        cell.material = _owner->getDefaultWallMaterial();
        cell.buildingGroupId = -1;
        break;
    case Brush::Building:
    case Brush::WindowlessBuilding:
        cell.type = (brush == Brush::Building)
            ? cityplanner::CellType::Building
            : cityplanner::CellType::WindowlessBuilding;
        cell.material.clear();
        _owner->rebuildGroups();
        _owner->selectGroupAt(gx, gy);
        break;
    }
}

void CityGridCanvas::applyErase(int gx, int gy)
{
    auto& grid = _owner->grid();
    if (gy < 0 || gy >= static_cast<int>(grid.size())) return;
    if (gx < 0 || gx >= static_cast<int>(grid[gy].size())) return;

    auto& cell = grid[gy][gx];
    bool wasBuilding = cityplanner::isBuildingType(cell.type);
    cell.type = cityplanner::CellType::Empty;
    cell.material.clear();
    cell.buildingGroupId = -1;

    if (wasBuilding)
        _owner->rebuildGroups();
}

void CityGridCanvas::onMouseDown(wxMouseEvent& ev)
{
    int gx, gy;
    gridFromMouse(ev.GetX(), ev.GetY(), gx, gy);

    int cols = _owner->getGridCols();
    int rows = _owner->getGridRows();
    if (gx < 0 || gx >= cols || gy < 0 || gy >= rows)
        return;

    if (ev.LeftDown())
    {
        _painting = true;
        applyBrush(gx, gy);
    }
    else if (ev.RightDown())
    {
        _erasing = true;
        applyErase(gx, gy);
    }
    Refresh();
    CaptureMouse();
}

void CityGridCanvas::onMouseUp(wxMouseEvent&)
{
    _painting = false;
    _erasing = false;
    if (HasCapture())
        ReleaseMouse();
}

void CityGridCanvas::onMouseMove(wxMouseEvent& ev)
{
    int gx, gy;
    gridFromMouse(ev.GetX(), ev.GetY(), gx, gy);

    int cols = _owner->getGridCols();
    int rows = _owner->getGridRows();
    bool validCell = (gx >= 0 && gx < cols && gy >= 0 && gy < rows);

    if (_hoveredX != gx || _hoveredY != gy)
    {
        _hoveredX = gx;
        _hoveredY = gy;
        Refresh();
    }

    if (!validCell) return;

    if (_painting)
    {
        applyBrush(gx, gy);
        Refresh();
    }
    else if (_erasing)
    {
        applyErase(gx, gy);
        Refresh();
    }
}

void CityGridCanvas::onMiddleDown(wxMouseEvent& ev)
{
    int gx, gy;
    gridFromMouse(ev.GetX(), ev.GetY(), gx, gy);

    int cols = _owner->getGridCols();
    int rows = _owner->getGridRows();
    if (gx < 0 || gx >= cols || gy < 0 || gy >= rows)
        return;

    _owner->selectGroupAt(gx, gy);
    Refresh();
}

void CityGridCanvas::onMouseLeave(wxMouseEvent&)
{
    _hoveredX = -1;
    _hoveredY = -1;
    Refresh();
}

} // namespace ui
