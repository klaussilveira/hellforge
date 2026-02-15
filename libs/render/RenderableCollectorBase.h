#pragma once

#include "irenderable.h"
#include "imapmerge.h"
#include "inode.h"
#include "iselection.h"

namespace render
{

/**
 * Front end renderer base implementation shared by CamRenderer and XYRenderer
 * 
 * Provides support for highlighting selected objects by activating the
 * RenderableCollector's "highlight" flags based on the renderable object's
 * selection state.
 */
class RenderableCollectorBase: public IRenderableCollector
{
protected:
    std::size_t _flags;

public:
    RenderableCollectorBase() :
        _flags(Highlight::Flags::NoHighlight)
    {}

    ~RenderableCollectorBase() override
    {}

    bool hasHighlightFlags() const override
    {
        return _flags != 0;
    }

    bool hasHighlightFlag(Highlight::Flags flags) const override
    {
        return (_flags & flags) == flags;
    }

    void setHighlightFlag(Highlight::Flags flags, bool enabled) override
    {
        if (enabled)
        {
            _flags |= flags;
        }
        else
        {
            _flags &= ~flags;
        }
    }

    virtual void processNode(const scene::INodePtr& node, const VolumeTest& volume)
    {
        node->onPreRender(volume);

        // greebo: Highlighting propagates to child nodes
        auto parent = node->getParent();

        auto highlightFlags = node->getHighlightFlags();

        auto nodeType = node->getNodeType();

        // Particle nodes shouldn't inherit the highlight flags from their parent, 
        // as it obstructs the view when their wireframe gets rendered (#5682)
        if (parent && nodeType != scene::INode::Type::Particle)
        {
            highlightFlags |= parent->getHighlightFlags();
        }

        if (nodeType == scene::INode::Type::MergeAction)
        {
            setHighlightFlag(Highlight::MergeAction, true);

            auto mergeActionNode = std::dynamic_pointer_cast<scene::IMergeActionNode>(node);
            assert(mergeActionNode);

            using scene::merge::ActionType;
            switch (mergeActionNode->getActionType())
            {
            case ActionType::AddChildNode:
            case ActionType::AddEntity:
                setHighlightFlag(Highlight::MergeActionAdd, true);
                setHighlightFlag(Highlight::MergeActionChange, false);
                setHighlightFlag(Highlight::MergeActionRemove, false);
                setHighlightFlag(Highlight::MergeActionConflict, false);
                break;

            case ActionType::AddKeyValue:
            case ActionType::ChangeKeyValue:
            case ActionType::RemoveKeyValue:
                setHighlightFlag(Highlight::MergeActionChange, true);
                setHighlightFlag(Highlight::MergeActionAdd, false);
                setHighlightFlag(Highlight::MergeActionRemove, false);
                setHighlightFlag(Highlight::MergeActionConflict, false);
                break;

            case ActionType::RemoveChildNode:
            case ActionType::RemoveEntity:
                setHighlightFlag(Highlight::MergeActionRemove, true);
                setHighlightFlag(Highlight::MergeActionAdd, false);
                setHighlightFlag(Highlight::MergeActionChange, false);
                setHighlightFlag(Highlight::MergeActionConflict, false);
                break;

            case ActionType::ConflictResolution:
                setHighlightFlag(Highlight::MergeActionConflict, true);
                setHighlightFlag(Highlight::MergeActionAdd, false);
                setHighlightFlag(Highlight::MergeActionChange, false);
                setHighlightFlag(Highlight::MergeActionRemove, false);
                break;

            case ActionType::NoAction:
                break;
            }
        }
        else
        {
            setHighlightFlag(Highlight::MergeAction, false);
            setHighlightFlag(Highlight::MergeActionAdd, false);
            setHighlightFlag(Highlight::MergeActionChange, false);
            setHighlightFlag(Highlight::MergeActionRemove, false);
            setHighlightFlag(Highlight::MergeActionConflict, false);
        }

        if (highlightFlags & Renderable::Highlight::Selected)
        {
            if (GlobalSelectionSystem().getSelectionMode() != selection::SelectionMode::Component)
            {
                setHighlightFlag(Highlight::Faces, true);
                setHighlightFlag(Highlight::Primitives, true);
            }
            else
            {
                // In component mode, don't highlight the whole primitive -
                // only selected components (faces/vertices/edges) should be highlighted
                setHighlightFlag(Highlight::Faces, false);
                setHighlightFlag(Highlight::Primitives, false);
            }

            // Pass on the info about whether we have a group member selected
            if (highlightFlags & Renderable::Highlight::GroupMember)
            {
                setHighlightFlag(Highlight::GroupMember, true);
            }
            else
            {
                setHighlightFlag(Highlight::GroupMember, false);
            }
        }
        else
        {
            setHighlightFlag(Highlight::Primitives, false);
            setHighlightFlag(Highlight::Faces, false);
            setHighlightFlag(Highlight::GroupMember, false);
        }

        // If this node should be highlighted, ask it to submit the corresponding geometry
        // Also call renderHighlights when Selected is set (for component mode highlighting)
        if (hasHighlightFlags() || (highlightFlags & Renderable::Highlight::Selected))
        {
            node->renderHighlights(*this, volume);
        }
    }
};

}
