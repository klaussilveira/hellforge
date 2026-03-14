#pragma once

#include "ibrush.h"
#include "ipatch.h"
#include "scenelib.h"
#include "math/Plane3.h"
#include "math/Matrix3.h"
#include "math/Vector3.h"
#include "math/pi.h"

#include <pugixml.hpp>

#include <cmath>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>

namespace osm
{

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

struct Polygon2D
{
    std::vector<Vector3> verts;

    Vector3 at(int i) const
    {
        int n = static_cast<int>(verts.size());
        return verts[((i % n) + n) % n];
    }
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

inline double area2D(const Vector3& a, const Vector3& b, const Vector3& c)
{
    return (a.x() * (b.y() - c.y()) + b.x() * (c.y() - a.y()) + c.x() * (a.y() - b.y()));
}

inline bool isReflex(const Vector3& p, const Vector3& o, const Vector3& n)
{
    double a = area2D(p, o, n);
    if (std::abs(a) <= 1e-6)
        return false;
    return a < 0;
}

inline bool isCollinear2D(const Vector3& a, const Vector3& b, const Vector3& c)
{
    return std::abs(area2D(a, b, c)) <= 1e-6;
}

inline double sqDist2D(const Vector3& a, const Vector3& b)
{
    double dx = b.x() - a.x();
    double dy = b.y() - a.y();
    return dx * dx + dy * dy;
}

inline Vector3 lineIntersect2D(const Vector3& p1, const Vector3& p2,
                                const Vector3& q1, const Vector3& q2)
{
    double a1 = p2.y() - p1.y();
    double b1 = p1.x() - p2.x();
    double c1 = a1 * p1.x() + b1 * p1.y();
    double a2 = q2.y() - q1.y();
    double b2 = q1.x() - q2.x();
    double c2 = a2 * q1.x() + b2 * q1.y();
    double det = a1 * b2 - a2 * b1;

    if (std::abs(det) > 1e-4)
        return Vector3((b2 * c1 - b1 * c2) / det, (a1 * c2 - a2 * c1) / det, 0);
    return Vector3(0, 0, 0);
}

inline bool segmentIntersect(const Vector3& p1, const Vector3& p2,
                              const Vector3& q1, const Vector3& q2, Vector3& out)
{
    double a = q2.y() - q1.y();
    double b = p2.x() - p1.x();
    double c = q2.x() - q1.x();
    double d = p2.y() - p1.y();
    double denom = (a * b) - (c * d);

    if (std::abs(denom) < 1e-5)
        return false;

    double e = p1.y() - q1.y();
    double f = p1.x() - q1.x();
    double oneOverDenom = 1.0 / denom;

    double ua = (c * e - a * f) * oneOverDenom;
    if (ua < 0.0 || ua > 1.0)
        return false;

    double ub = (b * e - d * f) * oneOverDenom;
    if (ub < 0.0 || ub > 1.0)
        return false;

    if (ua == 0.0 && ub == 0.0)
        return false;

    out = Vector3(p1.x() + ua * b, p1.y() + ua * d, 0);
    return true;
}

inline bool canSee(int i, int j, const Polygon2D& p)
{
    Vector3 prev = p.at(i - 1);
    Vector3 on = p.at(i);
    Vector3 next = p.at(i + 1);
    Vector3 onj = p.at(j);

    if (isReflex(prev, on, next))
    {
        if (area2D(on, prev, onj) >= 0 && area2D(on, next, onj) <= 0)
            return false;
    }
    else
    {
        if (area2D(on, next, onj) <= 0 || area2D(on, prev, onj) >= 0)
            return false;
    }

    Vector3 prevj = p.at(j - 1);
    Vector3 nextj = p.at(j + 1);

    if (isReflex(prevj, onj, nextj))
    {
        if (area2D(onj, prevj, on) >= 0 && area2D(onj, nextj, on) <= 0)
            return false;
    }
    else
    {
        if (area2D(onj, nextj, on) <= 0 || area2D(onj, prevj, on) >= 0)
            return false;
    }

    int n = static_cast<int>(p.verts.size());
    for (int k = 0; k < n; k++)
    {
        Vector3 ep1 = p.at(i);
        Vector3 ep2 = p.at(j);
        Vector3 eq1 = p.at(k);
        Vector3 eq2 = p.at(k + 1);

        if (ep1 == eq1 || ep1 == eq2 || ep2 == eq1 || ep2 == eq2)
            continue;

        Vector3 intPt;
        if (segmentIntersect(ep1, ep2, eq1, eq2, intPt))
        {
            if (intPt != eq1 || intPt != eq2)
                return false;
        }
    }

    return true;
}

inline double signedArea(const Polygon2D& p)
{
    double r = 0;
    int n = static_cast<int>(p.verts.size());
    for (int i = 0; i < n; i++)
    {
        int j = (i + 1) % n;
        r += p.verts[i].x() * p.verts[j].y();
        r -= p.verts[i].y() * p.verts[j].x();
    }
    return r / 2.0;
}

inline void forceCCW(Polygon2D& p)
{
    if (signedArea(p) < 0)
        std::reverse(p.verts.begin(), p.verts.end());
}

inline Polygon2D collinearSimplify(const Polygon2D& p, double tolerance)
{
    if (p.verts.size() <= 3)
        return p;

    Polygon2D result;
    int n = static_cast<int>(p.verts.size());
    for (int i = 0; i < n; i++)
    {
        Vector3 prev = p.at(i - 1);
        Vector3 current = p.at(i);
        Vector3 next = p.at(i + 1);
        if (std::abs(area2D(prev, current, next)) > tolerance)
            result.verts.push_back(current);
    }
    return result;
}

inline Polygon2D copyRange(const Polygon2D& p, int i, int j, int extra)
{
    int n = static_cast<int>(p.verts.size());
    while (j < i) j += n;
    Polygon2D result;
    for (int k = i; k <= j; ++k)
        result.verts.push_back(p.at(k));
    result.verts.resize(result.verts.size() + extra);
    return result;
}

inline bool isEar(int i, const Polygon2D& p)
{
    int n = static_cast<int>(p.verts.size());
    Vector3 prev = p.at(i - 1);
    Vector3 on = p.at(i);
    Vector3 next = p.at(i + 1);

    if (isReflex(prev, on, next))
        return false;

    for (int j = 0; j < n; j++)
    {
        if (j == ((i - 1 + n) % n) || j == i || j == ((i + 1) % n))
            continue;

        Vector3 pt = p.verts[j];
        double d1 = area2D(prev, on, pt);
        double d2 = area2D(on, next, pt);
        double d3 = area2D(next, prev, pt);
        if (d1 >= 0 && d2 >= 0 && d3 >= 0)
            return false;
    }
    return true;
}

inline std::vector<Polygon2D> earClipTriangulate(const Polygon2D& input)
{
    std::vector<Polygon2D> result;
    Polygon2D p = input;
    forceCCW(p);

    int safety = static_cast<int>(p.verts.size()) * 2;
    while (p.verts.size() > 3 && safety > 0)
    {
        bool found = false;
        int n = static_cast<int>(p.verts.size());
        for (int i = 0; i < n; i++)
        {
            if (isEar(i, p))
            {
                Polygon2D tri;
                tri.verts.push_back(p.at(i - 1));
                tri.verts.push_back(p.at(i));
                tri.verts.push_back(p.at(i + 1));
                result.push_back(tri);
                p.verts.erase(p.verts.begin() + i);
                found = true;
                break;
            }
        }
        if (!found)
            break;
        safety--;
    }

    if (p.verts.size() >= 3)
        result.push_back(p);

    return result;
}

inline std::vector<Polygon2D> convexPartition(const Polygon2D& input, int depth = 0)
{
    std::vector<Polygon2D> list;
    Polygon2D p = input;

    if (p.verts.size() < 3)
        return list;

    if (depth > 100)
        return earClipTriangulate(p);

    forceCCW(p);

    int n = static_cast<int>(p.verts.size());

    for (int i = 0; i < n; i++)
    {
        Vector3 prev = p.at(i - 1);
        Vector3 on = p.at(i);
        Vector3 next = p.at(i + 1);

        if (!isReflex(prev, on, next))
            continue;

        double lowerDist = 1e18;
        double upperDist = 1e18;
        Vector3 lowerInt, upperInt;
        int lowerIndex = 0, upperIndex = 0;
        bool lowerFound = false, upperFound = false;

        for (int j = 0; j < n; j++)
        {
            if (j == i || j == ((i + 1) % n) || j == (i - 1 + n) % n)
                continue;

            Vector3 jSelf = p.at(j);
            Vector3 jPrev = p.at(j - 1);
            Vector3 jNext = p.at(j + 1);

            bool leftOK = area2D(prev, on, jSelf) > 0;
            bool rightOK = area2D(prev, on, jPrev) < 0;
            bool leftOnOK = isCollinear2D(prev, on, jSelf);
            bool rightOnOK = isCollinear2D(prev, on, jPrev);

            if (leftOnOK || rightOnOK)
            {
                double d = sqDist2D(on, jSelf);
                if (d < lowerDist) { lowerDist = d; lowerInt = jSelf; lowerIndex = j; lowerFound = true; }
                d = sqDist2D(on, jPrev);
                if (d < lowerDist) { lowerDist = d; lowerInt = jPrev; lowerIndex = j - 1; lowerFound = true; }
            }
            else if (leftOK && rightOK)
            {
                Vector3 intersect = lineIntersect2D(p.at(i - 1), p.at(i), p.at(j), p.at(j - 1));
                if (area2D(p.at(i + 1), p.at(i), intersect) < 0)
                {
                    double d = sqDist2D(p.at(i), intersect);
                    if (d < lowerDist) { lowerDist = d; lowerInt = intersect; lowerIndex = j; lowerFound = true; }
                }
            }

            bool leftOKn = area2D(next, on, jNext) > 0;
            bool rightOKn = area2D(next, on, jSelf) < 0;
            bool leftOnOKn = isCollinear2D(next, on, jNext);
            bool rightOnOKn = isCollinear2D(next, on, jSelf);

            if (leftOnOKn || rightOnOKn)
            {
                double d = sqDist2D(on, jNext);
                if (d < upperDist) { upperDist = d; upperInt = jNext; upperIndex = j + 1; upperFound = true; }
                d = sqDist2D(on, jSelf);
                if (d < upperDist) { upperDist = d; upperInt = jSelf; upperIndex = j; upperFound = true; }
            }
            else if (leftOKn && rightOKn)
            {
                Vector3 intersect = lineIntersect2D(p.at(i + 1), p.at(i), p.at(j), p.at(j + 1));
                if (area2D(p.at(i - 1), p.at(i), intersect) > 0)
                {
                    double d = sqDist2D(p.at(i), intersect);
                    if (d < upperDist) { upperDist = d; upperIndex = j; upperInt = intersect; upperFound = true; }
                }
            }
        }

        if (!lowerFound || !upperFound)
            return earClipTriangulate(p);

        Polygon2D lowerPoly, upperPoly;

        if (lowerIndex == (upperIndex + 1) % n)
        {
            Vector3 sp = (lowerInt + upperInt) * 0.5;
            lowerPoly = copyRange(p, i, upperIndex, 1);
            lowerPoly.verts.back() = sp;
            upperPoly = copyRange(p, lowerIndex, i, 1);
            upperPoly.verts.back() = sp;
        }
        else
        {
            double highest = 0;
            int bestIndex = lowerIndex;
            int uIdx = upperIndex;
            while (uIdx < lowerIndex) uIdx += n;

            for (int j = lowerIndex; j <= uIdx; ++j)
            {
                if (!canSee(i, j, p))
                    continue;

                double score = 1.0 / (sqDist2D(p.at(i), p.at(j)) + 1.0);
                Vector3 pj = p.at(j - 1);
                Vector3 oj = p.at(j);
                Vector3 nj = p.at(j + 1);

                if (isReflex(pj, oj, nj))
                {
                    if (area2D(pj, oj, on) <= 0 && area2D(nj, oj, on) >= 0)
                        score += 3;
                    else
                        score += 2;
                }
                else
                {
                    score += 1;
                }

                if (score > highest) { bestIndex = j; highest = score; }
            }

            lowerPoly = copyRange(p, i, bestIndex, 0);
            upperPoly = copyRange(p, bestIndex, i, 0);
        }

        if (lowerPoly.verts.size() < 3 || upperPoly.verts.size() < 3 ||
            (lowerPoly.verts.size() >= p.verts.size() && upperPoly.verts.size() >= p.verts.size()))
            return earClipTriangulate(p);

        if (lowerPoly.verts.size() < upperPoly.verts.size())
        {
            auto a = convexPartition(lowerPoly, depth + 1);
            auto b = convexPartition(upperPoly, depth + 1);
            list.insert(list.end(), a.begin(), a.end());
            list.insert(list.end(), b.begin(), b.end());
        }
        else
        {
            auto a = convexPartition(upperPoly, depth + 1);
            auto b = convexPartition(lowerPoly, depth + 1);
            list.insert(list.end(), a.begin(), a.end());
            list.insert(list.end(), b.begin(), b.end());
        }

        return list;
    }

    list.push_back(p);
    for (auto& poly : list)
        poly = collinearSimplify(poly, 0.3);
    return list;
}

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

inline std::vector<Polygon2D> getBuildingPolygons(const OsmBuilding& bd,
    const OsmData& data, double unitsPerMeter)
{
    if (bd.nodeIds.size() < 4)
        return {};

    Polygon2D poly;
    for (size_t j = 0; j < bd.nodeIds.size() - 1; j++)
    {
        auto it = data.nodes.find(bd.nodeIds[j]);
        if (it == data.nodes.end())
            return {};
        Vector3 v = lonLatToLocal(it->second.lon, it->second.lat,
                                   data.centerLon, data.centerLat);
        v *= unitsPerMeter;
        poly.verts.push_back(v);
    }

    forceCCW(poly);
    poly = collinearSimplify(poly, 0.3 * unitsPerMeter);

    if (poly.verts.size() < 3)
        return {};

    return convexPartition(poly);
}

inline scene::INodePtr createBuildingBrush(
    const Polygon2D& poly, double zBottom, double zTop,
    const std::string& wallMaterial, const std::string& roofMaterial,
    const std::string& floorMaterial, const scene::INodePtr& parent)
{
    if (poly.verts.size() < 3)
        return {};

    auto brushNode = GlobalBrushCreator().createBrush();
    parent->addChildNode(brushNode);

    auto& brush = *Node_getIBrush(brushNode);

    double texScale = 0.0078125;
    Matrix3 proj = Matrix3::getIdentity();
    proj.xx() = texScale;
    proj.yy() = texScale;

    int n = static_cast<int>(poly.verts.size());
    for (int i = 0; i < n; i++)
    {
        const Vector3& v1 = poly.verts[i];
        const Vector3& v2 = poly.verts[(i + 1) % n];

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
