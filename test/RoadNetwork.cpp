#include "gtest/gtest.h"

#include <cmath>
#include <string>
#include <vector>

#include "ui/road/RoadShape.h"

namespace test
{

namespace
{

std::vector<Vector3> control(std::initializer_list<Vector3> points)
{
    return std::vector<Vector3>(points);
}

std::vector<Vector3> straightLine()
{
    return control({ Vector3(-512, 0, 0), Vector3(0, 0, 0), Vector3(512, 0, 0) });
}

std::vector<Vector3> crossingLine()
{
    return control({ Vector3(0, -512, 0), Vector3(0, 0, 0), Vector3(0, 512, 0) });
}

std::vector<Vector3> bendLine()
{
    return control({ Vector3(-512, 0, 0), Vector3(-128, -32, 0), Vector3(0, -160, 0),
                     Vector3(64, -512, 0) });
}

std::vector<Vector3> shallowLineA()
{
    return control({ Vector3(-600, -100, 0), Vector3(0, -40, 0), Vector3(600, 20, 0) });
}

std::vector<Vector3> shallowLineB()
{
    return control({ Vector3(-600, 100, 0), Vector3(0, 20, 0), Vector3(600, -60, 0) });
}

road::RoadParams squareParams()
{
    road::RoadParams params;
    params.subdivisions = 8;
    params.lanes = 2;
    params.laneWidth = 128;
    params.sidewalk = true;
    params.sidewalkWidth = 96;
    params.sidewalkHeight = 16;
    params.curbStyle = road::CURB_SQUARE;
    params.curbRadius = 8;
    params.cornerStyle = road::CORNER_SQUARE;
    params.cornerRadius = 64;
    params.roadMaterial = "road";
    params.sidewalkMaterial = "walk";
    params.curbMaterial = "curb";
    params.hiddenMaterial = "caulk";

    return params;
}

std::vector<std::vector<Vector3>> centrelinesOf(const std::vector<std::vector<Vector3>>& curves,
                                                int subdivisions)
{
    std::vector<std::vector<Vector3>> result;

    for (const std::vector<Vector3>& curve : curves)
    {
        result.push_back(road::sampleCurve(curve, subdivisions));
    }

    return result;
}

double totalArea(const std::vector<road::Ring>& rings)
{
    double total = 0;

    for (const road::Ring& ring : rings)
    {
        total += road::ringArea(ring);
    }

    return total;
}

Vector3 faceNormal(const road::BrushFace& face)
{
    return face.plane.normal().getNormalised();
}

double faceDistance(const road::BrushFace& face)
{
    return face.plane.dist() / face.plane.normal().getLength();
}

std::vector<Vector3> faceWinding(const road::BrushSolid& solid, std::size_t which)
{
    Vector3 normal = faceNormal(solid.faces[which]);
    double distance = faceDistance(solid.faces[which]);

    Vector3 guide = std::fabs(normal.z()) < 0.9 ? Vector3(0, 0, 1) : Vector3(1, 0, 0);
    Vector3 uAxis = guide.cross(normal).getNormalised();
    Vector3 vAxis = normal.cross(uAxis);
    Vector3 origin = normal * distance;

    const double reach = 100000;

    std::vector<Vector3> winding = { origin - uAxis * reach - vAxis * reach,
                                     origin + uAxis * reach - vAxis * reach,
                                     origin + uAxis * reach + vAxis * reach,
                                     origin - uAxis * reach + vAxis * reach };

    for (std::size_t other = 0; other < solid.faces.size() && !winding.empty(); ++other)
    {
        if (other == which)
        {
            continue;
        }

        Vector3 clipNormal = faceNormal(solid.faces[other]);
        double clipDistance = faceDistance(solid.faces[other]);

        std::vector<Vector3> clipped;

        for (std::size_t index = 0; index < winding.size(); ++index)
        {
            const Vector3& from = winding[index];
            const Vector3& to = winding[(index + 1) % winding.size()];

            double fromSide = clipNormal.dot(from) - clipDistance;
            double toSide = clipNormal.dot(to) - clipDistance;

            if (fromSide <= 1e-7)
            {
                clipped.push_back(from);
            }

            if ((fromSide > 1e-7 && toSide < -1e-7) || (fromSide < -1e-7 && toSide > 1e-7))
            {
                clipped.push_back(from + (to - from) * (fromSide / (fromSide - toSide)));
            }
        }

        winding.swap(clipped);
    }

    return winding;
}

double windingArea(const std::vector<Vector3>& winding)
{
    if (winding.size() < 3)
    {
        return 0;
    }

    Vector3 total(0, 0, 0);

    for (std::size_t index = 1; index + 1 < winding.size(); ++index)
    {
        total += (winding[index] - winding[0]).cross(winding[index + 1] - winding[0]);
    }

    return total.getLength() * 0.5;
}

bool insideSolid(const road::BrushSolid& solid, const Vector3& point, double slack)
{
    for (const road::BrushFace& face : solid.faces)
    {
        if (faceNormal(face).dot(point) - faceDistance(face) > slack)
        {
            return false;
        }
    }

    return true;
}

bool insideAnySolid(const road::RoadPlan& plan, const Vector3& point, double slack)
{
    for (const road::BrushSolid& solid : plan.brushes)
    {
        if (insideSolid(solid, point, slack))
        {
            return true;
        }
    }

    return false;
}

const road::BrushFace* topFace(const road::BrushSolid& solid)
{
    for (const road::BrushFace& face : solid.faces)
    {
        if (faceNormal(face).z() > 0.5)
        {
            return &face;
        }
    }

    return nullptr;
}

Vector3 columnOnFace(const road::BrushFace& face, const Vector2& column)
{
    Vector3 normal = faceNormal(face);

    return Vector3(column.x(), column.y(),
                   (faceDistance(face) - normal.x() * column.x() - normal.y() * column.y()) /
                       normal.z());
}

Vector3 nearestOnFace(const road::BrushFace& face, const Vector3& point)
{
    Vector3 normal = faceNormal(face);

    return point - normal * (normal.dot(point) - faceDistance(face));
}

Vector2 uvAt(const road::FaceProjection& projection, const Vector3& point)
{
    Vector3 uEdge = projection.points[1] - projection.points[0];
    Vector3 vEdge = projection.points[2] - projection.points[0];
    Vector3 offset = point - projection.points[0];

    double gramUU = uEdge.dot(uEdge);
    double gramUV = uEdge.dot(vEdge);
    double gramVV = vEdge.dot(vEdge);
    double determinant = gramUU * gramVV - gramUV * gramUV;

    double along = (offset.dot(uEdge) * gramVV - offset.dot(vEdge) * gramUV) / determinant;
    double across = (offset.dot(vEdge) * gramUU - offset.dot(uEdge) * gramUV) / determinant;

    return projection.uvs[0] + (projection.uvs[1] - projection.uvs[0]) * along +
           (projection.uvs[2] - projection.uvs[0]) * across;
}

double topHeightAt(const road::BrushSolid& solid, const Vector2& point)
{
    const road::BrushFace* face = topFace(solid);
    Vector3 normal = faceNormal(*face);

    return (faceDistance(*face) - normal.x() * point.x() - normal.y() * point.y()) / normal.z();
}

bool coversColumn(const road::BrushSolid& solid, const Vector2& point)
{
    for (const road::BrushFace& face : solid.faces)
    {
        Vector3 normal = faceNormal(face);

        if (std::fabs(normal.z()) > 0.01)
        {
            continue;
        }

        if (normal.dot(Vector3(point.x(), point.y(), 0)) - faceDistance(face) > 0.02)
        {
            return false;
        }
    }

    return true;
}

double distanceToRings(const std::vector<road::Ring>& rings, const Vector2& point)
{
    double best = -1;

    for (const road::Ring& ring : rings)
    {
        for (std::size_t index = 0; index < ring.size(); ++index)
        {
            const Vector2& from = ring[index];
            const Vector2& to = ring[(index + 1) % ring.size()];

            Vector2 step = to - from;
            double lengthSquared = step.dot(step);
            double parameter = 0;

            if (lengthSquared > 1e-12)
            {
                parameter = (point - from).dot(step) / lengthSquared;
                parameter = std::max(0.0, std::min(1.0, parameter));
            }

            double distance = (point - (from + step * parameter)).getLength();

            if (best < 0 || distance < best)
            {
                best = distance;
            }
        }
    }

    return best;
}

bool finite(double value)
{
    return !std::isnan(value) && !std::isinf(value);
}

} // anonymous namespace

TEST(RoadFootprintTest, StraightCarriagewayIsTheLaneWidthTimesTheLength)
{
    road::Footprint footprint =
        road::buildFootprint(centrelinesOf({ straightLine() }, 8), 128, 96, 0, false);

    ASSERT_EQ(footprint.carriage.size(), 1u);
    EXPECT_NEAR(totalArea(footprint.carriage), 1024.0 * 256.0, 1.0);
}

TEST(RoadFootprintTest, SidewalkBandRingsTheCarriagewayAtTheRequestedWidth)
{
    road::Footprint footprint =
        road::buildFootprint(centrelinesOf({ straightLine() }, 8), 128, 96, 0, false);

    ASSERT_EQ(footprint.band.size(), 2u);

    double perimeter = 2.0 * (1024.0 + 256.0);
    double expected = perimeter * 96.0 + 3.14159265358979 * 96.0 * 96.0;

    EXPECT_NEAR(totalArea(footprint.band), expected, expected * 0.01);
}

TEST(RoadFootprintTest, CrossingRoadsMergeIntoOneCarriagewayPolygon)
{
    road::Footprint footprint = road::buildFootprint(
        centrelinesOf({ straightLine(), crossingLine() }, 8), 128, 96, 0, false);

    ASSERT_EQ(footprint.carriage.size(), 1u);
    EXPECT_NEAR(totalArea(footprint.carriage), 2.0 * 1024.0 * 256.0 - 256.0 * 256.0, 1.0);
}

TEST(RoadFootprintTest, ShallowCrossingMergesIntoOneCarriagewayPolygon)
{
    road::Footprint footprint = road::buildFootprint(
        centrelinesOf({ shallowLineA(), shallowLineB() }, 8), 128, 96, 0, false);

    ASSERT_EQ(footprint.carriage.size(), 1u);
}

TEST(RoadFootprintTest, RoundedCornersFillJunctionCornersAndLeaveStraightRoadsAlone)
{
    road::Footprint straightSquare =
        road::buildFootprint(centrelinesOf({ straightLine() }, 8), 128, 96, 0, false);
    road::Footprint straightRound =
        road::buildFootprint(centrelinesOf({ straightLine() }, 8), 128, 96, 64, true);

    EXPECT_NEAR(totalArea(straightSquare.carriage), totalArea(straightRound.carriage), 1.0);

    road::Footprint crossSquare = road::buildFootprint(
        centrelinesOf({ straightLine(), crossingLine() }, 8), 128, 96, 0, false);
    road::Footprint crossRound = road::buildFootprint(
        centrelinesOf({ straightLine(), crossingLine() }, 8), 128, 96, 64, true);

    EXPECT_GT(totalArea(crossRound.carriage), totalArea(crossSquare.carriage) + 1000.0);
}

TEST(RoadFootprintTest, SidewalkNeverReachesFurtherThanItsWidthFromTheCarriageway)
{
    road::Footprint footprint = road::buildFootprint(
        centrelinesOf({ bendLine(), crossingLine() }, 8), 128, 96, 64, true);

    ASSERT_FALSE(footprint.band.empty());

    for (const road::Ring& ring : footprint.band)
    {
        for (const Vector2& point : ring)
        {
            EXPECT_LE(distanceToRings(footprint.carriage, point), 96.0 + 1.0)
                << "band point " << point.x() << ", " << point.y();
        }
    }
}

TEST(RoadFootprintTest, TriangulationCoversTheRingAreaExactly)
{
    road::Footprint footprint = road::buildFootprint(
        centrelinesOf({ straightLine(), crossingLine() }, 8), 128, 96, 64, true);

    std::vector<road::Ring> triangles = road::triangulate(footprint.band);

    ASSERT_FALSE(triangles.empty());

    for (const road::Ring& triangle : triangles)
    {
        EXPECT_EQ(triangle.size(), 3u);
    }

    EXPECT_NEAR(totalArea(triangles), totalArea(footprint.band), 1.0);
}

TEST(RoadFootprintTest, MergingKeepsTheAreaAndProducesConvexPieces)
{
    road::Footprint footprint = road::buildFootprint(
        centrelinesOf({ straightLine(), crossingLine() }, 8), 128, 96, 64, true);

    std::vector<road::Ring> triangles = road::triangulate(footprint.band);
    std::vector<road::Ring> pieces =
        road::mergeConvex(triangles, [](const road::Ring&) { return true; });

    ASSERT_FALSE(pieces.empty());
    EXPECT_LT(pieces.size(), triangles.size());
    EXPECT_NEAR(totalArea(pieces), totalArea(triangles), 1.0);

    for (const road::Ring& piece : pieces)
    {
        EXPECT_TRUE(road::isConvex(piece));
    }
}

TEST(RoadFootprintTest, MergingStopsWhereTheCallerRefuses)
{
    road::Footprint footprint =
        road::buildFootprint(centrelinesOf({ straightLine() }, 8), 128, 96, 0, false);

    std::vector<road::Ring> triangles = road::triangulate(footprint.carriage);
    std::vector<road::Ring> pieces =
        road::mergeConvex(triangles, [](const road::Ring&) { return false; });

    EXPECT_EQ(pieces.size(), triangles.size());
}

TEST(RoadFootprintTest, AnchorProjectsOntoTheCentrelineAndReportsArcLength)
{
    std::vector<std::vector<Vector3>> centrelines = centrelinesOf({ straightLine() }, 8);

    road::Anchor anchor = road::anchorAt(centrelines, Vector2(100, 224));

    EXPECT_NEAR(anchor.position.x(), 100, 0.01);
    EXPECT_NEAR(anchor.position.y(), 0, 0.01);
    EXPECT_NEAR(anchor.direction.x(), 1, 0.01);
    EXPECT_NEAR(anchor.travelled, 612, 0.01);
}

TEST(RoadFootprintTest, AnchorPicksTheNearerCentreline)
{
    std::vector<std::vector<Vector3>> centrelines =
        centrelinesOf({ straightLine(), crossingLine() }, 8);

    road::Anchor anchor = road::anchorAt(centrelines, Vector2(20, 400));

    EXPECT_NEAR(anchor.position.x(), 0, 0.01);
    EXPECT_NEAR(anchor.position.y(), 400, 0.01);
}

TEST(RoadShapeTest, CaulkOnlyEverAppearsOnDownwardFacingFaces)
{
    std::vector<std::vector<std::vector<Vector3>>> cases = {
        { straightLine() },
        { bendLine() },
        { straightLine(), crossingLine() },
        { shallowLineA(), shallowLineB() },
        { bendLine(), shallowLineA() },
    };

    for (int style = 0; style < 2; ++style)
    {
        road::RoadParams params = squareParams();
        params.curbStyle = style == 0 ? road::CURB_SQUARE : road::CURB_ROUND;
        params.cornerStyle = road::CORNER_ROUND;

        for (std::size_t index = 0; index < cases.size(); ++index)
        {
            road::RoadPlan plan = road::buildPlan(cases[index], params);

            ASSERT_FALSE(plan.brushes.empty()) << "case " << index;

            for (const road::BrushSolid& solid : plan.brushes)
            {
                for (std::size_t face = 0; face < solid.faces.size(); ++face)
                {
                    if (solid.faces[face].material != params.hiddenMaterial)
                    {
                        continue;
                    }

                    EXPECT_LT(faceNormal(solid.faces[face]).z(), -0.99)
                        << "case " << index << " curb style " << style;
                }
            }
        }
    }
}

TEST(RoadShapeTest, CaulkFacesAreAlwaysCoveredFromAboveByTheirOwnBrush)
{
    road::RoadParams params = squareParams();
    params.curbStyle = road::CURB_ROUND;

    road::RoadPlan plan = road::buildPlan({ bendLine(), crossingLine() }, params);

    ASSERT_FALSE(plan.brushes.empty());

    for (const road::BrushSolid& solid : plan.brushes)
    {
        for (std::size_t face = 0; face < solid.faces.size(); ++face)
        {
            if (solid.faces[face].material != params.hiddenMaterial)
            {
                continue;
            }

            std::vector<Vector3> winding = faceWinding(solid, face);

            ASSERT_GE(winding.size(), 3u);

            for (const Vector3& point : winding)
            {
                EXPECT_TRUE(insideSolid(solid, point + Vector3(0, 0, 1), 0.05))
                    << "caulk face is not the underside of its own brush";
            }
        }
    }
}

TEST(RoadShapeTest, BrushesDoNotOverlapEachOther)
{
    road::RoadParams params = squareParams();
    params.cornerStyle = road::CORNER_ROUND;

    road::RoadPlan plan = road::buildPlan({ straightLine(), crossingLine() }, params);

    ASSERT_GT(plan.brushes.size(), 1u);

    for (std::size_t index = 0; index < plan.brushes.size(); ++index)
    {
        std::vector<Vector3> winding = faceWinding(plan.brushes[index], 0);

        ASSERT_GE(winding.size(), 3u);

        for (std::size_t corner = 1; corner + 1 < winding.size(); ++corner)
        {
            Vector3 probe = (winding[0] + winding[corner] + winding[corner + 1]) / 3.0;
            probe.z() -= 1.0;

            for (std::size_t other = 0; other < plan.brushes.size(); ++other)
            {
                if (other == index)
                {
                    continue;
                }

                EXPECT_FALSE(insideSolid(plan.brushes[other], probe, -0.05))
                    << "brush " << index << " overlaps brush " << other;
            }
        }
    }
}

TEST(RoadShapeTest, RoadSurfaceCoversTheDrawnCurveAtItsOwnHeight)
{
    road::RoadParams params = squareParams();
    params.cornerStyle = road::CORNER_ROUND;

    std::vector<std::vector<Vector3>> curves = { bendLine(), crossingLine() };

    road::RoadPlan plan = road::buildPlan(curves, params);

    for (const std::vector<Vector3>& curve : curves)
    {
        std::vector<Vector3> line = road::sampleCurve(curve, params.subdivisions);

        for (const Vector3& point : line)
        {
            EXPECT_TRUE(insideAnySolid(plan, point - Vector3(0, 0, 1), 0.02))
                << "no road under " << point.x() << ", " << point.y();

            bool found = false;

            for (const road::BrushSolid& solid : plan.brushes)
            {
                const road::BrushFace* face = topFace(solid);

                if (face == nullptr || face->material != params.roadMaterial)
                {
                    continue;
                }

                if (!coversColumn(solid, Vector2(point.x(), point.y())))
                {
                    continue;
                }

                EXPECT_NEAR(topHeightAt(solid, Vector2(point.x(), point.y())), point.z(), 0.05);
                found = true;
                break;
            }

            EXPECT_TRUE(found);
        }
    }
}

TEST(RoadShapeTest, SidewalkTopSitsOneCurbHeightAboveTheRoad)
{
    road::RoadParams params = squareParams();

    road::RoadPlan plan = road::buildPlan({ straightLine() }, params);

    ASSERT_FALSE(plan.brushes.empty());

    int carriageways = 0;
    int sidewalks = 0;

    for (const road::BrushSolid& solid : plan.brushes)
    {
        const road::BrushFace* face = topFace(solid);

        ASSERT_TRUE(face != nullptr);

        if (face->material == params.roadMaterial)
        {
            ++carriageways;
            EXPECT_NEAR(topHeightAt(solid, Vector2(0, 0)), 0, 0.01);
        }
        else if (face->material == params.sidewalkMaterial)
        {
            ++sidewalks;
            EXPECT_NEAR(topHeightAt(solid, Vector2(0, 176)), params.sidewalkHeight, 0.01);
        }
    }

    EXPECT_EQ(carriageways, 1);
    EXPECT_GE(sidewalks, 2);
    EXPECT_EQ(carriageways + sidewalks, static_cast<int>(plan.brushes.size()));
}

TEST(RoadShapeTest, RoundCurbAddsFacetsTangentToTheFilletAndSquareCurbAddsNone)
{
    road::RoadParams square = squareParams();

    road::RoadPlan flat = road::buildPlan({ straightLine() }, square);

    for (const road::BrushSolid& solid : flat.brushes)
    {
        for (const road::BrushFace& face : solid.faces)
        {
            double height = faceNormal(face).z();

            EXPECT_TRUE(std::fabs(height) < 0.01 || std::fabs(height) > 0.99)
                << "square curb produced a sloped face";
        }
    }

    road::RoadParams round = squareParams();
    round.curbStyle = road::CURB_ROUND;
    round.curbRadius = 8;

    road::RoadPlan filleted = road::buildPlan({ straightLine() }, round);

    int facets = 0;

    for (const road::BrushSolid& solid : filleted.brushes)
    {
        if (topFace(solid)->material != round.sidewalkMaterial)
        {
            continue;
        }

        for (const road::BrushFace& candidate : solid.faces)
        {
            Vector3 normal = faceNormal(candidate);

            if (normal.z() < 0.01 || normal.z() > 0.99)
            {
                continue;
            }

            ++facets;

            const road::BrushFace* curb = nullptr;
            double best = 0;

            for (const road::BrushFace& side : solid.faces)
            {
                Vector3 sideNormal = faceNormal(side);

                if (std::fabs(sideNormal.z()) > 0.01)
                {
                    continue;
                }

                double alignment = sideNormal.dot(Vector3(normal.x(), normal.y(), 0));

                if (curb == nullptr || alignment > best)
                {
                    curb = &side;
                    best = alignment;
                }
            }

            ASSERT_TRUE(curb != nullptr);

            Vector3 curbNormal = faceNormal(*curb);
            Vector3 axis = curbNormal * (faceDistance(*curb) - round.curbRadius) +
                           Vector3(0, 0, round.sidewalkHeight - round.curbRadius);

            EXPECT_NEAR(normal.dot(axis) - faceDistance(candidate), -round.curbRadius, 0.01)
                << "facet is not tangent to a fillet of the requested radius";
        }
    }

    EXPECT_GE(facets, 3);
    EXPECT_EQ(facets % 3, 0);
}

TEST(RoadShapeTest, GeneratedSurfacesCarryTheRequestedMaterials)
{
    road::RoadParams params = squareParams();

    road::RoadPlan plan = road::buildPlan({ straightLine() }, params);

    bool sawRoad = false;
    bool sawSidewalk = false;
    bool sawCurb = false;

    for (const road::BrushSolid& solid : plan.brushes)
    {
        for (const road::BrushFace& face : solid.faces)
        {
            Vector3 normal = faceNormal(face);

            if (normal.z() > 0.5 && face.material == params.roadMaterial)
            {
                sawRoad = true;
            }

            if (normal.z() > 0.5 && face.material == params.sidewalkMaterial)
            {
                sawSidewalk = true;
            }

            if (std::fabs(normal.z()) < 0.01 && face.material == params.curbMaterial &&
                std::fabs(std::fabs(faceDistance(face)) - 128) < 0.01)
            {
                sawCurb = true;
            }
        }
    }

    EXPECT_TRUE(sawRoad);
    EXPECT_TRUE(sawSidewalk);
    EXPECT_TRUE(sawCurb);
}

TEST(RoadShapeTest, DisablingSidewalksLeavesOnlyTheCarriageway)
{
    road::RoadParams params = squareParams();
    params.sidewalk = false;

    road::RoadPlan plan = road::buildPlan({ straightLine() }, params);

    ASSERT_EQ(plan.brushes.size(), 1u);

    for (const road::BrushSolid& solid : plan.brushes)
    {
        for (const road::BrushFace& face : solid.faces)
        {
            EXPECT_NE(face.material, params.sidewalkMaterial);
            EXPECT_NE(face.material, params.curbMaterial);
        }
    }
}

TEST(RoadShapeTest, LaneCountDrivesTheCarriagewayWidth)
{
    road::RoadParams params = squareParams();
    params.sidewalk = false;
    params.lanes = 4;

    road::RoadPlan plan = road::buildPlan({ straightLine() }, params);

    ASSERT_EQ(plan.brushes.size(), 1u);

    EXPECT_TRUE(insideSolid(plan.brushes[0], Vector3(0, 250, -1), 0.02));
    EXPECT_FALSE(insideSolid(plan.brushes[0], Vector3(0, 262, -1), 0.02));
}

TEST(RoadTextureTest, EveryHorizontalSurfaceInTheNetworkSharesOneProjection)
{
    road::RoadParams params = squareParams();
    params.cornerStyle = road::CORNER_ROUND;

    road::RoadPlan plan = road::buildPlan({ straightLine(), crossingLine() }, params);

    std::vector<Vector2> probes = { Vector2(0, 0), Vector2(137, -59), Vector2(-311, 208) };
    std::vector<Vector2> expected;
    int surfaces = 0;

    for (const road::BrushSolid& solid : plan.brushes)
    {
        const road::BrushFace* face = topFace(solid);

        ASSERT_TRUE(face != nullptr);

        road::FaceProjection projection;

        ASSERT_TRUE(road::faceProjection(face->plane, plan.frame, projection));

        for (std::size_t index = 0; index < probes.size(); ++index)
        {
            Vector2 uv = uvAt(projection, columnOnFace(*face, probes[index]));

            if (expected.size() <= index)
            {
                expected.push_back(uv);
                continue;
            }

            EXPECT_NEAR(uv.x(), expected[index].x(), 1e-6)
                << "surface " << surfaces << " probe " << index;
            EXPECT_NEAR(uv.y(), expected[index].y(), 1e-6)
                << "surface " << surfaces << " probe " << index;
        }

        ++surfaces;
    }

    EXPECT_GT(surfaces, 4);
}

TEST(RoadTextureTest, EveryCurbFaceLookingOneWaySharesOneProjection)
{
    road::RoadParams params = squareParams();

    road::RoadPlan plan = road::buildPlan({ straightLine() }, params);

    Vector3 probe(96, 0, -4);
    bool seeded = false;
    Vector2 expected;
    int faces = 0;

    for (const road::BrushSolid& solid : plan.brushes)
    {
        for (const road::BrushFace& face : solid.faces)
        {
            if ((faceNormal(face) - Vector3(0, -1, 0)).getLength() > 0.0001)
            {
                continue;
            }

            road::FaceProjection projection;

            ASSERT_TRUE(road::faceProjection(face.plane, plan.frame, projection));

            Vector2 uv = uvAt(projection, nearestOnFace(face, probe));

            ++faces;

            if (!seeded)
            {
                expected = uv;
                seeded = true;
                continue;
            }

            EXPECT_NEAR(uv.x(), expected.x(), 0.001) << "face " << faces;
            EXPECT_NEAR(uv.y(), expected.y(), 0.001) << "face " << faces;
        }
    }

    EXPECT_GE(faces, 2);
}

TEST(RoadTextureTest, HorizontalSurfacesCarrySquareTexelsAtTheRequestedScale)
{
    road::RoadParams params = squareParams();
    params.texScale = 1.0 / 128.0;

    road::RoadPlan plan = road::buildPlan({ straightLine() }, params);

    int roadTops = 0;
    int sidewalkTops = 0;

    for (const road::BrushSolid& solid : plan.brushes)
    {
        const road::BrushFace* face = topFace(solid);

        ASSERT_TRUE(face != nullptr);

        if (face->material == params.roadMaterial)
        {
            ++roadTops;
        }
        else if (face->material == params.sidewalkMaterial)
        {
            ++sidewalkTops;
        }
        else
        {
            continue;
        }

        road::FaceProjection projection;

        ASSERT_TRUE(road::faceProjection(face->plane, plan.frame, projection));

        Vector3 along = projection.points[1] - projection.points[0];
        Vector3 across = projection.points[2] - projection.points[0];

        EXPECT_NEAR(along.getLength(), 128.0, 1e-6) << "material " << face->material;
        EXPECT_NEAR(across.getLength(), 128.0, 1e-6) << "material " << face->material;
        EXPECT_NEAR(along.dot(across), 0.0, 1e-6) << "material " << face->material;
    }

    EXPECT_GE(roadTops, 1);
    EXPECT_GE(sidewalkTops, 2);
}

TEST(RoadTextureTest, CurbFacesKeepTheRequestedScaleInsteadOfStretchingToTheKerbHeight)
{
    road::RoadParams params = squareParams();
    params.texScale = 1.0 / 128.0;
    params.sidewalkHeight = 16;

    road::RoadPlan plan = road::buildPlan({ straightLine() }, params);

    int curbFaces = 0;

    for (const road::BrushSolid& solid : plan.brushes)
    {
        if (topFace(solid)->material != params.sidewalkMaterial)
        {
            continue;
        }

        for (const road::BrushFace& face : solid.faces)
        {
            Vector3 normal = faceNormal(face);

            if (std::fabs(normal.z()) > 0.01 ||
                std::fabs(std::fabs(faceDistance(face)) - 128) > 0.01)
            {
                continue;
            }

            ++curbFaces;

            road::FaceProjection projection;

            ASSERT_TRUE(road::faceProjection(face.plane, plan.frame, projection));

            Vector3 down = projection.points[2] - projection.points[0];

            EXPECT_NEAR(down.z(), -128.0, 1e-6);
            EXPECT_NEAR((projection.points[1] - projection.points[0]).getLength(), 128.0, 1e-6);
        }
    }

    EXPECT_GE(curbFaces, 2);
}

TEST(RoadTextureTest, TheFrameRunsAlongTheLongestCentreline)
{
    road::RoadParams params = squareParams();
    params.sidewalk = false;

    road::RoadPlan plan = road::buildPlan(
        { control({ Vector3(0, -512, 0), Vector3(0, 0, 0), Vector3(0, 512, 0) }),
          control({ Vector3(-450, -600, 0), Vector3(0, 0, 0), Vector3(450, 600, 0) }) },
        params);

    EXPECT_NEAR(plan.frame.uAxis.x(), 0.6, 1e-6);
    EXPECT_NEAR(plan.frame.uAxis.y(), 0.8, 1e-6);
    EXPECT_NEAR(plan.frame.uAxis.z(), 0.0, 1e-6);
}

TEST(RoadTextureTest, TheProjectionMeasuresDistanceFromTheFrameOrigin)
{
    road::RoadParams params = squareParams();
    params.sidewalk = false;
    params.texScale = 1.0 / 128.0;

    road::RoadPlan plan = road::buildPlan({ straightLine() }, params);

    ASSERT_EQ(plan.brushes.size(), 1u);

    EXPECT_NEAR(plan.frame.origin.x(), -512, 0.01);

    road::FaceProjection projection;

    ASSERT_TRUE(road::faceProjection(topFace(plan.brushes[0])->plane, plan.frame, projection));

    EXPECT_NEAR(uvAt(projection, Vector3(-512, 0, 0)).x(), 0.0, 1e-6);
    EXPECT_NEAR(uvAt(projection, Vector3(0, 0, 0)).x(), 4.0, 1e-6);
    EXPECT_NEAR(uvAt(projection, Vector3(512, 0, 0)).x(), 8.0, 1e-6);
    EXPECT_NEAR(uvAt(projection, Vector3(0, -64, 0)).y(), 0.5, 1e-6);
}

TEST(RoadTextureTest, EveryFaceOfEveryBrushReceivesAProjection)
{
    road::RoadParams params = squareParams();
    params.curbStyle = road::CURB_ROUND;
    params.cornerStyle = road::CORNER_ROUND;

    road::RoadPlan plan = road::buildPlan({ bendLine(), crossingLine() }, params);

    ASSERT_FALSE(plan.brushes.empty());

    for (const road::BrushSolid& solid : plan.brushes)
    {
        for (const road::BrushFace& face : solid.faces)
        {
            road::FaceProjection projection;

            ASSERT_TRUE(road::faceProjection(face.plane, plan.frame, projection));

            Vector3 along = projection.points[1] - projection.points[0];
            Vector3 across = projection.points[2] - projection.points[0];

            EXPECT_GT(along.cross(across).getLength(), 1e-6);
        }
    }
}

TEST(RoadShapeTest, EveryBrushIsBoundedAndHasFourContributingFaces)
{
    road::RoadParams params = squareParams();
    params.curbStyle = road::CURB_ROUND;
    params.cornerStyle = road::CORNER_ROUND;

    road::RoadPlan plan = road::buildPlan({ bendLine(), crossingLine() }, params);

    ASSERT_FALSE(plan.brushes.empty());

    for (const road::BrushSolid& solid : plan.brushes)
    {
        int contributing = 0;

        for (std::size_t face = 0; face < solid.faces.size(); ++face)
        {
            std::vector<Vector3> winding = faceWinding(solid, face);

            if (windingArea(winding) < 1e-6)
            {
                continue;
            }

            ++contributing;

            for (const Vector3& point : winding)
            {
                ASSERT_TRUE(finite(point.x()) && finite(point.y()) && finite(point.z()));
                EXPECT_LT(point.getLength(), 8192);
            }
        }

        EXPECT_GE(contributing, 4);
    }
}

TEST(RoadStabilityTest, DegenerateCurvesProduceEmptyOrSanePlans)
{
    road::RoadParams params = squareParams();

    std::vector<std::vector<std::vector<Vector3>>> cases = {
        {},
        { control({ Vector3(0, 0, 0) }) },
        { control({ Vector3(0, 0, 0), Vector3(0, 0, 0), Vector3(0, 0, 0) }) },
        { control({ Vector3(0, 0, 0), Vector3(1, 0, 0), Vector3(2, 0, 0) }) },
        { control({ Vector3(0, 0, 0), Vector3(0, 0, 0), Vector3(0, 0, 0), Vector3(0, 0, 0) }) },
    };

    for (std::size_t index = 0; index < cases.size(); ++index)
    {
        road::RoadPlan plan = road::buildPlan(cases[index], params);

        for (const road::BrushSolid& solid : plan.brushes)
        {
            EXPECT_GE(solid.faces.size(), 4u) << "case " << index;

            for (const road::BrushFace& face : solid.faces)
            {
                ASSERT_TRUE(finite(face.plane.dist())) << "case " << index;
                EXPECT_GT(face.plane.normal().getLength(), 1e-6) << "case " << index;
            }
        }
    }
}

TEST(RoadStabilityTest, ExtremeParameterCombinationsStaySane)
{
    std::vector<double> laneWidths = { 1, 8, 512 };
    std::vector<double> sidewalkWidths = { 1, 96, 512 };
    std::vector<double> heights = { 1, 16, 256 };
    std::vector<double> radii = { 0, 8, 1024 };

    for (double laneWidth : laneWidths)
    {
        for (double sidewalkWidth : sidewalkWidths)
        {
            for (double height : heights)
            {
                for (double radius : radii)
                {
                    road::RoadParams params = squareParams();
                    params.laneWidth = laneWidth;
                    params.sidewalkWidth = sidewalkWidth;
                    params.sidewalkHeight = height;
                    params.curbStyle = road::CURB_ROUND;
                    params.curbRadius = radius;
                    params.cornerStyle = road::CORNER_ROUND;
                    params.cornerRadius = radius;

                    road::RoadPlan plan = road::buildPlan({ bendLine() }, params);

                    for (const road::BrushSolid& solid : plan.brushes)
                    {
                        for (const road::BrushFace& face : solid.faces)
                        {
                            ASSERT_TRUE(finite(face.plane.dist()));
                            ASSERT_TRUE(finite(face.plane.normal().x()));
                            EXPECT_GT(face.plane.normal().getLength(), 1e-6);

                            if (face.material == params.hiddenMaterial)
                            {
                                EXPECT_LT(faceNormal(face).z(), -0.99);
                            }
                        }
                    }
                }
            }
        }
    }
}

TEST(RoadProjectionTest, ProjectionPointsLieOnTheFaceAndSpanOneTextureRepeat)
{
    Plane3 top(Vector3(0, 0, 1), 64);
    road::FaceProjection projection;

    road::TextureFrame frame;
    frame.origin = Vector3(10, 20, 64);

    ASSERT_TRUE(road::faceProjection(top, frame, projection));

    for (const Vector3& point : projection.points)
    {
        EXPECT_NEAR(top.normal().dot(point) - top.dist(), 0, 1e-9);
    }

    EXPECT_NEAR((projection.points[1] - projection.points[0]).getLength(), 128, 1e-9);
    EXPECT_NEAR((projection.points[2] - projection.points[0]).getLength(), 128, 1e-9);
    EXPECT_NEAR((projection.points[1] - projection.points[0])
                    .dot(projection.points[2] - projection.points[0]), 0, 1e-6);
}

TEST(RoadProjectionTest, UAxisFollowsTheFrameAndUvsStartAtTheFrameOrigin)
{
    Plane3 top(Vector3(0, 0, 1), 0);
    road::FaceProjection projection;

    road::TextureFrame frame;
    frame.origin = Vector3(-256, 0, 0);
    frame.uAxis = Vector3(0.6, 0.8, 0);

    ASSERT_TRUE(road::faceProjection(top, frame, projection));

    Vector3 uAxis = (projection.points[1] - projection.points[0]).getNormalised();

    EXPECT_NEAR(uAxis.x(), 0.6, 1e-9);
    EXPECT_NEAR(uAxis.y(), 0.8, 1e-9);
    EXPECT_NEAR(uAxis.z(), 0, 1e-9);

    EXPECT_NEAR(uvAt(projection, Vector3(-256, 0, 0)).x(), 0.0, 1e-9);
    EXPECT_NEAR(uvAt(projection, Vector3(-256 + 0.6 * 256, 0.8 * 256, 0)).x(), 2.0, 1e-9);
    EXPECT_NEAR(uvAt(projection, Vector3(-256 + 0.8 * 128, -0.6 * 128, 0)).y(), 1.0, 1e-9);
}

TEST(RoadProjectionTest, TheProjectionOfAHorizontalFaceDependsOnlyOnTheWorldColumn)
{
    road::TextureFrame frame;
    road::FaceProjection flat;
    road::FaceProjection raised;
    road::FaceProjection sloped;

    ASSERT_TRUE(road::faceProjection(Plane3(Vector3(0, 0, 1), 0), frame, flat));
    ASSERT_TRUE(road::faceProjection(Plane3(Vector3(0, 0, 1), 16), frame, raised));
    ASSERT_TRUE(road::faceProjection(Plane3(Vector3(0, 0.3, 1).getNormalised(), 0), frame, sloped));

    for (const Vector2& column : { Vector2(64, 32), Vector2(-500, 220) })
    {
        Vector3 onFlat(column.x(), column.y(), 0);
        Vector3 onRaised(column.x(), column.y(), 16);
        Vector3 onSloped(column.x(), column.y(), -0.3 * column.y());

        Vector2 reference = uvAt(flat, onFlat);

        EXPECT_NEAR(uvAt(raised, onRaised).x(), reference.x(), 1e-9);
        EXPECT_NEAR(uvAt(raised, onRaised).y(), reference.y(), 1e-9);
        EXPECT_NEAR(uvAt(sloped, onSloped).x(), reference.x(), 1e-9);
        EXPECT_NEAR(uvAt(sloped, onSloped).y(), reference.y(), 1e-9);
    }
}

TEST(RoadProjectionTest, ParallelVerticalFacesShareOneUvField)
{
    road::TextureFrame frame;
    frame.origin = Vector3(-512, 0, 0);

    road::FaceProjection curb;
    road::FaceProjection outer;

    ASSERT_TRUE(road::faceProjection(Plane3(Vector3(0, -1, 0), -128), frame, curb));
    ASSERT_TRUE(road::faceProjection(Plane3(Vector3(0, -1, 0), 224), frame, outer));

    EXPECT_NEAR(uvAt(curb, Vector3(96, 128, -4)).x(), 4.75, 1e-9);
    EXPECT_NEAR(uvAt(curb, Vector3(96, 128, -4)).y(), 0.03125, 1e-9);
    EXPECT_NEAR(uvAt(outer, Vector3(96, -224, -4)).x(), 4.75, 1e-9);
    EXPECT_NEAR(uvAt(outer, Vector3(96, -224, -4)).y(), 0.03125, 1e-9);
}

TEST(RoadProjectionTest, BothCurbFacesGetTheirVAxisPointingDown)
{
    road::FaceProjection projection;
    road::TextureFrame frame;

    for (double sign : { 1.0, -1.0 })
    {
        Plane3 side(Vector3(0, sign, 0), 128);

        ASSERT_TRUE(road::faceProjection(side, frame, projection)) << "sign " << sign;

        EXPECT_NEAR((projection.points[2] - projection.points[0]).z(), -128, 1e-9)
            << "sign " << sign;
    }
}

TEST(RoadProjectionTest, EveryFaceOrientationGetsAProjection)
{
    road::TextureFrame frame;
    road::FaceProjection projection;

    std::vector<Vector3> normals = { Vector3(0, 0, 1),
                                     Vector3(0, 0, -1),
                                     Vector3(1, 0, 0),
                                     Vector3(-1, 0, 0),
                                     Vector3(0, 1, 0),
                                     Vector3(0, -1, 0),
                                     Vector3(0.6, 0, 0.8),
                                     Vector3(0.8, 0, 0.6),
                                     Vector3(0.5, 0.5, 0.7071067811865476) };

    for (std::size_t index = 0; index < normals.size(); ++index)
    {
        ASSERT_TRUE(road::faceProjection(Plane3(normals[index].getNormalised(), 64), frame,
                                         projection))
            << "normal " << index;

        Vector3 along = projection.points[1] - projection.points[0];
        Vector3 across = projection.points[2] - projection.points[0];

        EXPECT_GT(along.cross(across).getLength(), 1e-6) << "normal " << index;
    }
}

TEST(RoadProjectionTest, AZeroTextureScaleIsRejected)
{
    road::TextureFrame frame;
    frame.scale = 0;

    road::FaceProjection projection;

    EXPECT_FALSE(road::faceProjection(Plane3(Vector3(0, 0, 1), 0), frame, projection));
}

} // namespace test
