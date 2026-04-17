#pragma once

#include "icommandsystem.h"
#include "wxutil/dialog/Dialog.h"
#include "wxutil/XmlResourceBasedWidget.h"
#include "CityPlannerModel.h"

#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/textctrl.h>
#include <wx/spinctrl.h>
#include <wx/choice.h>
#include <wx/checkbox.h>
#include <wx/statbox.h>
#include <wx/stattext.h>

#include <string>
#include <vector>

namespace ui
{

enum class Brush : int
{
    Floor = 0,
    Wall,
    HalfSquareN,
    HalfSquareS,
    HalfSquareE,
    HalfSquareW,
    Building,
    WindowlessBuilding
};

class CityGridCanvas;

class CityPlannerDialog : public wxutil::Dialog, private wxutil::XmlResourceBasedWidget
{
    friend class CityGridCanvas;

private:
    wxScrolledWindow* _scrollWin;
    CityGridCanvas* _canvas;

    std::vector<std::vector<cityplanner::CityCell>> _grid;
    std::vector<cityplanner::BuildingGroup> _groups;
    int _selectedGroup;

    Brush _activeBrush;

    wxSpinCtrl* _gridCols;
    wxSpinCtrl* _gridRows;

    wxStaticBoxSizer* _floorBrushSizer;
    wxStaticBoxSizer* _wallBrushSizer;
    wxStaticBoxSizer* _bldgBrushSizer;

    wxTextCtrl* _floorMat;
    wxTextCtrl* _wallMat;

    wxStaticText* _bldgHeaderLabel;
    wxSpinCtrl*   _bpFloorCount;
    wxTextCtrl*   _bpFloorHeight;
    wxTextCtrl*   _bpWallThickness;
    wxTextCtrl*   _bpTrimHeight;
    wxChoice*     _bpWindowMode;
    wxSpinCtrl*   _bpWindowsPerFloor;
    wxTextCtrl*   _bpWindowWidth;
    wxTextCtrl*   _bpWindowHeight;
    wxTextCtrl*   _bpSillHeight;
    wxCheckBox*   _bpCornerColumns;
    wxTextCtrl*   _bpCornerExtrude;
    wxCheckBox*   _bpNoFirstFloorWindows;
    wxCheckBox*   _bpFirstFloorDoor;
    wxChoice*     _bpRoofType;
    wxTextCtrl*   _bpRoofHeight;
    wxTextCtrl*   _bpRoofBorderHeight;
    wxTextCtrl*   _bpWallMaterial;
    wxTextCtrl*   _bpTrimMaterial;

    bool _suppressParamEvents;

public:
    CityPlannerDialog();

    int getGridCols();
    int getGridRows();
    float getTileWidth();
    float getTileHeight();
    float getFloorThickness();
    float getWallHeight();
    bool getHeightVariation();

    Brush activeBrush() const { return _activeBrush; }
    std::string getDefaultFloorMaterial();
    std::string getDefaultWallMaterial();

    std::vector<std::vector<cityplanner::CityCell>>& grid() { return _grid; }
    const std::vector<cityplanner::BuildingGroup>& groups() const { return _groups; }
    int selectedGroup() const { return _selectedGroup; }

    void rebuildGroups();
    void selectGroupAt(int gx, int gy);
    void commitPanelToCurrentGroup();

    building::BuildingParams readParamsFromPanel();
    void writeParamsToPanel(const building::BuildingParams& p);

    static void Show(const cmd::ArgumentList& args);

private:
    void onBrushChanged(wxCommandEvent& ev);
    void onGridSizeChanged(wxSpinEvent& ev);
    void onRandom(wxCommandEvent& ev);
    void onParamsChanged(wxCommandEvent& ev);
    void onParamsSpin(wxSpinEvent& ev);
    void onParamsText(wxCommandEvent& ev);

    void buildDefaultsPanel(wxPanel* parent);
    wxPanel* buildBuildingParamsPanel(wxWindow* parent);
    void resizeGrid();
    void updateBrushPanelVisibility();
    void updateSelectedGroupHeader();
    void updateWindowControlsState();
};

class CityGridCanvas : public wxPanel
{
private:
    CityPlannerDialog* _owner;
    int _hoveredX, _hoveredY;
    bool _painting;
    bool _erasing;

public:
    CityGridCanvas(wxWindow* parent, CityPlannerDialog* owner);

private:
    void onPaint(wxPaintEvent& ev);
    void onMouseDown(wxMouseEvent& ev);
    void onMouseUp(wxMouseEvent& ev);
    void onMouseMove(wxMouseEvent& ev);
    void onMiddleDown(wxMouseEvent& ev);
    void onMouseLeave(wxMouseEvent& ev);

    int cellSize() const;
    void gridFromMouse(int mx, int my, int& gx, int& gy);
    void applyBrush(int gx, int gy);
    void applyErase(int gx, int gy);
};

} // namespace ui
