#include "GuiPropertyEditor.h"

#include "i18n.h"

#include "ui/common/GuiChooser.h"
#include "wxutil/Bitmap.h"

namespace ui
{

GuiPropertyEditor::GuiPropertyEditor(wxWindow* parent, IEntitySelection& entities, const ITargetKey::Ptr& key)
: PropertyEditor(entities),
  _key(key)
{
	constructBrowseButtonPanel(parent, _("Choose GUI..."),
		wxutil::GetLocalBitmap("book.png"));
}

void GuiPropertyEditor::onBrowseButtonClick()
{
	std::string prev = getKeyValueFromSelection(_key->getFullKey());
	std::string picked = GuiChooser::ChooseGui(prev, wxGetTopLevelParent(getWidget()));

	if (!picked.empty() && picked != prev)
	{
		setKeyValueOnSelection(_key->getFullKey(), picked);
	}
}

} // namespace
