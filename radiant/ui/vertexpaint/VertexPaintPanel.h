#pragma once

#include <sigc++/trackable.h>
#include "wxutil/DockablePanel.h"
#include "math/Vector3.h"

#include <memory>
#include <wx/spinctrl.h>

class wxChoice;
class wxButton;

namespace ui
{

class VertexPaintPreview;

class VertexPaintPanel :
    public wxutil::DockablePanel,
    public sigc::trackable
{
private:
    wxChoice* _channelChoice;
    wxChoice* _previewModeChoice;
    wxChoice* _falloffTypeChoice;
    wxSpinCtrlDouble* _radius;
    wxSpinCtrlDouble* _strength;
    wxSpinCtrlDouble* _falloff;
    wxButton* _fillRButton;
    wxButton* _fillGButton;
    wxButton* _fillBButton;
    wxButton* _clearButton;
    wxButton* _saveButton;

    std::unique_ptr<VertexPaintPreview> _preview;
    bool _previewAttached = false;

    sigc::connection _settingsChangedConnection;

public:
    VertexPaintPanel(wxWindow* parent);
    ~VertexPaintPanel() override;

protected:
    void onPanelActivated() override;
    void onPanelDeactivated() override;

private:
    void populateWindow();
    void pullFromSettings();
    void pushToSettings();
    void onPreviewModeChanged(wxCommandEvent& ev);
    void onChoiceChange(wxCommandEvent& ev);
    void onSpinChange(wxSpinDoubleEvent& ev);
    void onSaveClicked(wxCommandEvent& ev);
    void fillTargetWithColour(const Vector3& colour);
};

}
