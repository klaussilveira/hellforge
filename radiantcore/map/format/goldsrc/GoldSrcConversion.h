#pragma once

#include "GoldSrcLoader.h"

#include "imap.h"
#include "inode.h"
#include "iselectiongroup.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace map
{

namespace goldsrc
{

double toRadians(double degrees);

std::string* findValue(Entity& entity, const std::string& key);

void setValue(Entity& entity, const std::string& key, const std::string& value);

void removeKey(Entity& entity, const std::string& key);

std::vector<double> parseNumbers(const std::string* value);

std::string formatVector(const Vector3& v);

void convertLight(Entity& entity, const AABB& worldBounds, double scale, const std::string& coneKey, double radius);

void convertAngles(Entity& entity, bool isBrushEntity);

void convertRendering(Entity& entity);

void convertTargets(std::vector<Entity>& entities);

bool usesOnly(const Solid& solid, const std::set<std::string>& textures);

std::size_t removeToolSolids(std::vector<Solid>& solids, const std::set<std::string>& visibilityTextures,
	const std::string& originTexture);

AABB getBounds(const std::vector<Solid>& solids);

Vector3 takeOrigin(Entity& entity, const std::string& originTexture);

std::pair<double, double> textureSize(const Face& face, const std::string& shader);

scene::INodePtr createEntity(const Entity& entity, const std::string& logTag);

scene::INodePtr createBrush(const Solid& solid, const Vector3& origin, const std::string& material);

class SceneAssigner
{
	scene::IMapRootNodePtr _root;
	std::map<int, int> _layerIds;
	std::map<int, selection::ISelectionGroupPtr> _groups;

public:
	SceneAssigner(const scene::IMapRootNodePtr& root, const std::vector<Visgroup>& visgroups);

	void assign(const scene::INodePtr& node, const std::vector<int>& visgroups, const std::vector<int>& groupChain);
};

}

}
