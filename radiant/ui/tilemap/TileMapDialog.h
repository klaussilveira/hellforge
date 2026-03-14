#pragma once

#include "icommandsystem.h"
#include "wxutil/dialog/Dialog.h"
#include "wxutil/XmlResourceBasedWidget.h"
#include "TileMapGeometry.h"

#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/textctrl.h>
#include <wx/spinctrl.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/statbox.h>
#include <vector>

namespace ui
{

class TileGridCanvas;

class TileMapDialog : public wxutil::Dialog, private wxutil::XmlResourceBasedWidget
{
private:
    wxScrolledWindow* _scrollWin;
    TileGridCanvas* _canvas;
    int _currentFloor;

    std::vector<std::vector<std::vector<tilemap::Tile>>> _floors;

    int _activeBrush; // 0=Room, 1=Stairs, 2=Light

    wxSpinCtrl* _gridCols;
    wxSpinCtrl* _gridRows;
    wxChoice* _wallThickness;
    wxChoice* _roomStyle;

    wxStaticBoxSizer* _defMaterialsSizer;
    wxStaticBoxSizer* _defStairsSizer;
    wxStaticBoxSizer* _defLightSizer;

    wxTextCtrl* _defFloorMat;
    wxTextCtrl* _defCeilingMat;
    wxTextCtrl* _defWallMat;
    wxTextCtrl* _defStairMat;
    wxSpinCtrl* _defStepCount;
    wxTextCtrl* _defStepHeight;
    wxChoice* _defStairDir;
    wxCheckBox* _defStairSolid;
    wxTextCtrl* _defLightRadius;
    wxTextCtrl* _defLightColorR;
    wxTextCtrl* _defLightColorG;
    wxTextCtrl* _defLightColorB;
    wxTextCtrl* _defLightTexture;

public:
    TileMapDialog();

    float getTileWidth();
    float getTileHeight();
    float getFloorGap();
    float getCeilingHeight();
    int getGridCols();
    int getGridRows();
    int getRoomStyle();
    int getWallThickness();

    tilemap::TileMaterials getDefaultMaterials();
    tilemap::StairsParams getDefaultStairsParams();
    tilemap::LightParams getDefaultLightParams();

    std::vector<std::vector<tilemap::Tile>>& currentFloorGrid();
    const std::vector<std::vector<std::vector<tilemap::Tile>>>& floors() const { return _floors; }
    int floorCount() const { return static_cast<int>(_floors.size()); }
    int currentFloor() const { return _currentFloor; }
    int activeBrush() const { return _activeBrush; }

    static void Show(const cmd::ArgumentList& args);

private:
    void onFloorUp(wxCommandEvent& ev);
    void onFloorDown(wxCommandEvent& ev);
    void onBrushChanged(wxCommandEvent& ev);
    void onGridSizeChanged(wxSpinEvent& ev);
    void ensureFloorExists(int floor);
    void updateFloorLabel();
    void resizeGrids();
    void updateDefaultsPanelVisibility();
    void onRandom(wxCommandEvent& ev);
};

class TileGridCanvas : public wxPanel
{
private:
    TileMapDialog* _owner;
    int _hoveredX, _hoveredY;
    bool _painting;
    bool _erasing;

public:
    TileGridCanvas(wxWindow* parent, TileMapDialog* owner);

    void showTileProperties(int gx, int gy);

private:
    void onPaint(wxPaintEvent& ev);
    void onMouseDown(wxMouseEvent& ev);
    void onMouseUp(wxMouseEvent& ev);
    void onMouseMove(wxMouseEvent& ev);
    void onMiddleDown(wxMouseEvent& ev);
    void onMouseWheel(wxMouseEvent& ev);
    void onMouseLeave(wxMouseEvent& ev);

    int cellSize() const;
    void gridFromMouse(int mx, int my, int& gx, int& gy);
};

} // namespace ui
