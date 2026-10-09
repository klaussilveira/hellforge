#pragma once

#include "icommandsystem.h"
#include "hotspot/HotspotSheet.h"
#include "wxutil/dialog/DialogBase.h"

#include <wx/image.h>

#include <cstdint>
#include <string>
#include <vector>

class wxCheckBox;
class wxChoice;
class wxSlider;
class wxSpinCtrlDouble;
class wxStaticText;
class wxTextCtrl;

namespace ui
{

class HotspotCanvas;

class HotspotEditor :
    public wxutil::DialogBase
{
private:
    std::string _material;
    std::string _sourcePath;
    hotspot::Sheet _sheet;
    int _selected = -1;
    int _grid = 8;

    std::vector<uint8_t> _pixels;
    int _imageWidth = 0;
    int _imageHeight = 0;

    HotspotCanvas* _canvas;
    wxTextCtrl* _materialEntry;
    wxStaticText* _infoLabel;
    wxSpinCtrlDouble* _scale;
    wxSpinCtrlDouble* _inset;
    wxChoice* _gridChoice;
    wxSlider* _consistency;
    wxCheckBox* _rotate;
    wxCheckBox* _reflect;
    wxCheckBox* _alt;
    wxCheckBox* _mapOnly;

public:
    HotspotEditor(const std::string& material);

    static void Show(const cmd::ArgumentList& args);

private:
    void loadMaterial(const std::string& material);
    void loadImage();
    void updateControls();
    void updateInfo();
    void save();
    void detect();

    void onBrowseMaterial(wxCommandEvent& ev);
    void onFlagChanged(wxCommandEvent& ev);
};

}
