#include "SelectionInterface.h"

#include "ibrush.h"
#include "ipatch.h"
#include "ientity.h"

namespace script
{

const SelectionInfo& SelectionInterface::getSelectionInfo() 
{
	return GlobalSelectionSystem().getSelectionInfo();
}

void SelectionInterface::foreachSelected(const selection::SelectionSystem::Visitor& visitor)
{
	GlobalSelectionSystem().foreachSelected(visitor);
}

void SelectionInterface::foreachSelectedComponent(const selection::SelectionSystem::Visitor& visitor)
{
	GlobalSelectionSystem().foreachSelectedComponent(visitor);
}

void SelectionInterface::foreachSelectedFace(SelectedFaceVisitor& visitor)
{
    GlobalSelectionSystem().foreachFace([&](IFace& face)
    {
        visitor.visitFace(face);
    });
}

void SelectionInterface::setSelectedAll(int selected)
{
	GlobalSelectionSystem().setSelectedAll(static_cast<bool>(selected));
}

void SelectionInterface::setSelectedAllComponents(int selected)
{
	GlobalSelectionSystem().setSelectedAllComponents(static_cast<bool>(selected));
}

ScriptSceneNode SelectionInterface::ultimateSelected()
{
	return GlobalSelectionSystem().ultimateSelected();
}

ScriptSceneNode SelectionInterface::penultimateSelected() 
{
	return GlobalSelectionSystem().penultimateSelected();
}

py::list SelectionInterface::getSelectedNodes()
{
	py::list result;
	GlobalSelectionSystem().foreachSelected([&](const scene::INodePtr& node)
	{
		result.append(ScriptSceneNode(node));
	});
	return result;
}

py::list SelectionInterface::getSelectedBrushNodes()
{
	py::list result;
	GlobalSelectionSystem().foreachSelected([&](const scene::INodePtr& node)
	{
		if (Node_isBrush(node))
		{
			result.append(ScriptBrushNode(node));
		}
	});
	return result;
}

py::list SelectionInterface::getSelectedEntityNodes()
{
	py::list result;
	GlobalSelectionSystem().foreachSelected([&](const scene::INodePtr& node)
	{
		if (Node_isEntity(node))
		{
			result.append(ScriptEntityNode(node));
		}
	});
	return result;
}

py::list SelectionInterface::getSelectedPatchNodes()
{
	py::list result;
	GlobalSelectionSystem().foreachSelected([&](const scene::INodePtr& node)
	{
		if (Node_isPatch(node))
		{
			result.append(ScriptPatchNode(node));
		}
	});
	return result;
}

// IScriptInterface implementation
void SelectionInterface::registerInterface(py::module& scope, py::dict& globals)
{
	// Expose the SelectionInfo structure
	py::class_<SelectionInfo> selInfo(scope, "SelectionInformation");
	selInfo.def(py::init<>());
	selInfo.def_readonly("totalCount", &SelectionInfo::totalCount);
	selInfo.def_readonly("patchCount", &SelectionInfo::patchCount);
	selInfo.def_readonly("brushCount", &SelectionInfo::brushCount);
	selInfo.def_readonly("entityCount", &SelectionInfo::entityCount);
	selInfo.def_readonly("componentCount", &SelectionInfo::componentCount);

	// Expose the SelectionSystem::Visitor interface
	py::class_<selection::SelectionSystem::Visitor, SelectionVisitorWrapper> visitor(scope, "SelectionVisitor");
	visitor.def(py::init<>());
	visitor.def("visit", &selection::SelectionSystem::Visitor::visit);

    // Expose the SelectionFaceVisitor interface
    py::class_<SelectedFaceVisitor, SelectedFaceVisitorWrapper> faceVisitor(scope, "SelectedFaceVisitor");
    faceVisitor.def(py::init<>());
    faceVisitor.def("visitFace", &SelectedFaceVisitor::visitFace);

	// Add the module declaration to the given python namespace
	py::class_<SelectionInterface> selSys(scope, "SelectionSystem");

	selSys.def("getSelectionInfo", &SelectionInterface::getSelectionInfo, py::return_value_policy::reference);
	selSys.def("foreachSelected", &SelectionInterface::foreachSelected);
	selSys.def("foreachSelectedComponent", &SelectionInterface::foreachSelectedComponent);
	selSys.def("foreachSelectedFace", &SelectionInterface::foreachSelectedFace);
	selSys.def("setSelectedAll", &SelectionInterface::setSelectedAll);
	selSys.def("setSelectedAllComponents", &SelectionInterface::setSelectedAllComponents);
	selSys.def("ultimateSelected", &SelectionInterface::ultimateSelected);
	selSys.def("penultimateSelected", &SelectionInterface::penultimateSelected);
	selSys.def("getSelectedNodes", &SelectionInterface::getSelectedNodes);
	selSys.def("getSelectedBrushNodes", &SelectionInterface::getSelectedBrushNodes);
	selSys.def("getSelectedEntityNodes", &SelectionInterface::getSelectedEntityNodes);
	selSys.def("getSelectedPatchNodes", &SelectionInterface::getSelectedPatchNodes);

	// Now point the Python variable "GlobalSelectionSystem" to this instance
	globals["GlobalSelectionSystem"] = this;
}

} // namespace script
