#include "ChildPrimitives.h"

#include "ibrush.h"
#include "ientity.h"
#include "igroupnode.h"
#include "ilightnode.h"
#include "ipatch.h"

#include "scene/EntityNode.h"
#include "registry/registry.h"
#include "string/convert.h"

namespace scene
{

namespace
{

class PatchTranslator : public scene::NodeVisitor
{
	Vector3 _translation;
public:
	PatchTranslator(const Vector3& translation) :
		_translation(translation)
	{}

	bool pre(const scene::INodePtr& node) override
	{
		IPatch* patch = Node_getIPatch(node);
		if (patch)
		{
			for (std::size_t row = 0; row < patch->getHeight(); ++row)
			{
				for (std::size_t col = 0; col < patch->getWidth(); ++col)
				{
					patch->ctrlAt(row, col).vertex += _translation;
				}
			}
			patch->controlPointsChanged();
		}

		return true;
	}
};

void translateChildPatches(const scene::INodePtr& node, const Vector3& translation)
{
	PatchTranslator translator(translation);
	node->traverseChildren(translator);
}

}

// Local helper to add origins
class OriginAdder :
	public scene::NodeVisitor
{
public:
	// NodeVisitor implementation
	bool pre(const scene::INodePtr& node) override
	{
		Entity* entity = node->tryGetEntity();

		// Check for an entity
		if (entity != nullptr)
		{
			// greebo: Check for a Doom3Group
			scene::GroupNodePtr groupNode = Node_getGroupNode(node);

			// Don't handle the worldspawn children, they're safe&sound
			if (groupNode && !entity->isWorldspawn())
			{
				groupNode->addOriginToChildren();
				// Don't traverse the children
				return false;
			}

			if (Node_getLightNode(node) && !entity->isWorldspawn())
			{
				Vector3 origin = string::convert<Vector3>(
					entity->getKeyValue("origin"));
				translateChildPatches(node, -origin);
				return false;
			}
		}

		return true;
	}
};

class OriginRemover :
	public scene::NodeVisitor
{
public:
	bool pre(const scene::INodePtr& node) override
	{
		Entity* entity = node->tryGetEntity();

		// Check for an entity
		if (entity != nullptr)
		{
			// greebo: Check for a Doom3Group
			scene::GroupNodePtr groupNode = Node_getGroupNode(node);

			// Don't handle the worldspawn children, they're safe&sound
			if (groupNode && !entity->isWorldspawn())
			{
				groupNode->removeOriginFromChildren();
				// Don't traverse the children
				return false;
			}

			if (Node_getLightNode(node) && !entity->isWorldspawn())
			{
				Vector3 origin = string::convert<Vector3>(
					entity->getKeyValue("origin"));
				translateChildPatches(node, origin);
				return false;
			}
		}

		return true;
	}
};

void addOriginToChildPrimitives(const scene::INodePtr& root)
{
	// Disable texture lock during this process
    registry::ScopedKeyChanger<bool> changer(RKEY_ENABLE_TEXTURE_LOCK, false);

	OriginAdder adder;
	root->traverse(adder);
}

void removeOriginFromChildPrimitives(const scene::INodePtr& root)
{
	// Disable texture lock during this process
    registry::ScopedKeyChanger<bool> changer(RKEY_ENABLE_TEXTURE_LOCK, false);

	OriginRemover remover;
	root->traverse(remover);
}

} // namespace
