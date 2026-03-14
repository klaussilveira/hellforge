#include "TileMapDialog.h"
#include "TileMapGeometry.h"
#include "TileMapGenerators.h"

#include "i18n.h"
#include "ui/imainframe.h"
#include "imap.h"
#include "iselection.h"
#include "icameraview.h"
#include "ishaderclipboard.h"
#include "iundo.h"
#include "igrid.h"
#include "ientity.h"
#include "ieclass.h"
#include "iscenegraph.h"
#include "scene/Entity.h"
#include "scene/EntityNode.h"

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
#include <wx/sizer.h>
#include <wx/dcclient.h>
#include <wx/dcbuffer.h>
#include <wx/dialog.h>
#include <wx/checkbox.h>

#include <random>

#include "wxutil/Bitmap.h"
#include "ui/materials/MaterialChooser.h"
#include "ui/materials/MaterialSelector.h"

namespace
{
const char* const WINDOW_TITLE = N_("Tile Map Editor");
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

wxColour tileColour(const tilemap::Tile& tile)
{
    if (tilemap::isSolidType(tile.type))
        return wxColour(100, 160, 100);
    return wxColour(40, 40, 40);
}

void getSlopeTriangle(tilemap::TileType type, int x, int y, int cs, wxPoint pts[3])
{
    int px0 = x * cs + 1, py0 = y * cs + 1;
    int px1 = (x + 1) * cs - 1, py1 = (y + 1) * cs - 1;

    switch (type)
    {
    case tilemap::TileType::SlopeNW:
        pts[0] = wxPoint(px0, py0);
        pts[1] = wxPoint(px1, py0);
        pts[2] = wxPoint(px0, py1);
        break;
    case tilemap::TileType::SlopeNE:
        pts[0] = wxPoint(px0, py0);
        pts[1] = wxPoint(px1, py0);
        pts[2] = wxPoint(px1, py1);
        break;
    case tilemap::TileType::SlopeSW:
        pts[0] = wxPoint(px0, py0);
        pts[1] = wxPoint(px0, py1);
        pts[2] = wxPoint(px1, py1);
        break;
    case tilemap::TileType::SlopeSE:
        pts[0] = wxPoint(px1, py0);
        pts[1] = wxPoint(px0, py1);
        pts[2] = wxPoint(px1, py1);
        break;
    default:
        break;
    }
}

int getCurvePolygon(tilemap::TileType type, int x, int y, int cs, wxPoint pts[])
{
    int px0 = x * cs + 1, py0 = y * cs + 1;
    int px1 = (x + 1) * cs - 1, py1 = (y + 1) * cs - 1;
    double a = px1 - px0, b = py1 - py0;
    const double PI = 3.14159265358979323846;

    double ox, oy, startA, endA;
    switch (type)
    {
    case tilemap::TileType::CurveNW:
        ox = px0; oy = py0; startA = 0; endA = PI / 2; break;
    case tilemap::TileType::CurveNE:
        ox = px1; oy = py0; startA = PI / 2; endA = PI; break;
    case tilemap::TileType::CurveSW:
        ox = px0; oy = py1; startA = 3 * PI / 2; endA = 2 * PI; break;
    case tilemap::TileType::CurveSE:
        ox = px1; oy = py1; startA = PI; endA = 3 * PI / 2; break;
    default:
        return 0;
    }

    const int ARC_PTS = 9;
    int count = 0;
    pts[count++] = wxPoint(static_cast<int>(ox), static_cast<int>(oy));
    for (int i = 0; i < ARC_PTS; ++i)
    {
        double alpha = startA + i * (endA - startA) / (ARC_PTS - 1);
        pts[count++] = wxPoint(
            static_cast<int>(ox + a * std::cos(alpha)),
            static_cast<int>(oy + b * std::sin(alpha)));
    }
    return count;
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

namespace tilemap
{

void generateAllLights(
    const Vector3& origin,
    const std::vector<std::vector<Tile>>& grid,
    int gridW, int gridH,
    float tileW, float tileH,
    float floorZ, float ceilHeight)
{
    auto eclass = GlobalEntityClassManager().findClass("light");
    if (!eclass)
        return;

    for (int y = 0; y < gridH; ++y)
    {
        for (int x = 0; x < gridW; ++x)
        {
            if (y >= static_cast<int>(grid.size()) ||
                x >= static_cast<int>(grid[y].size()))
                continue;

            if (!grid[y][x].hasLight)
                continue;

            const auto& lp = grid[y][x].lightParams;

            float cx = origin.x() + (x + 0.5f) * tileW;
            float cy = origin.y() + (y + 0.5f) * tileH;
            float cz = floorZ + ceilHeight * 0.5f;

            scene::INodePtr node = GlobalEntityModule().createEntity(eclass);
            GlobalSceneGraph().root()->addChildNode(node);

            Entity* ent = node->tryGetEntity();
            if (ent)
            {
                ent->setKeyValue("origin", string::to_string(Vector3(cx, cy, cz)));
                ent->setKeyValue("light_radius",
                    string::to_string(Vector3(lp.radius, lp.radius, lp.radius)));
                ent->setKeyValue("_color",
                    string::to_string(lp.color));
                if (!lp.texture.empty())
                    ent->setKeyValue("texture", lp.texture);
            }

            Node_setSelected(node, true);
        }
    }
}

} // namespace tilemap

namespace ui
{

TileMapDialog::TileMapDialog()
    : Dialog(_(WINDOW_TITLE), GlobalMainFrame().getWxTopLevelWindow()),
      _scrollWin(nullptr),
      _canvas(nullptr),
      _currentFloor(0),
      _activeBrush(0),
      _gridCols(nullptr), _gridRows(nullptr),
      _wallThickness(nullptr), _roomStyle(nullptr),
      _defMaterialsSizer(nullptr), _defStairsSizer(nullptr), _defLightSizer(nullptr),
      _defFloorMat(nullptr), _defCeilingMat(nullptr), _defWallMat(nullptr),
      _defStairMat(nullptr),
      _defStepCount(nullptr), _defStepHeight(nullptr),
      _defStairDir(nullptr), _defStairSolid(nullptr),
      _defLightRadius(nullptr), _defLightColorR(nullptr),
      _defLightColorG(nullptr), _defLightColorB(nullptr),
      _defLightTexture(nullptr)
{
    _dialog->SetMinSize(wxSize(700, 750));

    auto* mainSizer = new wxBoxSizer(wxHORIZONTAL);

    auto* controlPanel = loadNamedPanel(_dialog, "TileMapControlPanel");
    mainSizer->Add(controlPanel, 0, wxEXPAND | wxALL, 6);

    auto* rightSizer = new wxBoxSizer(wxVERTICAL);

    _gridCols = findNamedObject<wxSpinCtrl>(_dialog, "TileMapGridCols");
    _gridRows = findNamedObject<wxSpinCtrl>(_dialog, "TileMapGridRows");

    auto* toolbar = new wxPanel(_dialog, wxID_ANY);
    auto* tbSizer = new wxBoxSizer(wxHORIZONTAL);

    auto addTbLabel = [&](const wxString& text, int leftPad = 8) {
        auto* lbl = new wxStaticText(toolbar, wxID_ANY, text);
        tbSizer->Add(lbl, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, leftPad);
    };

    addTbLabel("Wall Thickness:", 0);
    _wallThickness = new wxChoice(toolbar, wxID_ANY, wxDefaultPosition, wxSize(64, -1));
    _wallThickness->Append("8");
    _wallThickness->Append("16");
    _wallThickness->Append("32");
    _wallThickness->Append("64");
    _wallThickness->SetSelection(1);
    tbSizer->Add(_wallThickness, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);

    addTbLabel("Style:", 12);
    _roomStyle = new wxChoice(toolbar, wxID_ANY, wxDefaultPosition, wxSize(90, -1));
    _roomStyle->Append("Solid");
    _roomStyle->Append("Room");
    _roomStyle->Append("Shell");
    _roomStyle->Append("Passable");
    _roomStyle->SetSelection(0);
    tbSizer->Add(_roomStyle, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);

    tbSizer->AddStretchSpacer();
    auto* randomBtn = new wxButton(toolbar, wxID_ANY, "Random...");
    randomBtn->Bind(wxEVT_BUTTON, &TileMapDialog::onRandom, this);
    tbSizer->Add(randomBtn, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);

    toolbar->SetSizer(tbSizer);
    rightSizer->Add(toolbar, 0, wxEXPAND | wxALL, 6);

    _scrollWin = new wxScrolledWindow(_dialog, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxHSCROLL | wxVSCROLL);
    _scrollWin->SetScrollRate(CELL_SIZE, CELL_SIZE);

    _canvas = new TileGridCanvas(_scrollWin, this);
    int canvasW = DEFAULT_GRID_COLS * CELL_SIZE + 1;
    int canvasH = DEFAULT_GRID_ROWS * CELL_SIZE + 1;
    _canvas->SetMinSize(wxSize(canvasW, canvasH));
    _scrollWin->SetVirtualSize(canvasW, canvasH);

    auto* scrollSizer = new wxBoxSizer(wxVERTICAL);
    scrollSizer->Add(_canvas, 0, wxEXPAND);
    _scrollWin->SetSizer(scrollSizer);

    rightSizer->Add(_scrollWin, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

    mainSizer->Add(rightSizer, 1, wxEXPAND);
    _dialog->GetSizer()->Add(mainSizer, 1, wxEXPAND);

    auto* floorUpBtn = findNamedObject<wxButton>(_dialog, "TileMapFloorUp");
    floorUpBtn->SetBitmap(wxutil::GetLocalBitmap("arrow_up.png"));
    floorUpBtn->SetLabel("");
    floorUpBtn->Bind(wxEVT_BUTTON, &TileMapDialog::onFloorUp, this);

    auto* floorDownBtn = findNamedObject<wxButton>(_dialog, "TileMapFloorDown");
    floorDownBtn->SetBitmap(wxutil::GetLocalBitmap("arrow_down.png"));
    floorDownBtn->SetLabel("");
    floorDownBtn->Bind(wxEVT_BUTTON, &TileMapDialog::onFloorDown, this);

    findNamedObject<wxChoice>(_dialog, "TileMapBrush")
        ->Bind(wxEVT_CHOICE, &TileMapDialog::onBrushChanged, this);
    _gridCols->Bind(wxEVT_SPINCTRL, &TileMapDialog::onGridSizeChanged, this);
    _gridRows->Bind(wxEVT_SPINCTRL, &TileMapDialog::onGridSizeChanged, this);

    std::string defMat = getDefaultShader();

    auto* defaultsPanel = findNamedObject<wxPanel>(_dialog, "TileMapDefaultsPanel");
    auto* defSizer = new wxBoxSizer(wxVERTICAL);

    auto* matBox = new wxStaticBoxSizer(wxVERTICAL, defaultsPanel, "Default Materials");
    _defMaterialsSizer = matBox;
    auto* matGrid = new wxFlexGridSizer(2, 4, 6);
    matGrid->AddGrowableCol(1, 1);
    _defFloorMat = addMaterialRow(defaultsPanel, matGrid, "Floor:", defMat);
    _defCeilingMat = addMaterialRow(defaultsPanel, matGrid, "Ceiling:", defMat);
    _defWallMat = addMaterialRow(defaultsPanel, matGrid, "Walls:", defMat);
    matBox->Add(matGrid, 1, wxEXPAND | wxALL, 6);
    defSizer->Add(matBox, 0, wxEXPAND);

    auto* stairBox = new wxStaticBoxSizer(wxVERTICAL, defaultsPanel, "Default Stairs");
    _defStairsSizer = stairBox;
    auto* stairGrid = new wxFlexGridSizer(2, 4, 6);
    stairGrid->AddGrowableCol(1, 1);

    _defStairMat = addMaterialRow(defaultsPanel, stairGrid, "Material:", defMat);

    stairGrid->Add(new wxStaticText(defaultsPanel, wxID_ANY, "Steps:"), 0, wxALIGN_CENTER_VERTICAL);
    _defStepCount = new wxSpinCtrl(defaultsPanel, wxID_ANY, "8", wxDefaultPosition, wxDefaultSize,
        wxSP_ARROW_KEYS, 1, 64, 8);
    stairGrid->Add(_defStepCount, 1, wxEXPAND);

    stairGrid->Add(new wxStaticText(defaultsPanel, wxID_ANY, "Step Height:"), 0, wxALIGN_CENTER_VERTICAL);
    _defStepHeight = new wxTextCtrl(defaultsPanel, wxID_ANY, "16");
    stairGrid->Add(_defStepHeight, 1, wxEXPAND);

    stairGrid->Add(new wxStaticText(defaultsPanel, wxID_ANY, "Direction:"), 0, wxALIGN_CENTER_VERTICAL);
    _defStairDir = new wxChoice(defaultsPanel, wxID_ANY);
    _defStairDir->Append("East (+X)");
    _defStairDir->Append("North (+Y)");
    _defStairDir->Append("West (-X)");
    _defStairDir->Append("South (-Y)");
    _defStairDir->SetSelection(0);
    stairGrid->Add(_defStairDir, 1, wxEXPAND);

    stairGrid->Add(new wxStaticText(defaultsPanel, wxID_ANY, "Solid:"), 0, wxALIGN_CENTER_VERTICAL);
    _defStairSolid = new wxCheckBox(defaultsPanel, wxID_ANY, "From base");
    _defStairSolid->SetValue(true);
    stairGrid->Add(_defStairSolid, 1, wxEXPAND);

    stairBox->Add(stairGrid, 1, wxEXPAND | wxALL, 6);
    defSizer->Add(stairBox, 0, wxEXPAND | wxTOP, 6);

    auto* lightBox = new wxStaticBoxSizer(wxVERTICAL, defaultsPanel, "Default Light");
    _defLightSizer = lightBox;
    auto* lightGrid = new wxFlexGridSizer(2, 4, 6);
    lightGrid->AddGrowableCol(1, 1);

    lightGrid->Add(new wxStaticText(defaultsPanel, wxID_ANY, "Radius:"), 0, wxALIGN_CENTER_VERTICAL);
    _defLightRadius = new wxTextCtrl(defaultsPanel, wxID_ANY, "128");
    lightGrid->Add(_defLightRadius, 1, wxEXPAND);

    lightGrid->Add(new wxStaticText(defaultsPanel, wxID_ANY, "Color R:"), 0, wxALIGN_CENTER_VERTICAL);
    _defLightColorR = new wxTextCtrl(defaultsPanel, wxID_ANY, "1.0");
    lightGrid->Add(_defLightColorR, 1, wxEXPAND);

    lightGrid->Add(new wxStaticText(defaultsPanel, wxID_ANY, "Color G:"), 0, wxALIGN_CENTER_VERTICAL);
    _defLightColorG = new wxTextCtrl(defaultsPanel, wxID_ANY, "1.0");
    lightGrid->Add(_defLightColorG, 1, wxEXPAND);

    lightGrid->Add(new wxStaticText(defaultsPanel, wxID_ANY, "Color B:"), 0, wxALIGN_CENTER_VERTICAL);
    _defLightColorB = new wxTextCtrl(defaultsPanel, wxID_ANY, "1.0");
    lightGrid->Add(_defLightColorB, 1, wxEXPAND);

    _defLightTexture = addMaterialRow(defaultsPanel, lightGrid, "Texture:", "lights/defaultPointLight");

    lightBox->Add(lightGrid, 1, wxEXPAND | wxALL, 6);
    defSizer->Add(lightBox, 0, wxEXPAND | wxTOP, 6);

    defaultsPanel->SetSizer(defSizer);

    ensureFloorExists(0);
    updateFloorLabel();
    updateDefaultsPanelVisibility();
}

float TileMapDialog::getTileWidth()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "TileMapTileWidth")->GetValue().ToStdString(), 128.0f);
}

float TileMapDialog::getTileHeight()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "TileMapTileHeight")->GetValue().ToStdString(), 128.0f);
}

float TileMapDialog::getFloorGap()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "TileMapFloorGap")->GetValue().ToStdString(), 0.0f);
}

float TileMapDialog::getCeilingHeight()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "TileMapCeilingHeight")->GetValue().ToStdString(), 128.0f);
}

int TileMapDialog::getGridCols()
{
    return _gridCols->GetValue();
}

int TileMapDialog::getGridRows()
{
    return _gridRows->GetValue();
}

int TileMapDialog::getRoomStyle()
{
    return _roomStyle->GetSelection();
}

int TileMapDialog::getWallThickness()
{
    static const int values[] = { 8, 16, 32, 64 };
    return values[_wallThickness->GetSelection()];
}

tilemap::TileMaterials TileMapDialog::getDefaultMaterials()
{
    std::string wall = _defWallMat->GetValue().ToStdString();
    return {
        _defFloorMat->GetValue().ToStdString(),
        _defCeilingMat->GetValue().ToStdString(),
        wall, wall, wall, wall
    };
}

tilemap::StairsParams TileMapDialog::getDefaultStairsParams()
{
    return {
        _defStepCount->GetValue(),
        string::convert<float>(_defStepHeight->GetValue().ToStdString(), 16.0f),
        _defStairSolid->GetValue(),
        _defStairDir->GetSelection(),
        _defStairMat->GetValue().ToStdString()
    };
}

tilemap::LightParams TileMapDialog::getDefaultLightParams()
{
    return {
        string::convert<float>(_defLightRadius->GetValue().ToStdString(), 128.0f),
        Vector3(
            string::convert<float>(_defLightColorR->GetValue().ToStdString(), 1.0f),
            string::convert<float>(_defLightColorG->GetValue().ToStdString(), 1.0f),
            string::convert<float>(_defLightColorB->GetValue().ToStdString(), 1.0f)),
        _defLightTexture->GetValue().ToStdString()
    };
}

std::vector<std::vector<tilemap::Tile>>& TileMapDialog::currentFloorGrid()
{
    ensureFloorExists(_currentFloor);
    return _floors[_currentFloor];
}

void TileMapDialog::ensureFloorExists(int floor)
{
    while (static_cast<int>(_floors.size()) <= floor)
    {
        int cols = getGridCols();
        int rows = getGridRows();
        _floors.emplace_back(rows, std::vector<tilemap::Tile>(cols));
    }
}

void TileMapDialog::resizeGrids()
{
    int cols = getGridCols();
    int rows = getGridRows();

    for (auto& floor : _floors)
    {
        floor.resize(rows);
        for (auto& row : floor)
            row.resize(cols);
    }

    int canvasW = cols * CELL_SIZE + 1;
    int canvasH = rows * CELL_SIZE + 1;
    _canvas->SetMinSize(wxSize(canvasW, canvasH));
    _scrollWin->SetVirtualSize(canvasW, canvasH);
    _scrollWin->FitInside();
    _canvas->Refresh();
}

void TileMapDialog::updateFloorLabel()
{
    auto* label = findNamedObject<wxStaticText>(_dialog, "TileMapFloorLabel");
    label->SetLabel(wxString::Format("Floor %d", _currentFloor));
}

void TileMapDialog::onFloorUp(wxCommandEvent& ev)
{
    ++_currentFloor;
    ensureFloorExists(_currentFloor);
    updateFloorLabel();
    _canvas->Refresh();
}

void TileMapDialog::onFloorDown(wxCommandEvent& ev)
{
    if (_currentFloor > 0)
    {
        --_currentFloor;
        updateFloorLabel();
        _canvas->Refresh();
    }
}

void TileMapDialog::onBrushChanged(wxCommandEvent& ev)
{
    _activeBrush = findNamedObject<wxChoice>(_dialog, "TileMapBrush")->GetSelection();
    updateDefaultsPanelVisibility();
}

void TileMapDialog::updateDefaultsPanelVisibility()
{
    bool showMats = (_activeBrush == 0 || (_activeBrush >= 3 && _activeBrush <= 10));
    bool showStairs = (_activeBrush == 1);
    bool showLight = (_activeBrush == 2);

    _defMaterialsSizer->GetStaticBox()->Show(showMats);
    _defMaterialsSizer->ShowItems(showMats);
    _defStairsSizer->GetStaticBox()->Show(showStairs);
    _defStairsSizer->ShowItems(showStairs);
    _defLightSizer->GetStaticBox()->Show(showLight);
    _defLightSizer->ShowItems(showLight);

    auto* defaultsPanel = _defMaterialsSizer->GetStaticBox()->GetParent();
    defaultsPanel->Layout();
    defaultsPanel->GetParent()->Layout();
}

void TileMapDialog::onGridSizeChanged(wxSpinEvent& ev)
{
    resizeGrids();
}

void TileMapDialog::onRandom(wxCommandEvent& ev)
{
    wxDialog dlg(_dialog, wxID_ANY, "Random Generator",
        wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE);

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    auto* grid = new wxFlexGridSizer(2, 6, 8);
    grid->AddGrowableCol(1, 1);

    grid->Add(new wxStaticText(&dlg, wxID_ANY, "Algorithm:"), 0, wxALIGN_CENTER_VERTICAL);
    auto* algoChoice = new wxChoice(&dlg, wxID_ANY);
    algoChoice->Append("Cellular Automata");
    algoChoice->Append("BSP Dungeon");
    algoChoice->Append("L-System");
    algoChoice->Append("Maze");
    algoChoice->Append("Deathmatch");
    algoChoice->Append("Random Noise");
    algoChoice->SetSelection(0);
    grid->Add(algoChoice, 1, wxEXPAND);

    grid->Add(new wxStaticText(&dlg, wxID_ANY, "Seed:"), 0, wxALIGN_CENTER_VERTICAL);
    auto* seedRow = new wxBoxSizer(wxHORIZONTAL);
    std::random_device rd;
    auto* seedCtrl = new wxSpinCtrl(&dlg, wxID_ANY, wxEmptyString,
        wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS,
        0, 999999, rd() % 1000000);
    seedRow->Add(seedCtrl, 1, wxEXPAND);
    auto* rndBtn = new wxButton(&dlg, wxID_ANY, "Rnd", wxDefaultPosition, wxSize(36, -1));
    rndBtn->Bind(wxEVT_BUTTON, [seedCtrl](wxCommandEvent&) {
        std::random_device r;
        seedCtrl->SetValue(r() % 1000000);
    });
    seedRow->Add(rndBtn, 0, wxLEFT, 4);
    grid->Add(seedRow, 1, wxEXPAND);

    auto* descLabel = new wxStaticText(&dlg, wxID_ANY, "");
    descLabel->SetForegroundColour(wxColour(140, 140, 140));

    auto updateDesc = [algoChoice, descLabel](wxCommandEvent&) {
        static const char* descs[] = {
            "Creates island-like cave formations using cellular automata rules.",
            "Generates dungeon rooms connected by corridors using binary space partitioning.",
            "Creates road-like networks using string rewrite rules.",
            "Generates a maze using recursive backtracking.",
            "Loop-based layouts with hubs and cross-connections. No dead ends.",
            "Random fill. Combine with expand and smooth modifiers."
        };
        int sel = algoChoice->GetSelection();
        if (sel >= 0 && sel < 6)
            descLabel->SetLabel(descs[sel]);
        descLabel->GetParent()->Layout();
    };
    algoChoice->Bind(wxEVT_CHOICE, updateDesc);
    wxCommandEvent dummy;
    updateDesc(dummy);

    sizer->Add(grid, 0, wxEXPAND | wxALL, 12);
    sizer->Add(descLabel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    auto* btnSizer = dlg.CreateStdDialogButtonSizer(wxOK | wxCANCEL);
    sizer->Add(btnSizer, 0, wxEXPAND | wxBOTTOM | wxLEFT | wxRIGHT, 12);

    dlg.SetSizer(sizer);
    dlg.SetMinSize(wxSize(340, -1));
    dlg.Fit();

    if (dlg.ShowModal() != wxID_OK)
        return;

    int algo = algoChoice->GetSelection();
    uint32_t seed = static_cast<uint32_t>(seedCtrl->GetValue());
    int cols = getGridCols();
    int rows = getGridRows();
    auto materials = getDefaultMaterials();

    auto& floorGrid = currentFloorGrid();

    switch (algo)
    {
    case 0: tilemap::generateCellularAutomata(floorGrid, cols, rows, materials, seed); break;
    case 1: tilemap::generateBSPDungeon(floorGrid, cols, rows, materials, seed); break;
    case 2: tilemap::generateLSystem(floorGrid, cols, rows, materials, seed); break;
    case 3: tilemap::generateMaze(floorGrid, cols, rows, materials, seed); break;
    case 4: tilemap::generateDeathmatch(floorGrid, cols, rows, materials, seed); break;
    case 5: tilemap::generateRandomNoise(floorGrid, cols, rows, materials, seed); break;
    }

    _canvas->Refresh();
}

void TileMapDialog::Show(const cmd::ArgumentList& args)
{
    TileMapDialog dialog;

    if (dialog.run() != IDialog::RESULT_OK)
        return;

    float tileW = dialog.getTileWidth();
    float tileH = dialog.getTileHeight();
    float floorGap = dialog.getFloorGap();
    float ceilH = dialog.getCeilingHeight();
    int cols = dialog.getGridCols();
    int rows = dialog.getGridRows();
    int roomStyle = dialog.getRoomStyle();
    int wallThickness = dialog.getWallThickness();

    Vector3 spawnPos = getSpawnPosition();

    {
        UndoableCommand undo("tileMapGenerate");
        GlobalSelectionSystem().setSelectedAll(false);

        scene::INodePtr worldspawn = GlobalMapModule().findOrInsertWorldspawn();

        for (int f = 0; f < static_cast<int>(dialog._floors.size()); ++f)
        {
            float baseZ = f * (ceilH + floorGap);

            tilemap::generateRoomBrushes(
                spawnPos, dialog._floors[f],
                cols, rows,
                tileW, tileH,
                baseZ, ceilH,
                worldspawn);
        }
    }

    if (roomStyle > 0 && GlobalSelectionSystem().countSelected() > 0)
    {
        GridSize thicknessGrid;
        switch (wallThickness)
        {
        case 8:  thicknessGrid = GRID_8; break;
        case 32: thicknessGrid = GRID_32; break;
        case 64: thicknessGrid = GRID_64; break;
        default: thicknessGrid = GRID_16; break;
        }

        GridSize savedGrid = static_cast<GridSize>(GlobalGrid().getGridPower());
        GlobalGrid().setGridSize(thicknessGrid);

        switch (roomStyle)
        {
        case 1: GlobalCommandSystem().executeCommand("CSGRoom"); break;
        case 2: GlobalCommandSystem().executeCommand("CSGShell"); break;
        case 3: GlobalCommandSystem().executeCommand("CSGPassable"); break;
        }

        GlobalGrid().setGridSize(savedGrid);
    }

    bool hasAnyStairs = false;
    bool hasAnyLights = false;
    bool hasAnyCurves = false;
    for (const auto& floor : dialog._floors)
        for (const auto& row : floor)
            for (const auto& tile : row)
            {
                if (tile.hasStairs) hasAnyStairs = true;
                if (tile.hasLight) hasAnyLights = true;
                if (tilemap::isCurveType(tile.type)) hasAnyCurves = true;
            }

    if (hasAnyCurves)
    {
        UndoableCommand undo("tileMapCurves");
        GlobalSelectionSystem().setSelectedAll(false);

        scene::INodePtr worldspawn = GlobalMapModule().findOrInsertWorldspawn();

        for (int f = 0; f < static_cast<int>(dialog._floors.size()); ++f)
        {
            float baseZ = f * (ceilH + floorGap);

            tilemap::generateAllCurvePatches(
                spawnPos, dialog._floors[f],
                cols, rows,
                tileW, tileH,
                baseZ, ceilH,
                worldspawn);
        }
    }

    if (hasAnyStairs)
    {
        UndoableCommand undo("tileMapStairs");
        GlobalSelectionSystem().setSelectedAll(false);

        scene::INodePtr worldspawn = GlobalMapModule().findOrInsertWorldspawn();

        for (int f = 0; f < static_cast<int>(dialog._floors.size()); ++f)
        {
            float baseZ = f * (ceilH + floorGap);

            tilemap::generateAllStairs(
                spawnPos, dialog._floors[f],
                cols, rows,
                tileW, tileH,
                baseZ, worldspawn);
        }
    }

    if (hasAnyLights)
    {
        UndoableCommand undo("tileMapLights");
        GlobalSelectionSystem().setSelectedAll(false);

        for (int f = 0; f < static_cast<int>(dialog._floors.size()); ++f)
        {
            float baseZ = f * (ceilH + floorGap);

            tilemap::generateAllLights(
                spawnPos, dialog._floors[f],
                cols, rows,
                tileW, tileH,
                baseZ, ceilH);
        }
    }
}

// --- TileGridCanvas ---

TileGridCanvas::TileGridCanvas(wxWindow* parent, TileMapDialog* owner)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
              wxFULL_REPAINT_ON_RESIZE | wxBORDER_NONE),
      _owner(owner),
      _hoveredX(-1), _hoveredY(-1),
      _painting(false), _erasing(false)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);

    Bind(wxEVT_PAINT, &TileGridCanvas::onPaint, this);
    Bind(wxEVT_LEFT_DOWN, &TileGridCanvas::onMouseDown, this);
    Bind(wxEVT_LEFT_UP, &TileGridCanvas::onMouseUp, this);
    Bind(wxEVT_RIGHT_DOWN, &TileGridCanvas::onMouseDown, this);
    Bind(wxEVT_RIGHT_UP, &TileGridCanvas::onMouseUp, this);
    Bind(wxEVT_MIDDLE_DOWN, &TileGridCanvas::onMiddleDown, this);
    Bind(wxEVT_MOUSEWHEEL, &TileGridCanvas::onMouseWheel, this);
    Bind(wxEVT_MOTION, &TileGridCanvas::onMouseMove, this);
    Bind(wxEVT_LEAVE_WINDOW, &TileGridCanvas::onMouseLeave, this);
}

int TileGridCanvas::cellSize() const
{
    return CELL_SIZE;
}

void TileGridCanvas::gridFromMouse(int mx, int my, int& gx, int& gy)
{
    gx = mx / cellSize();
    gy = my / cellSize();
}

void TileGridCanvas::onPaint(wxPaintEvent& ev)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(wxColour(30, 30, 30)));
    dc.Clear();

    int cols = _owner->getGridCols();
    int rows = _owner->getGridRows();
    int cs = cellSize();
    int curFloor = _owner->currentFloor();
    const auto& allFloors = _owner->floors();
    int bgR = 30, bgG = 30, bgB = 30;

    const int maxGhostDist = 4;

    for (int f = 0; f < _owner->floorCount(); ++f)
    {
        if (f == curFloor)
            continue;

        int dist = std::abs(f - curFloor);
        if (dist > maxGhostDist)
            continue;

        double alpha = 0.15 / dist;
        const auto& fgrid = allFloors[f];

        for (int y = 0; y < rows && y < static_cast<int>(fgrid.size()); ++y)
        {
            for (int x = 0; x < cols && x < static_cast<int>(fgrid[y].size()); ++x)
            {
                if (fgrid[y][x].type == tilemap::TileType::Empty)
                    continue;

                wxColour tc = tileColour(fgrid[y][x]);
                int r = bgR + static_cast<int>((tc.Red() - bgR) * alpha);
                int g = bgG + static_cast<int>((tc.Green() - bgG) * alpha);
                int b = bgB + static_cast<int>((tc.Blue() - bgB) * alpha);
                dc.SetBrush(wxBrush(wxColour(r, g, b)));
                dc.SetPen(*wxTRANSPARENT_PEN);
                if (tilemap::isSlopeType(fgrid[y][x].type))
                {
                    wxPoint pts[3];
                    getSlopeTriangle(fgrid[y][x].type, x, y, cs, pts);
                    dc.DrawPolygon(3, pts);
                }
                else if (tilemap::isCurveType(fgrid[y][x].type))
                {
                    wxPoint pts[10];
                    int count = getCurvePolygon(fgrid[y][x].type, x, y, cs, pts);
                    dc.DrawPolygon(count, pts);
                }
                else
                {
                    dc.DrawRectangle(x * cs + 1, y * cs + 1, cs - 1, cs - 1);
                }
            }
        }
    }

    auto& grid = _owner->currentFloorGrid();
    for (int y = 0; y < rows && y < static_cast<int>(grid.size()); ++y)
    {
        for (int x = 0; x < cols && x < static_cast<int>(grid[y].size()); ++x)
        {
            if (grid[y][x].type == tilemap::TileType::Empty)
                continue;

            wxColour col = tileColour(grid[y][x]);
            dc.SetBrush(wxBrush(col));
            dc.SetPen(*wxTRANSPARENT_PEN);
            if (tilemap::isSlopeType(grid[y][x].type))
            {
                wxPoint pts[3];
                getSlopeTriangle(grid[y][x].type, x, y, cs, pts);
                dc.DrawPolygon(3, pts);
            }
            else if (tilemap::isCurveType(grid[y][x].type))
            {
                wxPoint pts[10];
                int count = getCurvePolygon(grid[y][x].type, x, y, cs, pts);
                dc.DrawPolygon(count, pts);
            }
            else
            {
                dc.DrawRectangle(x * cs + 1, y * cs + 1, cs - 1, cs - 1);
            }

            if (grid[y][x].hasStairs && grid[y][x].hasLight)
            {
                dc.SetTextForeground(*wxWHITE);
                dc.SetFont(wxFont(6, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
                dc.DrawText("S", x * cs + 2, y * cs + cs / 2 - 5);
                dc.SetTextForeground(wxColour(255, 255, 100));
                dc.DrawText("L", x * cs + cs - 10, y * cs + cs / 2 - 5);
            }
            else if (grid[y][x].hasStairs)
            {
                dc.SetTextForeground(*wxWHITE);
                dc.SetFont(wxFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
                dc.DrawText("S", x * cs + cs / 2 - 3, y * cs + cs / 2 - 5);
            }
            else if (grid[y][x].hasLight)
            {
                dc.SetTextForeground(wxColour(255, 255, 100));
                dc.SetFont(wxFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
                dc.DrawText("L", x * cs + cs / 2 - 3, y * cs + cs / 2 - 5);
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

void TileGridCanvas::onMouseDown(wxMouseEvent& ev)
{
    int gx, gy;
    gridFromMouse(ev.GetX(), ev.GetY(), gx, gy);

    auto& grid = _owner->currentFloorGrid();
    int cols = _owner->getGridCols();
    int rows = _owner->getGridRows();

    if (gx < 0 || gx >= cols || gy < 0 || gy >= rows)
        return;

    if (ev.LeftDown())
    {
        _painting = true;
        auto& tile = grid[gy][gx];
        int brush = _owner->activeBrush();
        if (brush == 0)
        {
            tile.type = tilemap::TileType::Room;
            tile.materials = _owner->getDefaultMaterials();
        }
        else if (brush == 1)
        {
            if (tilemap::isSolidType(tile.type))
            {
                tile.hasStairs = true;
                tile.stairsParams = _owner->getDefaultStairsParams();
            }
        }
        else if (brush == 2)
        {
            if (tilemap::isSolidType(tile.type))
            {
                tile.hasLight = true;
                tile.lightParams = _owner->getDefaultLightParams();
            }
        }
        else if (brush >= 3 && brush <= 6)
        {
            static const tilemap::TileType slopeTypes[] = {
                tilemap::TileType::SlopeNW, tilemap::TileType::SlopeNE,
                tilemap::TileType::SlopeSW, tilemap::TileType::SlopeSE
            };
            tile.type = slopeTypes[brush - 3];
            tile.materials = _owner->getDefaultMaterials();
        }
        else if (brush >= 7 && brush <= 10)
        {
            static const tilemap::TileType curveTypes[] = {
                tilemap::TileType::CurveNW, tilemap::TileType::CurveNE,
                tilemap::TileType::CurveSW, tilemap::TileType::CurveSE
            };
            tile.type = curveTypes[brush - 7];
            tile.materials = _owner->getDefaultMaterials();
        }
        Refresh();
    }
    else if (ev.RightDown())
    {
        _erasing = true;
        auto& tile = grid[gy][gx];
        int brush = _owner->activeBrush();
        if (brush == 0 || (brush >= 3 && brush <= 10))
        {
            tile.type = tilemap::TileType::Empty;
            tile.hasStairs = false;
            tile.hasLight = false;
        }
        else if (brush == 1)
        {
            tile.hasStairs = false;
        }
        else if (brush == 2)
        {
            tile.hasLight = false;
        }
        Refresh();
    }

    CaptureMouse();
}

void TileGridCanvas::onMouseUp(wxMouseEvent& ev)
{
    _painting = false;
    _erasing = false;
    if (HasCapture())
        ReleaseMouse();
}

void TileGridCanvas::onMouseMove(wxMouseEvent& ev)
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

    if (!validCell)
        return;

    auto& grid = _owner->currentFloorGrid();

    if (_painting)
    {
        auto& tile = grid[gy][gx];
        int brush = _owner->activeBrush();
        if (brush == 0)
        {
            tile.type = tilemap::TileType::Room;
            tile.materials = _owner->getDefaultMaterials();
        }
        else if (brush == 1)
        {
            if (tilemap::isSolidType(tile.type))
            {
                tile.hasStairs = true;
                tile.stairsParams = _owner->getDefaultStairsParams();
            }
        }
        else if (brush == 2)
        {
            if (tilemap::isSolidType(tile.type))
            {
                tile.hasLight = true;
                tile.lightParams = _owner->getDefaultLightParams();
            }
        }
        else if (brush >= 3 && brush <= 6)
        {
            static const tilemap::TileType slopeTypes[] = {
                tilemap::TileType::SlopeNW, tilemap::TileType::SlopeNE,
                tilemap::TileType::SlopeSW, tilemap::TileType::SlopeSE
            };
            tile.type = slopeTypes[brush - 3];
            tile.materials = _owner->getDefaultMaterials();
        }
        else if (brush >= 7 && brush <= 10)
        {
            static const tilemap::TileType curveTypes[] = {
                tilemap::TileType::CurveNW, tilemap::TileType::CurveNE,
                tilemap::TileType::CurveSW, tilemap::TileType::CurveSE
            };
            tile.type = curveTypes[brush - 7];
            tile.materials = _owner->getDefaultMaterials();
        }
        Refresh();
    }
    else if (_erasing)
    {
        auto& tile = grid[gy][gx];
        int brush = _owner->activeBrush();
        if (brush == 0 || (brush >= 3 && brush <= 10))
        {
            tile.type = tilemap::TileType::Empty;
            tile.hasStairs = false;
            tile.hasLight = false;
        }
        else if (brush == 1)
        {
            tile.hasStairs = false;
        }
        else if (brush == 2)
        {
            tile.hasLight = false;
        }
        Refresh();
    }
}

void TileGridCanvas::onMiddleDown(wxMouseEvent& ev)
{
    int gx, gy;
    gridFromMouse(ev.GetX(), ev.GetY(), gx, gy);

    int cols = _owner->getGridCols();
    int rows = _owner->getGridRows();

    if (gx < 0 || gx >= cols || gy < 0 || gy >= rows)
        return;

    showTileProperties(gx, gy);
}

void TileGridCanvas::onMouseWheel(wxMouseEvent& ev)
{
    int gx, gy;
    gridFromMouse(ev.GetX(), ev.GetY(), gx, gy);

    int cols = _owner->getGridCols();
    int rows = _owner->getGridRows();

    if (gx < 0 || gx >= cols || gy < 0 || gy >= rows)
        return;

    auto& grid = _owner->currentFloorGrid();
    auto& tile = grid[gy][gx];

    if (!tilemap::isSolidType(tile.type))
        return;

    static const tilemap::TileType cycle[] = {
        tilemap::TileType::Room,
        tilemap::TileType::SlopeNW,
        tilemap::TileType::CurveNW,
        tilemap::TileType::SlopeNE,
        tilemap::TileType::CurveNE,
        tilemap::TileType::SlopeSW,
        tilemap::TileType::CurveSW,
        tilemap::TileType::SlopeSE,
        tilemap::TileType::CurveSE
    };
    static const int count = sizeof(cycle) / sizeof(cycle[0]);

    int cur = 0;
    for (int i = 0; i < count; ++i)
    {
        if (cycle[i] == tile.type)
        {
            cur = i;
            break;
        }
    }

    int dir = (ev.GetWheelRotation() > 0) ? 1 : -1;
    cur = (cur + dir + count) % count;
    tile.type = cycle[cur];

    Refresh();
}

void TileGridCanvas::onMouseLeave(wxMouseEvent& ev)
{
    _hoveredX = -1;
    _hoveredY = -1;
    Refresh();
}

void TileGridCanvas::showTileProperties(int gx, int gy)
{
    auto& grid = _owner->currentFloorGrid();
    auto& tile = grid[gy][gx];

    if (tile.type == tilemap::TileType::Empty)
        return;

    wxString typeName = "Room";
    if (tile.type == tilemap::TileType::SlopeNW) typeName = "Slope NW";
    else if (tile.type == tilemap::TileType::SlopeNE) typeName = "Slope NE";
    else if (tile.type == tilemap::TileType::SlopeSW) typeName = "Slope SW";
    else if (tile.type == tilemap::TileType::SlopeSE) typeName = "Slope SE";
    else if (tile.type == tilemap::TileType::CurveNW) typeName = "Curve NW";
    else if (tile.type == tilemap::TileType::CurveNE) typeName = "Curve NE";
    else if (tile.type == tilemap::TileType::CurveSW) typeName = "Curve SW";
    else if (tile.type == tilemap::TileType::CurveSE) typeName = "Curve SE";

    wxString overlays;
    if (tile.hasStairs) overlays += " + Stairs";
    if (tile.hasLight) overlays += " + Light";

    wxDialog propDlg(GetParent(), wxID_ANY,
        wxString::Format("Tile (%d, %d) - %s%s", gx, gy, typeName, overlays),
        wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    auto* gridSizer = new wxFlexGridSizer(2, 6, 12);
    gridSizer->AddGrowableCol(1, 1);

    auto* floorMat = addMaterialRow(&propDlg, gridSizer, "Floor:", tile.materials.floor);
    auto* ceilMat = addMaterialRow(&propDlg, gridSizer, "Ceiling:", tile.materials.ceiling);
    auto* wallNMat = addMaterialRow(&propDlg, gridSizer, "Wall North:", tile.materials.wallNorth);
    auto* wallSMat = addMaterialRow(&propDlg, gridSizer, "Wall South:", tile.materials.wallSouth);
    auto* wallEMat = addMaterialRow(&propDlg, gridSizer, "Wall East:", tile.materials.wallEast);
    auto* wallWMat = addMaterialRow(&propDlg, gridSizer, "Wall West:", tile.materials.wallWest);

    wxTextCtrl* stairMatCtrl = nullptr;
    wxSpinCtrl* stepCountCtrl = nullptr;
    wxTextCtrl* stepHeightCtrl = nullptr;
    wxChoice* dirChoice = nullptr;
    wxCheckBox* solidCheck = nullptr;
    wxTextCtrl* lightRadiusCtrl = nullptr;
    wxTextCtrl* lightColorRCtrl = nullptr;
    wxTextCtrl* lightColorGCtrl = nullptr;
    wxTextCtrl* lightColorBCtrl = nullptr;
    wxTextCtrl* lightTextureCtrl = nullptr;

    if (tile.hasStairs)
    {
        gridSizer->Add(new wxStaticText(&propDlg, wxID_ANY, ""), 0);
        auto* stairLabel = new wxStaticText(&propDlg, wxID_ANY, "--- Stairs ---");
        stairLabel->SetForegroundColour(wxColour(180, 150, 80));
        gridSizer->Add(stairLabel, 0, wxALIGN_CENTER_HORIZONTAL);

        stairMatCtrl = addMaterialRow(&propDlg, gridSizer, "Stair Mat:", tile.stairsParams.material);

        gridSizer->Add(new wxStaticText(&propDlg, wxID_ANY, "Step Count:"), 0, wxALIGN_CENTER_VERTICAL);
        stepCountCtrl = new wxSpinCtrl(&propDlg, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
            wxSP_ARROW_KEYS, 1, 64, tile.stairsParams.stepCount);
        gridSizer->Add(stepCountCtrl, 1, wxEXPAND);

        gridSizer->Add(new wxStaticText(&propDlg, wxID_ANY, "Step Height:"), 0, wxALIGN_CENTER_VERTICAL);
        stepHeightCtrl = new wxTextCtrl(&propDlg, wxID_ANY,
            std::to_string(static_cast<int>(tile.stairsParams.stepHeight)));
        gridSizer->Add(stepHeightCtrl, 1, wxEXPAND);

        gridSizer->Add(new wxStaticText(&propDlg, wxID_ANY, "Direction:"), 0, wxALIGN_CENTER_VERTICAL);
        dirChoice = new wxChoice(&propDlg, wxID_ANY);
        dirChoice->Append("East (+X)");
        dirChoice->Append("North (+Y)");
        dirChoice->Append("West (-X)");
        dirChoice->Append("South (-Y)");
        dirChoice->SetSelection(tile.stairsParams.direction);
        gridSizer->Add(dirChoice, 1, wxEXPAND);

        gridSizer->Add(new wxStaticText(&propDlg, wxID_ANY, "Solid:"), 0, wxALIGN_CENTER_VERTICAL);
        solidCheck = new wxCheckBox(&propDlg, wxID_ANY, "Steps extend from base");
        solidCheck->SetValue(tile.stairsParams.solid);
        gridSizer->Add(solidCheck, 1, wxEXPAND);
    }

    if (tile.hasLight)
    {
        gridSizer->Add(new wxStaticText(&propDlg, wxID_ANY, ""), 0);
        auto* lightLabel = new wxStaticText(&propDlg, wxID_ANY, "--- Light ---");
        lightLabel->SetForegroundColour(wxColour(255, 255, 100));
        gridSizer->Add(lightLabel, 0, wxALIGN_CENTER_HORIZONTAL);

        gridSizer->Add(new wxStaticText(&propDlg, wxID_ANY, "Radius:"), 0, wxALIGN_CENTER_VERTICAL);
        lightRadiusCtrl = new wxTextCtrl(&propDlg, wxID_ANY,
            std::to_string(static_cast<int>(tile.lightParams.radius)));
        gridSizer->Add(lightRadiusCtrl, 1, wxEXPAND);

        gridSizer->Add(new wxStaticText(&propDlg, wxID_ANY, "Color R:"), 0, wxALIGN_CENTER_VERTICAL);
        lightColorRCtrl = new wxTextCtrl(&propDlg, wxID_ANY,
            wxString::Format("%.2f", tile.lightParams.color.x()));
        gridSizer->Add(lightColorRCtrl, 1, wxEXPAND);

        gridSizer->Add(new wxStaticText(&propDlg, wxID_ANY, "Color G:"), 0, wxALIGN_CENTER_VERTICAL);
        lightColorGCtrl = new wxTextCtrl(&propDlg, wxID_ANY,
            wxString::Format("%.2f", tile.lightParams.color.y()));
        gridSizer->Add(lightColorGCtrl, 1, wxEXPAND);

        gridSizer->Add(new wxStaticText(&propDlg, wxID_ANY, "Color B:"), 0, wxALIGN_CENTER_VERTICAL);
        lightColorBCtrl = new wxTextCtrl(&propDlg, wxID_ANY,
            wxString::Format("%.2f", tile.lightParams.color.z()));
        gridSizer->Add(lightColorBCtrl, 1, wxEXPAND);

        lightTextureCtrl = addMaterialRow(&propDlg, gridSizer, "Texture:", tile.lightParams.texture);
    }

    sizer->Add(gridSizer, 1, wxEXPAND | wxALL, 12);

    auto* btnSizer = propDlg.CreateStdDialogButtonSizer(wxOK | wxCANCEL);
    sizer->Add(btnSizer, 0, wxEXPAND | wxBOTTOM | wxLEFT | wxRIGHT, 12);

    propDlg.SetSizer(sizer);
    propDlg.SetMinSize(wxSize(400, -1));
    propDlg.Fit();

    if (propDlg.ShowModal() != wxID_OK)
        return;

    tile.materials.floor = floorMat->GetValue().ToStdString();
    tile.materials.ceiling = ceilMat->GetValue().ToStdString();
    tile.materials.wallNorth = wallNMat->GetValue().ToStdString();
    tile.materials.wallSouth = wallSMat->GetValue().ToStdString();
    tile.materials.wallEast = wallEMat->GetValue().ToStdString();
    tile.materials.wallWest = wallWMat->GetValue().ToStdString();

    if (tile.hasStairs)
    {
        tile.stairsParams.material = stairMatCtrl->GetValue().ToStdString();
        tile.stairsParams.stepCount = stepCountCtrl->GetValue();
        tile.stairsParams.stepHeight = string::convert<float>(
            stepHeightCtrl->GetValue().ToStdString(), 16.0f);
        tile.stairsParams.direction = dirChoice->GetSelection();
        tile.stairsParams.solid = solidCheck->GetValue();
    }

    if (tile.hasLight)
    {
        tile.lightParams.radius = string::convert<float>(
            lightRadiusCtrl->GetValue().ToStdString(), 128.0f);
        tile.lightParams.color = Vector3(
            string::convert<float>(lightColorRCtrl->GetValue().ToStdString(), 1.0f),
            string::convert<float>(lightColorGCtrl->GetValue().ToStdString(), 1.0f),
            string::convert<float>(lightColorBCtrl->GetValue().ToStdString(), 1.0f));
        tile.lightParams.texture = lightTextureCtrl->GetValue().ToStdString();
    }

    Refresh();
}

} // namespace ui
