#pragma once

#include "math/AABB.h"
#include "math/Plane3.h"
#include "math/Vector3.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace map
{

namespace goldsrc
{

struct Face
{
	std::string texture;
	Plane3 plane;
	bool valveAxes = false;
	Vector3 uAxis;
	Vector3 vAxis;
	double shiftU = 0;
	double shiftV = 0;
	double rotation = 0;
	double scaleU = 1;
	double scaleV = 1;
};

struct Solid
{
	std::vector<Face> faces;
	AABB bounds;
	std::vector<int> visgroups;
	std::vector<int> groups;
};

struct Entity
{
	std::vector<std::pair<std::string, std::string>> keyValues;
	std::vector<Solid> solids;
	std::vector<int> visgroups;
	std::vector<int> groups;
};

struct Visgroup
{
	int id;
	std::string name;
	bool visible;
};

struct MapData
{
	Entity world;
	std::vector<Entity> entities;
	std::vector<Visgroup> visgroups;
	std::size_t skippedPaths = 0;
	std::size_t skippedCurves = 0;
};

struct ScanResult
{
	std::set<std::string> textures;
	std::set<std::string> entities;
	std::set<std::string> brushEntities;
	std::map<std::string, std::pair<double, double>> textureSizes;
};

MapData load(const std::string& contents);

std::map<std::string, std::pair<double, double>> readWadTextureSizes(const std::string& mapPath);

ScanResult scanMap(const std::string& path);

}

}
