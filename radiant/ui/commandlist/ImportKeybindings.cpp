#include "ImportKeybindings.h"

#include "i18n.h"
#include "imodule.h"
#include "imousetoolmanager.h"
#include "itextstream.h"
#include "ui/idialogmanager.h"
#include "ui/ieventmanager.h"
#include "ui/imainframe.h"

#include <wx/event.h>

#include <optional>
#include <string>
#include <vector>

#include "os/fs.h"
#include "settings/SettingsManager.h"
#include "wxutil/FileChooser.h"
#include "wxutil/KeyCode.h"
#include "wxutil/Modifier.h"
#include "wxutil/MouseButton.h"
#include "wxutil/dialog/MessageBox.h"
#include "xmlutil/Document.h"

namespace ui
{

namespace
{

#ifdef WIN32
	const char* const DARKRADIANT_FOLDER_NAME = "DarkRadiant";
#else
	const char* const DARKRADIANT_FOLDER_NAME = "darkradiant";
#endif

std::string findDarkRadiantInputFile()
{
	const auto& ctx = module::GlobalModuleRegistry().getApplicationContext();

	fs::path hellforgePath(ctx.getSettingsPath());

	if (hellforgePath.filename().empty())
	{
		hellforgePath = hellforgePath.parent_path();
	}

	fs::path drPath = hellforgePath.parent_path() / DARKRADIANT_FOLDER_NAME;

	return settings::SettingsManager::findInHighestVersionFolder(drPath.string(), "input.xml");
}

class BoundCommandCollector :
	public IEventVisitor
{
public:
	std::vector<std::string> commands;

	void visit(const std::string& eventName, const IAccelerator& accel) override
	{
		if (accel.getKey() != 0)
		{
			commands.push_back(eventName);
		}
	}
};

std::size_t applyShortcutsFromXmlFile(const std::string& filename)
{
	xml::Document doc(filename);

	if (!doc.isValid())
	{
		rError() << "ImportKeybindings: cannot parse " << filename << std::endl;
		return 0;
	}

	xml::NodeList shortcutList = doc.findXPath("//shortcuts/shortcut");

	if (shortcutList.empty())
	{
		rWarning() << "ImportKeybindings: no shortcut definitions in " << filename << std::endl;
		return 0;
	}

	BoundCommandCollector collector;
	GlobalEventManager().foreachEvent(collector);

	for (const std::string& cmd : collector.commands)
	{
		GlobalEventManager().disconnectAccelerator(cmd);
	}

	std::size_t applied = 0;

	for (const xml::Node& node : shortcutList)
	{
		std::string command = node.getAttributeValue("command");
		std::string keyName = node.getAttributeValue("key");
		std::string modifierStr = node.getAttributeValue("modifiers");

		if (command.empty() || keyName.empty()) continue;

		unsigned int keyCode = wxutil::keycode::getKeyCodeFromName(keyName);

		if (keyCode == 0) continue;

		unsigned int modifierFlags = wxutil::Modifier::GetStateFromModifierString(modifierStr);

		wxKeyEvent ev(wxEVT_KEY_DOWN);
		ev.m_keyCode = static_cast<long>(keyCode);
		ev.SetShiftDown((modifierFlags & wxutil::Modifier::SHIFT) != 0);
		ev.SetControlDown((modifierFlags & wxutil::Modifier::CONTROL) != 0);
		ev.SetAltDown((modifierFlags & wxutil::Modifier::ALT) != 0);

		GlobalEventManager().connectAccelerator(ev, command);

		++applied;
	}

	rMessage() << "ImportKeybindings: applied " << applied << " shortcuts from " << filename << std::endl;

	return applied;
}

std::optional<IMouseToolGroup::Type> mouseToolGroupTypeFromName(const std::string& name)
{
	if (name == "OrthoView")   return IMouseToolGroup::Type::OrthoView;
	if (name == "CameraView")  return IMouseToolGroup::Type::CameraView;
	if (name == "TextureTool") return IMouseToolGroup::Type::TextureTool;
	return std::nullopt;
}

std::size_t applyMouseMappingsFromXmlFile(const std::string& filename)
{
	xml::Document doc(filename);

	if (!doc.isValid())
	{
		return 0;
	}

	xml::NodeList mappingNodes = doc.findXPath("//mouseToolMappings[@name='user']/mouseToolMapping");

	if (mappingNodes.empty())
	{
		mappingNodes = doc.findXPath("//mouseToolMapping");
	}

	if (mappingNodes.empty())
	{
		return 0;
	}

	std::size_t totalApplied = 0;

	for (const xml::Node& mappingNode : mappingNodes)
	{
		auto type = mouseToolGroupTypeFromName(mappingNode.getAttributeValue("name"));

		if (!type) continue;

		IMouseToolGroup& group = GlobalMouseToolManager().getGroup(*type);
		xml::NodeList toolNodes = mappingNode.getNamedChildren("tool");

		group.clearToolMappings();

		group.foreachMouseTool([&](const MouseToolPtr& tool)
		{
			for (const xml::Node& toolNode : toolNodes)
			{
				if (toolNode.getAttributeValue("name") != tool->getName()) continue;

				unsigned int state = wxutil::MouseButton::LoadFromNode(toolNode)
					| wxutil::Modifier::LoadFromNode(toolNode);

				group.addToolMapping(state, tool);
				++totalApplied;
				break;
			}
		});
	}

	rMessage() << "ImportKeybindings: applied " << totalApplied
		<< " mouse tool mappings from " << filename << std::endl;

	return totalApplied;
}

}

void ImportKeybindingsFromDarkRadiant(const cmd::ArgumentList& args)
{
	std::string suggestion = findDarkRadiantInputFile();

	wxutil::FileChooser chooser(GlobalMainFrame().getWxTopLevelWindow(),
		_("Import Keybindings from DarkRadiant"), true);

	if (!suggestion.empty())
	{
		chooser.setCurrentPath(suggestion);
	}

	std::string selected = chooser.display();

	if (selected.empty())
	{
		return;
	}

	std::size_t shortcutCount = applyShortcutsFromXmlFile(selected);
	std::size_t mouseCount = applyMouseMappingsFromXmlFile(selected);

	if (shortcutCount == 0 && mouseCount == 0)
	{
		wxutil::Messagebox::Show(_("Import Failed"),
			_("The selected file could not be parsed or did not contain any keybinding definitions."),
			IDialog::MESSAGE_ERROR);
		return;
	}

	std::string message = _("Imported ");
	message += std::to_string(shortcutCount);
	message += _(" keyboard shortcut(s) and ");
	message += std::to_string(mouseCount);
	message += _(" mouse binding(s) from:\n");
	message += selected;

	wxutil::Messagebox::Show(_("Import Successful"), message, IDialog::MESSAGE_CONFIRM);
}

}
