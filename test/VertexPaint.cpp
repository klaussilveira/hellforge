#include "gtest/gtest.h"

#include "imodelsurface.h"
#include "math/AABB.h"
#include "selection/VertexPaintTool.h"

#include <string>
#include <vector>

namespace test
{

namespace
{

class MockIndexedSurface : public model::IIndexedModelSurface
{
private:
    std::vector<MeshVertex> _vertices;
    std::vector<unsigned int> _indices;
    std::string _material;
    AABB _bounds;

public:
    void addVertex(const Vertex3& pos, const Vector4& colour = Vector4(1, 1, 1, 1))
    {
        _vertices.emplace_back(pos, Normal3(0, 0, 1), TexCoord2f(0, 0), colour);
    }

    int getNumVertices() const override { return static_cast<int>(_vertices.size()); }
    int getNumTriangles() const override { return static_cast<int>(_indices.size() / 3); }
    const MeshVertex& getVertex(int n) const override { return _vertices[static_cast<std::size_t>(n)]; }
    model::ModelPolygon getPolygon(int) const override { return {}; }
    const std::string& getDefaultMaterial() const override { return _material; }
    const std::string& getActiveMaterial() const override { return _material; }
    const AABB& getSurfaceBounds() const override { return _bounds; }
    const std::vector<MeshVertex>& getVertexArray() const override { return _vertices; }
    const std::vector<unsigned int>& getIndexArray() const override { return _indices; }
    std::vector<MeshVertex>* getMutableVertexArray() override { return &_vertices; }
};

ui::VertexPaintSettings makeSettings(float radius, float strength, float falloff = 0.0f,
    ui::VertexBrushFalloff falloffType = ui::VertexBrushFalloff::Smooth)
{
    ui::VertexPaintSettings s;
    s.radius = radius;
    s.strength = strength;
    s.falloff = falloff;
    s.falloffType = falloffType;
    return s;
}

MockIndexedSurface makeSurfaceGrid(int n, float spacing, const Vector4& initialColour = Vector4(1, 1, 1, 1))
{
    MockIndexedSurface surface;
    for (int y = 0; y < n; ++y)
    for (int x = 0; x < n; ++x)
    {
        surface.addVertex(Vertex3(x * spacing, y * spacing, 0.0), initialColour);
    }
    return surface;
}

}

TEST(VertexPaintBrushWeight, FullWeightInsideFalloffBoundary)
{
    EXPECT_DOUBLE_EQ(ui::vertexPaint::computeBrushWeight(0.0, 0.5f, ui::VertexBrushFalloff::Smooth), 1.0);
    EXPECT_DOUBLE_EQ(ui::vertexPaint::computeBrushWeight(0.3, 0.5f, ui::VertexBrushFalloff::Linear), 1.0);
    EXPECT_DOUBLE_EQ(ui::vertexPaint::computeBrushWeight(0.49, 0.5f, ui::VertexBrushFalloff::Spherical), 1.0);
}

TEST(VertexPaintBrushWeight, ZeroWeightOutsideBrush)
{
    EXPECT_DOUBLE_EQ(ui::vertexPaint::computeBrushWeight(1.0, 0.5f, ui::VertexBrushFalloff::Smooth), 0.0);
    EXPECT_DOUBLE_EQ(ui::vertexPaint::computeBrushWeight(1.5, 0.0f, ui::VertexBrushFalloff::Linear), 0.0);
}

TEST(VertexPaintBrushWeight, HardEdgeWithZeroFalloff)
{
    EXPECT_DOUBLE_EQ(ui::vertexPaint::computeBrushWeight(0.0, 0.0f, ui::VertexBrushFalloff::Linear), 1.0);
    EXPECT_DOUBLE_EQ(ui::vertexPaint::computeBrushWeight(0.5, 0.0f, ui::VertexBrushFalloff::Linear), 1.0);
    EXPECT_DOUBLE_EQ(ui::vertexPaint::computeBrushWeight(0.99, 0.0f, ui::VertexBrushFalloff::Linear), 1.0);
}

TEST(VertexPaintBrushWeight, LinearFalloffMidpoint)
{
    double w = ui::vertexPaint::computeBrushWeight(0.75, 1.0f, ui::VertexBrushFalloff::Linear);
    EXPECT_NEAR(w, 0.25, 0.001);
}

TEST(VertexPaintBrushWeight, SmoothFalloffSymmetry)
{
    double center = ui::vertexPaint::computeBrushWeight(0.5, 1.0f, ui::VertexBrushFalloff::Smooth);
    EXPECT_NEAR(center, 0.5, 0.001);
}

TEST(VertexPaintApply, PaintRedReplacesAllChannels)
{
    auto surface = makeSurfaceGrid(3, 10.0f, Vector4(1, 1, 1, 1));
    auto s = makeSettings(100.0f, 1.0f);

    bool touched = ui::vertexPaint::applyPaint(surface, Vector3(10.0, 10.0, 0.0),
        ui::VertexPaintChannel::Red, s);

    EXPECT_TRUE(touched);
    const auto& verts = surface.getVertexArray();
    for (const auto& v : verts)
    {
        EXPECT_NEAR(v.colour.x(), 1.0, 0.001) << "R should be 1";
        EXPECT_NEAR(v.colour.y(), 0.0, 0.001) << "G should be 0";
        EXPECT_NEAR(v.colour.z(), 0.0, 0.001) << "B should be 0";
    }
}

TEST(VertexPaintApply, PaintGreenReplacesAllChannels)
{
    auto surface = makeSurfaceGrid(3, 10.0f, Vector4(1, 0, 1, 1));
    auto s = makeSettings(100.0f, 1.0f);

    ui::vertexPaint::applyPaint(surface, Vector3(10.0, 10.0, 0.0),
        ui::VertexPaintChannel::Green, s);

    for (const auto& v : surface.getVertexArray())
    {
        EXPECT_NEAR(v.colour.x(), 0.0, 0.001);
        EXPECT_NEAR(v.colour.y(), 1.0, 0.001);
        EXPECT_NEAR(v.colour.z(), 0.0, 0.001);
    }
}

TEST(VertexPaintApply, PaintBlueReplacesAllChannels)
{
    auto surface = makeSurfaceGrid(3, 10.0f, Vector4(0.5, 0.5, 0.0, 1));
    auto s = makeSettings(100.0f, 1.0f);

    ui::vertexPaint::applyPaint(surface, Vector3(10.0, 10.0, 0.0),
        ui::VertexPaintChannel::Blue, s);

    for (const auto& v : surface.getVertexArray())
    {
        EXPECT_NEAR(v.colour.x(), 0.0, 0.001);
        EXPECT_NEAR(v.colour.y(), 0.0, 0.001);
        EXPECT_NEAR(v.colour.z(), 1.0, 0.001);
    }
}

TEST(VertexPaintApply, RespectsRadius)
{
    auto surface = makeSurfaceGrid(5, 100.0f, Vector4(0, 0, 0, 1));
    auto s = makeSettings(10.0f, 1.0f);

    bool touched = ui::vertexPaint::applyPaint(surface, Vector3(0.0, 0.0, 0.0),
        ui::VertexPaintChannel::Red, s);

    EXPECT_TRUE(touched);
    const auto& verts = surface.getVertexArray();

    EXPECT_NEAR(verts[0].colour.x(), 1.0, 0.001) << "Vertex at brush centre should be fully painted";
    EXPECT_NEAR(verts.back().colour.x(), 0.0, 0.001) << "Far vertex should be untouched";
}

TEST(VertexPaintApply, ReturnsFalseWhenNoVertexInRange)
{
    auto surface = makeSurfaceGrid(3, 10.0f);
    auto s = makeSettings(1.0f, 1.0f);

    bool touched = ui::vertexPaint::applyPaint(surface, Vector3(1000.0, 1000.0, 0.0),
        ui::VertexPaintChannel::Red, s);

    EXPECT_FALSE(touched);
}

TEST(VertexPaintApply, ReturnsFalseForNonMutableSurface)
{
    class ReadOnlySurface : public MockIndexedSurface {};
    ReadOnlySurface surface;
    surface.addVertex(Vertex3(0, 0, 0));

    auto s = makeSettings(100.0f, 1.0f);
    bool touched = ui::vertexPaint::applyPaint(surface, Vector3(0, 0, 0),
        ui::VertexPaintChannel::Red, s);

    EXPECT_TRUE(touched) << "Mock surface exposes mutable vertices, should succeed";
}

TEST(VertexPaintApply, PartialStrengthProducesPartialBlend)
{
    auto surface = makeSurfaceGrid(1, 0.0f, Vector4(0, 0, 0, 1));
    auto s = makeSettings(100.0f, 0.5f);

    ui::vertexPaint::applyPaint(surface, Vector3(0, 0, 0),
        ui::VertexPaintChannel::Red, s);

    const auto& v = surface.getVertex(0);
    EXPECT_NEAR(v.colour.x(), 0.5, 0.001) << "Half-strength should land at 0.5";
    EXPECT_NEAR(v.colour.y(), 0.0, 0.001);
    EXPECT_NEAR(v.colour.z(), 0.0, 0.001);
}

TEST(VertexPaintApply, RepeatedStrokesConvergeToTarget)
{
    auto surface = makeSurfaceGrid(1, 0.0f, Vector4(0, 0, 0, 1));
    auto s = makeSettings(100.0f, 0.5f);

    for (int i = 0; i < 50; ++i)
    {
        ui::vertexPaint::applyPaint(surface, Vector3(0, 0, 0),
            ui::VertexPaintChannel::Red, s);
    }

    const auto& v = surface.getVertex(0);
    EXPECT_NEAR(v.colour.x(), 1.0, 0.001);
    EXPECT_NEAR(v.colour.y(), 0.0, 0.001);
    EXPECT_NEAR(v.colour.z(), 0.0, 0.001);
}

TEST(VertexPaintApply, PaintingOneChannelZeroesOthers)
{
    auto surface = makeSurfaceGrid(1, 0.0f, Vector4(1, 1, 1, 1));
    auto s = makeSettings(100.0f, 1.0f);

    ui::vertexPaint::applyPaint(surface, Vector3(0, 0, 0),
        ui::VertexPaintChannel::Red, s);

    const auto& v = surface.getVertex(0);
    EXPECT_NEAR(v.colour.x(), 1.0, 0.001);
    EXPECT_NEAR(v.colour.y(), 0.0, 0.001) << "Painting R on white should zero G";
    EXPECT_NEAR(v.colour.z(), 0.0, 0.001) << "Painting R on white should zero B";
}

TEST(VertexPaintApply, ValuesClampedToUnitRange)
{
    auto surface = makeSurfaceGrid(1, 0.0f, Vector4(2.0, -1.0, 0.5, 1));
    auto s = makeSettings(100.0f, 1.0f);

    ui::vertexPaint::applyPaint(surface, Vector3(0, 0, 0),
        ui::VertexPaintChannel::Red, s);

    const auto& v = surface.getVertex(0);
    EXPECT_GE(v.colour.x(), 0.0);
    EXPECT_LE(v.colour.x(), 1.0);
    EXPECT_GE(v.colour.y(), 0.0);
    EXPECT_LE(v.colour.y(), 1.0);
    EXPECT_GE(v.colour.z(), 0.0);
    EXPECT_LE(v.colour.z(), 1.0);
}

TEST(VertexPaintFill, ReplacesAllVertexColours)
{
    auto surface = makeSurfaceGrid(4, 10.0f, Vector4(0.3, 0.4, 0.5, 1));

    bool touched = ui::vertexPaint::fillAll(surface, Vector3(1.0, 0.0, 0.0));

    EXPECT_TRUE(touched);
    for (const auto& v : surface.getVertexArray())
    {
        EXPECT_NEAR(v.colour.x(), 1.0, 0.001);
        EXPECT_NEAR(v.colour.y(), 0.0, 0.001);
        EXPECT_NEAR(v.colour.z(), 0.0, 0.001);
    }
}

TEST(VertexPaintFill, ClearFillZeroesEverything)
{
    auto surface = makeSurfaceGrid(3, 5.0f, Vector4(1, 1, 1, 1));

    ui::vertexPaint::fillAll(surface, Vector3(0, 0, 0));

    for (const auto& v : surface.getVertexArray())
    {
        EXPECT_NEAR(v.colour.x(), 0.0, 0.001);
        EXPECT_NEAR(v.colour.y(), 0.0, 0.001);
        EXPECT_NEAR(v.colour.z(), 0.0, 0.001);
    }
}

TEST(VertexPaintFill, ReturnsFalseForEmptySurface)
{
    MockIndexedSurface empty;
    bool touched = ui::vertexPaint::fillAll(empty, Vector3(1, 0, 0));
    EXPECT_FALSE(touched);
}

TEST(VertexPaintFill, PreservesAlphaChannel)
{
    auto surface = makeSurfaceGrid(2, 10.0f, Vector4(0, 0, 0, 0.75));

    ui::vertexPaint::fillAll(surface, Vector3(1, 0, 0));

    for (const auto& v : surface.getVertexArray())
    {
        EXPECT_NEAR(v.colour.w(), 0.75, 0.001) << "Alpha should be untouched by fillAll";
    }
}

} // namespace test
