#include "CornerPatchDialog.h"
#include "CornerPatchGeometry.h"

#include "i18n.h"
#include "ui/imainframe.h"
#include "imap.h"
#include "iscenegraph.h"
#include "iundo.h"

#include "string/convert.h"
#include "wxutil/dialog/MessageBox.h"

#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/spinctrl.h>
#include <wx/checkbox.h>

namespace
{
const char* const WINDOW_TITLE = N_("Corner Patch");
} // anonymous namespace

namespace ui
{

CornerPatchDialog::CornerPatchDialog(const cornerpatch::CornerDetection& detection,
                                     int defaultRadius, const scene::INodePtr& parent)
    : Dialog(_(WINDOW_TITLE), GlobalMainFrame().getWxTopLevelWindow()),
      _detection(detection), _parent(parent)
{
    _dialog->GetSizer()->Add(
        loadNamedPanel(_dialog, "CornerPatchMainPanel"), 1, wxEXPAND | wxALL, 12);

    wxStaticText* topLabel = findNamedObject<wxStaticText>(_dialog, "CornerPatchTopLabel");
    topLabel->SetFont(topLabel->GetFont().Bold());

    findNamedObject<wxTextCtrl>(_dialog, "CornerPatchRadius")
        ->SetValue(std::to_string(defaultRadius));

    bindParameterEvents(_dialog, this, &CornerPatchDialog::onParameterChanged);

    regenerate();
}

int CornerPatchDialog::getSegments()
{
    return findNamedObject<wxSpinCtrl>(_dialog, "CornerPatchSegments")->GetValue();
}

float CornerPatchDialog::getRadius()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "CornerPatchRadius")->GetValue().ToStdString(), 0.0f);
}

float CornerPatchDialog::getArcDegrees()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "CornerPatchArcDegrees")->GetValue().ToStdString(), 90.0f);
}

bool CornerPatchDialog::getInvert()
{
    return findNamedObject<wxCheckBox>(_dialog, "CornerPatchInvert")->GetValue();
}

GeneratorPreview& CornerPatchDialog::getPreview()
{
    return _preview;
}

void CornerPatchDialog::onParameterChanged(wxCommandEvent& ev)
{
    regenerate();
}

void CornerPatchDialog::generateInto()
{
    int segments = getSegments();
    float radius = getRadius();
    float arcDegrees = getArcDegrees();
    bool invert = getInvert();

    if (radius <= 0 || arcDegrees <= 0)
    {
        return;
    }

    cornerpatch::generateCornerPatch(_detection, segments, radius, arcDegrees,
                                     invert, _parent);
}

void CornerPatchDialog::regenerate()
{
    _preview.update(_parent, [this]() { generateInto(); });
}

void CornerPatchDialog::commitToMap()
{
    _preview.commit(_parent, "cornerPatchCreate", [this]() { generateInto(); });
}

void CornerPatchDialog::Show(const cmd::ArgumentList& args)
{
    auto detection = cornerpatch::detectCornerFromSelection();
    if (!detection.valid)
    {
        wxutil::Messagebox::ShowError(
            _("Select exactly 2 brushes that form an L-shaped corner."),
            GlobalMainFrame().getWxTopLevelWindow());
        return;
    }

    int defaultRadius = static_cast<int>(std::min(detection.radius1, detection.radius2));

    scene::INodePtr worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    CornerPatchDialog dialog(detection, defaultRadius, worldspawn);

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
