#include "MissionInfoGuiView.h"

#include "math/Vector4.h"

namespace ui
{

namespace
{

void setWindowDefText(const gui::IGuiPtr& gui, const std::string& windowDefName, const std::string& value)
{
	gui::IGuiWindowDefPtr windowDef = gui->findWindowDef(windowDefName);

	if (windowDef)
	{
		windowDef->text.setValue(value);
	}
}

}

MissionInfoGuiView::MissionInfoGuiView(wxWindow* parent) :
	GuiView(parent)
{}

void MissionInfoGuiView::setGLViewPort()
{
	double width = _windowDims[0];
	double height = _windowDims[1];

	double aspectRatio = _bgDims[0] / _bgDims[1];

	if (width / height > aspectRatio)
	{
		width = height * aspectRatio;
	}
	else
	{
		height = width / aspectRatio;
	}

	wxSize viewportSize = GetGLViewportSize(wxSize(static_cast<int>(width), static_cast<int>(height)));
	glViewport(0, 0, viewportSize.GetWidth(), viewportSize.GetHeight());
}

void MissionInfoGuiView::setGui(const gui::IGuiPtr& gui)
{
	// Call the base class first
	GuiView::setGui(gui);

	Vector2 topLeft(0, 0);
	Vector2 bottomRight(640, 480);

	if (_gui != NULL)
	{
		gui::IGuiWindowDefPtr bgWindowDef = _gui->findWindowDef(getTargetWindowDefName());

		if (bgWindowDef)
		{
			const Vector4& rect = bgWindowDef->rect;
			topLeft = Vector2(rect[0], rect[1]);
			bottomRight = Vector2(rect[0] + rect[2], rect[1] + rect[3]);
		}
	}

	_bgDims = bottomRight - topLeft;

	_renderer.setVisibleArea(topLeft, bottomRight);

	// Only draw a certain windowDef
	setWindowDefFilter(getTargetWindowDefName());
}

// ---------- Darkmod.xt ----------------

DarkmodTxtGuiView::DarkmodTxtGuiView(wxWindow* parent) :
	MissionInfoGuiView(parent)
{}

void DarkmodTxtGuiView::setMissionInfoFile(const map::DarkmodTxtPtr& file)
{
	_file = file;
}

void DarkmodTxtGuiView::updateGuiState()
{
	const gui::IGuiPtr& gui = getGui();

	if (!_file || !gui) return;

	// This is a localised value, hardcode some for the moment being
	gui->setStateString("details_posx", "100");

	setWindowDefText(gui, "modTitle", _file->getTitle());
	setWindowDefText(gui, "modDescription", _file->getDescription());
	setWindowDefText(gui, "modAuthor", _file->getAuthor());

	// These are internationalised strings in the GUI code, let's hardcode some for the preview
	setWindowDefText(gui, "modLastPlayedTitle", "Last played:");
	setWindowDefText(gui, "modCompletedTitle", "Completed:");
	setWindowDefText(gui, "modLastPlayedValue", "2017-11-19");
	setWindowDefText(gui, "modCompletedValue", "2017-11-26");
	setWindowDefText(gui, "modSizeTitle", "Space used:");
	setWindowDefText(gui, "modSizeValue", "123 MB");
	setWindowDefText(gui, "modSizeEraseFromDiskAction", "[Erase from disk]");

	setWindowDefText(gui, "modLoadN", "Install Mission");
	setWindowDefText(gui, "modLoadH", "Install Mission");
	setWindowDefText(gui, "modLoad", "Install Mission");
	setWindowDefText(gui, "moreInfoH", "Notes");
	setWindowDefText(gui, "moreInfoN", "Notes");
	setWindowDefText(gui, "moreInfo", "Notes");

	redraw();
}

std::string DarkmodTxtGuiView::getTargetWindowDefName()
{
	return "ModToInstallParent";
}

// ---------- Readme.xt ----------------

ReadmeTxtGuiView::ReadmeTxtGuiView(wxWindow* parent) :
	MissionInfoGuiView(parent)
{}

void ReadmeTxtGuiView::setMissionInfoFile(const map::ReadmeTxtPtr& file)
{
	_file = file;
}

void ReadmeTxtGuiView::updateGuiState()
{
	const gui::IGuiPtr& gui = getGui();

	if (!_file || !gui) return;

	gui->setStateString("ModNotesText", _file->getContents());
	setWindowDefText(gui, "ModInstallationNotesButtonOK", "OK");

	redraw();
}

std::string ReadmeTxtGuiView::getTargetWindowDefName()
{
	return "ModInstallationNotesParchment";
}

} // namespace
