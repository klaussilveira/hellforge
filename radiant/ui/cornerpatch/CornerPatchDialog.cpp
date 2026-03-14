#include "CornerPatchDialog.h"
#include "CornerPatchGeometry.h"

#include "i18n.h"
#include "ui/imainframe.h"
#include "imap.h"
#include "iselection.h"
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

CornerPatchDialog::CornerPatchDialog()
    : Dialog(_(WINDOW_TITLE), GlobalMainFrame().getWxTopLevelWindow())
{
    _dialog->GetSizer()->Add(
        loadNamedPanel(_dialog, "CornerPatchMainPanel"), 1, wxEXPAND | wxALL, 12);

    wxStaticText* topLabel = findNamedObject<wxStaticText>(_dialog, "CornerPatchTopLabel");
    topLabel->SetFont(topLabel->GetFont().Bold());
}

void CornerPatchDialog::setRadius(int value)
{
    findNamedObject<wxTextCtrl>(_dialog, "CornerPatchRadius")
        ->SetValue(std::to_string(value));
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

    CornerPatchDialog dialog;

    int defaultRadius = static_cast<int>(std::min(detection.radius1, detection.radius2));
    dialog.setRadius(defaultRadius);

    if (dialog.run() != IDialog::RESULT_OK)
        return;

    int segments = dialog.getSegments();
    float radius = dialog.getRadius();
    float arcDegrees = dialog.getArcDegrees();
    bool invert = dialog.getInvert();

    UndoableCommand undo("cornerPatchCreate");
    GlobalSelectionSystem().setSelectedAll(false);

    scene::INodePtr worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    cornerpatch::generateCornerPatch(detection, segments, radius, arcDegrees,
                                     invert, worldspawn);
}

} // namespace ui
