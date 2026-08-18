#pragma once

#include "inode.h"
#include "iscenegraph.h"
#include "iselection.h"
#include "iundo.h"
#include "scenelib.h"
#include "ui/imainframe.h"

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/spinctrl.h>
#include <wx/textctrl.h>
#include <wx/window.h>

#include <functional>
#include <set>
#include <string>
#include <vector>

namespace ui
{

template<typename DialogClass>
inline void bindParameterEvents(wxWindow* window, DialogClass* self,
                                void (DialogClass::*handler)(wxCommandEvent&))
{
    window->Bind(wxEVT_TEXT, handler, self);
    window->Bind(wxEVT_CHECKBOX, handler, self);
    window->Bind(wxEVT_CHOICE, handler, self);
    window->Bind(wxEVT_SPINCTRL, handler, self);
}

class GeneratorPreview
{
private:
    std::vector<scene::INodePtr> _nodes;

    std::vector<scene::INodePtr> capture(const scene::INodePtr& parent,
                                         const std::function<void()>& generate)
    {
        std::set<scene::INode*> existing;

        parent->foreachNode([&](const scene::INodePtr& node)
        {
            existing.insert(node.get());
            return true;
        });

        generate();

        std::vector<scene::INodePtr> created;

        parent->foreachNode([&](const scene::INodePtr& node)
        {
            if (existing.count(node.get()) == 0)
            {
                created.push_back(node);
            }

            return true;
        });

        return created;
    }

public:
    void update(const scene::INodePtr& parent, const std::function<void()>& generate)
    {
        clear();

        _nodes = capture(parent, generate);

        for (const scene::INodePtr& node : _nodes)
        {
            Node_setSelected(node, false);
        }

        SceneChangeNotify();
        GlobalMainFrame().updateAllWindows();
    }

    ~GeneratorPreview()
    {
        clear();
    }

    void clear()
    {
        if (_nodes.empty())
        {
            return;
        }

        for (const scene::INodePtr& node : _nodes)
        {
            scene::removeNodeFromParent(node);
        }

        _nodes.clear();

        SceneChangeNotify();
    }

    void commit(const scene::INodePtr& parent, const std::string& command,
                const std::function<void()>& generate)
    {
        clear();

        UndoableCommand undo(command);

        GlobalSelectionSystem().setSelectedAll(false);
        GlobalSelectionSystem().setSelectedAllComponents(false);

        for (const scene::INodePtr& node : capture(parent, generate))
        {
            Node_setSelected(node, true);
        }

        SceneChangeNotify();
    }
};

} // namespace ui
