#pragma once

#include "ibrush.h"
#include "ipatch.h"
#include "scenelib.h"
#include "gamelib.h"
#include "math/Plane3.h"
#include "math/Matrix3.h"
#include "math/Vector2.h"
#include "math/Vector3.h"
#include "math/pi.h"
#include "polygon/Polygon2D.h"

#include <pugixml.hpp>

#include <cmath>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>

namespace osm
{

inline double getTextureScale()
{
    return game::current::getValue<double>("/generators/texScale", 1.0 / 128.0);
}

struct OsmNode
{
    long long id;
    double lat;
    double lon;
};

struct OsmBuilding
{
    long long id;
    std::vector<long long> nodeIds;
    double height;
    double minHeight;
    int levels;
    bool outline;
    bool part;
};

struct OsmRoad
{
    long long id;
    std::vector<long long> nodeIds;
    std::string highwayType;
    double width;
    int lanes;
};

struct OsmData
{
    std::unordered_map<long long, OsmNode> nodes;
    std::vector<OsmBuilding> buildings;
    std::vector<OsmRoad> roads;
    double centerLat = 0;
    double centerLon = 0;
};

struct OsmImportParams
{
    double unitsPerMeter = 40.0;
    double levelHeight = 3.0;
    std::string wallMaterial = "textures/common/caulk";
    std::string roofMaterial = "textures/common/caulk";
    std::string floorMaterial = "textures/common/caulk";
    std::string roadMaterial = "textures/common/caulk";
    std::string sidewalkMaterial = "textures/common/caulk";
    double baseZ = 0.0;
    double defaultLaneWidth = 3.5;
    double sidewalkWidth = 2.0;
    double curbHeight = 0.2;
};

inline Vector3 lonLatToLocal(double lon, double lat, double centerLon, double centerLat)
{
    const double R = 6371000.0;
    const double deg2rad = math::PI / 180.0;
    double x = (lon - centerLon) * deg2rad * R * std::cos(centerLat * deg2rad);
    double y = (lat - centerLat) * deg2rad * R;
    return Vector3(x, y, 0);
}

inline bool parseOsmFile(const std::string& filepath, OsmData& data, double levelHeight)
{
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filepath.c_str());
    if (!result)
        return false;

    auto root = doc.child("osm");
    if (!root)
        return false;

    auto bounds = root.child("bounds");
    if (bounds)
    {
        double minlat = bounds.attribute("minlat").as_double();
        double maxlat = bounds.attribute("maxlat").as_double();
        double minlon = bounds.attribute("minlon").as_double();
        double maxlon = bounds.attribute("maxlon").as_double();
        data.centerLat = (minlat + maxlat) * 0.5;
        data.centerLon = (minlon + maxlon) * 0.5;
    }

    for (auto n : root.children("node"))
    {
        OsmNode node;
        node.id = n.attribute("id").as_llong();
        node.lat = n.attribute("lat").as_double();
        node.lon = n.attribute("lon").as_double();
        data.nodes[node.id] = node;
    }

    for (auto w : root.children("way"))
    {
        long long id = w.attribute("id").as_llong();
        bool building = false;
        bool buildingPart = false;
        double height = 0;
        double minH = 0;
        int levels = 0;
        std::string highwayType;
        double roadWidth = 0;
        int roadLanes = 0;
        std::vector<long long> refIds;

        for (auto cn : w.children())
        {
            std::string name = cn.name();
            if (name == "tag")
            {
                std::string k = cn.attribute("k").as_string();
                if (k == "building")
                    building = true;
                else if (k == "building:part")
                    buildingPart = true;
                else if (k == "height")
                    height = cn.attribute("v").as_double();
                else if (k == "min_height")
                    minH = cn.attribute("v").as_double();
                else if (k == "building:levels")
                {
                    try { levels = std::stoi(cn.attribute("v").as_string()); }
                    catch (...) {}
                }
                else if (k == "highway")
                    highwayType = cn.attribute("v").as_string();
                else if (k == "width")
                    roadWidth = cn.attribute("v").as_double();
                else if (k == "lanes")
                {
                    try { roadLanes = std::stoi(cn.attribute("v").as_string()); }
                    catch (...) {}
                }
            }
            else if (name == "nd")
            {
                refIds.push_back(cn.attribute("ref").as_llong());
            }
        }

        if (building || buildingPart)
        {
            if (levels == 0) levels = 1;
            if (height == 0) height = levels * levelHeight;

            OsmBuilding bd;
            bd.id = id;
            bd.nodeIds = refIds;
            bd.height = height;
            bd.minHeight = minH;
            bd.levels = levels;
            bd.outline = false;
            bd.part = buildingPart;
            data.buildings.push_back(std::move(bd));
        }

        if (!highwayType.empty() && refIds.size() >= 2)
        {
            if (highwayType == "traffic_signals" || highwayType == "crossing" ||
                highwayType == "bus_stop" || highwayType == "stop" ||
                highwayType == "motorway_junction" || highwayType == "elevator" ||
                highwayType == "escalator")
                continue;

            OsmRoad rd;
            rd.id = id;
            rd.nodeIds = std::move(refIds);
            rd.highwayType = std::move(highwayType);
            rd.width = roadWidth;
            rd.lanes = roadLanes;
            data.roads.push_back(std::move(rd));
        }
    }

    for (auto r : root.children("relation"))
    {
        bool isBuildingRelation = false;
        long long outlineId = -1;

        for (auto cn : r.children())
        {
            std::string name = cn.name();
            if (name == "tag")
            {
                std::string k = cn.attribute("k").as_string();
                if (k == "type" && std::string(cn.attribute("v").as_string()) == "building")
                    isBuildingRelation = true;
            }
            else if (name == "member")
            {
                std::string type = cn.attribute("type").as_string();
                std::string role = cn.attribute("role").as_string();
                if (type == "way" && role == "outline")
                    outlineId = cn.attribute("ref").as_llong();
            }
        }

        if (isBuildingRelation && outlineId >= 0)
        {
            for (auto& bd : data.buildings)
            {
                if (bd.id == outlineId && !bd.part)
                {
                    bd.outline = true;
                    bd.height = levelHeight;
                    break;
                }
            }
        }
    }

    return true;
}

inline std::vector<polygon::Ring> getBuildingPolygons(const OsmBuilding& bd,
    const OsmData& data, double unitsPerMeter)
{
    if (bd.nodeIds.size() < 4)
        return {};

    polygon::Ring outline;
    for (size_t j = 0; j < bd.nodeIds.size() - 1; j++)
    {
        auto it = data.nodes.find(bd.nodeIds[j]);
        if (it == data.nodes.end())
            return {};
        Vector3 v = lonLatToLocal(it->second.lon, it->second.lat,
                                   data.centerLon, data.centerLat);
        v *= unitsPerMeter;
        outline.push_back(Vector2(v.x(), v.y()));
    }

    if (outline.size() < 3)
        return {};

    return polygon::convexPieces({ outline }, 0.3 * unitsPerMeter);
}

inline scene::INodePtr createBuildingBrush(
    const polygon::Ring& poly, double zBottom, double zTop,
    const std::string& wallMaterial, const std::string& roofMaterial,
    const std::string& floorMaterial, const scene::INodePtr& parent)
{
    if (poly.size() < 3)
        return {};

    auto brushNode = GlobalBrushCreator().createBrush();
    parent->addChildNode(brushNode);

    auto& brush = *Node_getIBrush(brushNode);

    double texScale = getTextureScale();
    Matrix3 proj = Matrix3::getIdentity();
    proj.xx() = texScale;
    proj.yy() = texScale;

    int n = static_cast<int>(poly.size());
    for (int i = 0; i < n; i++)
    {
        const Vector2& v1 = poly[i];
        const Vector2& v2 = poly[(i + 1) % n];

        double nx = v2.y() - v1.y();
        double ny = v1.x() - v2.x();
        double len = std::sqrt(nx * nx + ny * ny);
        if (len < 1e-6) continue;
        nx /= len;
        ny /= len;

        double dist = nx * v1.x() + ny * v1.y();
        brush.addFace(Plane3(nx, ny, 0, dist), proj, wallMaterial);
    }

    brush.addFace(Plane3(0, 0, 1, zTop), proj, roofMaterial);
    brush.addFace(Plane3(0, 0, -1, -zBottom), proj, floorMaterial);

    brush.evaluateBRep();
    return brushNode;
}

inline int generateOsmBuildings(const OsmData& data, const OsmImportParams& params,
                                 const scene::INodePtr& parent)
{
    int brushCount = 0;

    for (const auto& bd : data.buildings)
    {
        auto polys = getBuildingPolygons(bd, data, params.unitsPerMeter);

        double zBottom = params.baseZ + bd.minHeight * params.unitsPerMeter;
        double zTop = params.baseZ + bd.height * params.unitsPerMeter;

        if (zTop - zBottom < 1.0)
            continue;

        for (const auto& poly : polys)
        {
            auto node = createBuildingBrush(poly, zBottom, zTop,
                params.wallMaterial, params.roofMaterial, params.floorMaterial, parent);
            if (node)
            {
                Node_setSelected(node, true);
                brushCount++;
            }
        }
    }

    return brushCount;
}

inline double getRoadWidth(const OsmRoad& rd, double defaultLaneWidth)
{
    if (rd.width > 0)
        return rd.width;

    int lanes = rd.lanes;
    if (lanes <= 0)
    {
        if (rd.highwayType == "motorway" || rd.highwayType == "trunk")
            lanes = 4;
        else if (rd.highwayType == "primary" || rd.highwayType == "secondary")
            lanes = 2;
        else if (rd.highwayType == "footway" || rd.highwayType == "cycleway" ||
                 rd.highwayType == "steps" || rd.highwayType == "pedestrian")
            lanes = 1;
        else
            lanes = 2;
    }

    double laneW = defaultLaneWidth;
    if (rd.highwayType == "footway" || rd.highwayType == "cycleway" ||
        rd.highwayType == "steps" || rd.highwayType == "pedestrian")
        laneW = 2.0;

    return lanes * laneW;
}

inline int generateOsmRoads(const OsmData& data, const OsmImportParams& params,
                             const scene::INodePtr& parent)
{
    int patchCount = 0;

    for (const auto& rd : data.roads)
    {
        if (rd.nodeIds.size() < 2)
            continue;

        std::vector<Vector3> path;
        bool valid = true;
        for (auto nid : rd.nodeIds)
        {
            auto it = data.nodes.find(nid);
            if (it == data.nodes.end()) { valid = false; break; }
            Vector3 v = lonLatToLocal(it->second.lon, it->second.lat,
                                       data.centerLon, data.centerLat);
            v *= params.unitsPerMeter;
            v.z() = params.baseZ;
            path.push_back(v);
        }
        if (!valid || path.size() < 2)
            continue;

        double halfWidth = getRoadWidth(rd, params.defaultLaneWidth) * params.unitsPerMeter * 0.5;

        int rawHeight = static_cast<int>(path.size());
        int patchHeight = (rawHeight % 2 == 0) ? rawHeight + 1 : rawHeight;
        if (patchHeight < 3) patchHeight = 3;

        std::vector<Vector3> usedPath;
        if (patchHeight != rawHeight)
        {
            for (int i = 0; i < patchHeight; ++i)
            {
                double t = static_cast<double>(i) / (patchHeight - 1) * (rawHeight - 1);
                int seg = std::min(static_cast<int>(t), rawHeight - 2);
                double frac = t - seg;
                usedPath.push_back(path[seg] + (path[seg + 1] - path[seg]) * frac);
            }
        }
        else
        {
            usedPath = path;
        }

        double totalLen = 0;
        std::vector<double> cumLen(patchHeight, 0);
        for (int i = 1; i < patchHeight; ++i)
        {
            totalLen += (usedPath[i] - usedPath[i - 1]).getLength();
            cumLen[i] = totalLen;
        }

        std::vector<Vector3> perps(patchHeight);
        for (int row = 0; row < patchHeight; ++row)
        {
            Vector3 tangent;
            if (row == 0)
                tangent = (usedPath[1] - usedPath[0]);
            else if (row == patchHeight - 1)
                tangent = (usedPath[row] - usedPath[row - 1]);
            else
                tangent = (usedPath[row + 1] - usedPath[row - 1]);

            double tLen = std::sqrt(tangent.x() * tangent.x() + tangent.y() * tangent.y());
            if (tLen < 1e-6) tLen = 1.0;
            perps[row] = Vector3(-tangent.y() / tLen, tangent.x() / tLen, 0);
        }

        auto patchNode = GlobalPatchModule().createPatch(patch::PatchDefType::Def2);
        parent->addChildNode(patchNode);

        auto* patch = Node_getIPatch(patchNode);
        patch->setDims(3, static_cast<std::size_t>(patchHeight));
        patch->setShader(params.roadMaterial);

        for (int row = 0; row < patchHeight; ++row)
        {
            double v = (totalLen > 0) ? cumLen[row] / totalLen : 0;

            patch->ctrlAt(row, 0).vertex = usedPath[row] + perps[row] * halfWidth;
            patch->ctrlAt(row, 0).texcoord = Vector2(0, v);

            patch->ctrlAt(row, 1).vertex = usedPath[row];
            patch->ctrlAt(row, 1).texcoord = Vector2(0.5, v);

            patch->ctrlAt(row, 2).vertex = usedPath[row] - perps[row] * halfWidth;
            patch->ctrlAt(row, 2).texcoord = Vector2(1.0, v);
        }

        patch->controlPointsChanged();
        Node_setSelected(patchNode, true);
        patchCount++;

        double swWidth = params.sidewalkWidth * params.unitsPerMeter;
        double curbZ = params.baseZ + params.curbHeight * params.unitsPerMeter;

        for (int side = 0; side < 2; ++side)
        {
            double sign = (side == 0) ? -1.0 : 1.0;

            auto swNode = GlobalPatchModule().createPatch(patch::PatchDefType::Def2);
            parent->addChildNode(swNode);

            auto* sw = Node_getIPatch(swNode);
            sw->setDims(3, static_cast<std::size_t>(patchHeight));
            sw->setShader(params.sidewalkMaterial);

            for (int row = 0; row < patchHeight; ++row)
            {
                double v = (totalLen > 0) ? cumLen[row] / totalLen : 0;

                Vector3 roadEdge = usedPath[row] + perps[row] * (sign * halfWidth);
                Vector3 swOuter = usedPath[row] + perps[row] * (sign * (halfWidth + swWidth));
                Vector3 swMid = (roadEdge + swOuter) * 0.5;

                roadEdge.z() = curbZ;
                swMid.z() = curbZ;
                swOuter.z() = curbZ;

                Vector3 c0 = (side == 0) ? roadEdge : swOuter;
                Vector3 c2 = (side == 0) ? swOuter : roadEdge;

                sw->ctrlAt(row, 0).vertex = c0;
                sw->ctrlAt(row, 0).texcoord = Vector2(0, v);

                sw->ctrlAt(row, 1).vertex = swMid;
                sw->ctrlAt(row, 1).texcoord = Vector2(0.5, v);

                sw->ctrlAt(row, 2).vertex = c2;
                sw->ctrlAt(row, 2).texcoord = Vector2(1.0, v);
            }

            sw->controlPointsChanged();
            Node_setSelected(swNode, true);
            patchCount++;
        }
    }

    return patchCount;
}

} // namespace osm
