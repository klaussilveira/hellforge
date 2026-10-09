#include "Hotspot.h"

#include "i18n.h"
#include "ibrush.h"
#include "ipatch.h"
#include "iselection.h"
#include "iundo.h"
#include "iscenegraph.h"
#include "brush/TextureProjection.h"
#include "command/ExecutionNotPossible.h"
#include "hotspot/HotspotFit.h"
#include "hotspot/HotspotLookup.h"
#include "registry/registry.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <numeric>
#include <optional>
#include <random>

namespace selection
{

namespace algorithm
{

namespace
{

const char* const RKEY_DEFAULT_TEXTURE_SCALE = "user/ui/textures/defaultTextureScale";

std::mt19937& getRandomGenerator()
{
    static std::mt19937 generator{ std::random_device{}() };
    return generator;
}

double getUnitsPerPixel(const hotspot::Sheet& sheet)
{
    return sheet.scale > 0 ? sheet.scale : registry::getValue<double>(RKEY_DEFAULT_TEXTURE_SCALE, 0.5);
}

class SheetCache
{
    std::map<std::string, std::optional<hotspot::LoadedSheet>> _sheets;

public:
    const hotspot::Sheet* get(const std::string& material)
    {
        auto found = _sheets.find(material);

        if (found == _sheets.end())
        {
            found = _sheets.emplace(material, hotspot::findSheet(material)).first;
        }

        return found->second && !found->second->sheet.rects.empty() ? &found->second->sheet : nullptr;
    }
};

void getFaceBasis(const Vector3& normal, Vector3& right, Vector3& up)
{
    Vector3 worldUp = std::abs(normal.z()) < 0.7 ? Vector3(0, 0, 1) : Vector3(0, 1, 0);

    right = worldUp.cross(normal).getNormalised();
    up = normal.cross(right).getNormalised();
}

bool pointTouchesSegment(const Vector3& point, const Vector3& a, const Vector3& b)
{
    const double epsilon = 0.01;

    auto edge = b - a;
    auto lengthSquared = edge.getLengthSquared();
    auto t = lengthSquared > 0 ? std::clamp((point - a).dot(edge) / lengthSquared, 0.0, 1.0) : 0.0;

    return (a + edge * t - point).getLength() < epsilon;
}

bool windingTouches(const IWinding& a, const IWinding& b)
{
    for (const auto& vertex : a)
    {
        for (std::size_t i = 0; i < b.size(); ++i)
        {
            if (pointTouchesSegment(vertex.vertex, b[i].vertex, b[(i + 1) % b.size()].vertex))
            {
                return true;
            }
        }
    }

    return false;
}

bool facesConnected(IFace& a, IFace& b)
{
    const auto& planeA = a.getPlane3();
    const auto& planeB = b.getPlane3();

    if (planeA.normal().dot(planeB.normal()) < 1 - 1e-6 || std::abs(planeA.dist() - planeB.dist()) > 0.01)
    {
        return false;
    }

    return windingTouches(a.getWinding(), b.getWinding()) || windingTouches(b.getWinding(), a.getWinding());
}

std::vector<std::vector<IFace*>> buildIslands(const std::vector<IFace*>& faces, bool merge)
{
    std::vector<std::size_t> parent(faces.size());
    std::iota(parent.begin(), parent.end(), 0);

    std::function<std::size_t(std::size_t)> findRoot = [&](std::size_t i)
    {
        return parent[i] == i ? i : parent[i] = findRoot(parent[i]);
    };

    if (merge)
    {
        for (std::size_t i = 0; i < faces.size(); ++i)
        {
            for (std::size_t j = i + 1; j < faces.size(); ++j)
            {
                if (findRoot(i) != findRoot(j) && facesConnected(*faces[i], *faces[j]))
                {
                    parent[findRoot(j)] = findRoot(i);
                }
            }
        }
    }

    std::map<std::size_t, std::vector<IFace*>> islands;

    for (std::size_t i = 0; i < faces.size(); ++i)
    {
        islands[findRoot(i)].push_back(faces[i]);
    }

    std::vector<std::vector<IFace*>> result;

    for (auto& [root, island] : islands)
    {
        result.push_back(std::move(island));
    }

    return result;
}

bool fitFaces(const std::vector<IFace*>& faces, const hotspot::Sheet& sheet, bool alt, bool allowRotation)
{
    auto& first = *faces.front();

    if (first.getWinding().empty())
    {
        return false;
    }

    auto normal = first.getPlane3().normal();

    Vector3 right, up;
    getFaceBasis(normal, right, up);

    auto origin = first.getWinding().front().vertex;

    double minX = 0, maxX = 0, minY = 0, maxY = 0;

    for (auto* face : faces)
    {
        for (const auto& vertex : face->getWinding())
        {
            auto offset = vertex.vertex - origin;
            minX = std::min(minX, offset.dot(right));
            maxX = std::max(maxX, offset.dot(right));
            minY = std::min(minY, offset.dot(up));
            maxY = std::max(maxY, offset.dot(up));
        }
    }

    Vector3 points[3] =
    {
        origin + right * minX + up * maxY,
        origin + right * maxX + up * maxY,
        origin + right * minX + up * minY,
    };

    TextureProjection currentProjection;
    currentProjection.setTransform(first.getProjectionMatrix());

    hotspot::Placement current;
    current.topLeft = currentProjection.getTextureCoordsForVertex(points[0], normal, Matrix4::getIdentity());
    current.topRight = currentProjection.getTextureCoordsForVertex(points[1], normal, Matrix4::getIdentity());
    current.bottomLeft = currentProjection.getTextureCoordsForVertex(points[2], normal, Matrix4::getIdentity());

    auto placement = hotspot::choosePlacement(sheet, maxX - minX, maxY - minY, getUnitsPerPixel(sheet),
        alt, current, getRandomGenerator(), allowRotation);

    if (!placement)
    {
        return false;
    }

    Vector2 uvs[3] = { placement->topLeft, placement->topRight, placement->bottomLeft };

    for (auto* face : faces)
    {
        TextureProjection projection;
        projection.calculateFromPoints(points, uvs, face->getPlane3().normal());

        face->undoSave();
        face->setProjectionMatrix(projection.getMatrix());
    }

    return true;
}

bool fitPatch(IPatch& patch, const hotspot::Sheet& sheet, bool alt, bool allowRotation)
{
    auto width = patch.getWidth();
    auto height = patch.getHeight();

    if (width < 2 || height < 2)
    {
        return false;
    }

    std::vector<double> s(width * height, 0.0);
    std::vector<double> t(width * height, 0.0);

    double totalWidth = 0;
    double totalHeight = 0;

    for (std::size_t row = 0; row < height; ++row)
    {
        double length = 0;

        for (std::size_t col = 1; col < width; ++col)
        {
            length += (patch.ctrlAt(row, col).vertex - patch.ctrlAt(row, col - 1).vertex).getLength();
            s[row * width + col] = length;
        }

        for (std::size_t col = 0; col < width; ++col)
        {
            s[row * width + col] = length > 0 ? s[row * width + col] / length : static_cast<double>(col) / (width - 1);
        }

        totalWidth += length;
    }

    for (std::size_t col = 0; col < width; ++col)
    {
        double length = 0;

        for (std::size_t row = 1; row < height; ++row)
        {
            length += (patch.ctrlAt(row, col).vertex - patch.ctrlAt(row - 1, col).vertex).getLength();
            t[row * width + col] = length;
        }

        for (std::size_t row = 0; row < height; ++row)
        {
            t[row * width + col] = length > 0 ? t[row * width + col] / length : static_cast<double>(row) / (height - 1);
        }

        totalHeight += length;
    }

    double rise = 0;

    for (std::size_t col = 0; col < width; ++col)
    {
        rise += patch.ctrlAt(height - 1, col).vertex.z() - patch.ctrlAt(0, col).vertex.z();
    }

    bool flip = rise > 0;

    if (flip)
    {
        for (auto& value : t)
        {
            value = 1.0 - value;
        }
    }

    auto topRow = flip ? height - 1 : 0;
    auto bottomRow = flip ? 0 : height - 1;

    hotspot::Placement current;
    current.topLeft = patch.ctrlAt(topRow, 0).texcoord;
    current.topRight = patch.ctrlAt(topRow, width - 1).texcoord;
    current.bottomLeft = patch.ctrlAt(bottomRow, 0).texcoord;

    auto placement = hotspot::choosePlacement(sheet, totalWidth / height, totalHeight / width,
        getUnitsPerPixel(sheet), alt, current, getRandomGenerator(), allowRotation);

    if (!placement)
    {
        return false;
    }

    auto across = placement->topRight - placement->topLeft;
    auto down = placement->bottomLeft - placement->topLeft;

    patch.undoSave();

    for (std::size_t row = 0; row < height; ++row)
    {
        for (std::size_t col = 0; col < width; ++col)
        {
            auto index = row * width + col;
            patch.ctrlAt(row, col).texcoord = placement->topLeft + across * s[index] + down * t[index];
        }
    }

    patch.controlPointsChanged();

    return true;
}

}

std::size_t hotspotSelection(bool alt)
{
    SheetCache cache;
    std::map<const hotspot::Sheet*, std::vector<IFace*>> facesBySheet;
    std::size_t fitted = 0;

    GlobalSelectionSystem().foreachFace([&](IFace& face)
    {
        if (auto* sheet = cache.get(face.getShader()); sheet)
        {
            facesBySheet[sheet].push_back(&face);
        }
    });

    bool merge = registry::getValue<bool>(hotspot::RKEY_MERGE_COPLANAR);
    bool allowRotation = registry::getValue<bool>(hotspot::RKEY_ALLOW_ROTATION);

    for (const auto& [sheet, faces] : facesBySheet)
    {
        for (const auto& island : buildIslands(faces, merge))
        {
            if (fitFaces(island, *sheet, alt, allowRotation))
            {
                fitted += island.size();
            }
        }
    }

    GlobalSelectionSystem().foreachPatch([&](IPatch& patch)
    {
        if (auto* sheet = cache.get(patch.getShader()); sheet && fitPatch(patch, *sheet, alt, allowRotation))
        {
            ++fitted;
        }
    });

    SceneChangeNotify();

    return fitted;
}

void applyHotspotForMaterial(const std::string& material)
{
    if (auto loaded = hotspot::findSheet(material); loaded && !loaded->sheet.rects.empty())
    {
        hotspotSelection(false);
    }
}

namespace
{

void runHotspotCommand(bool alt)
{
    if (GlobalSelectionSystem().countSelected() == 0 && GlobalSelectionSystem().getSelectedFaceCount() == 0)
    {
        throw cmd::ExecutionNotPossible(_("Nothing selected to hotspot."));
    }

    UndoableCommand undo(alt ? "hotspotTextureAlt" : "hotspotTexture");

    if (hotspotSelection(alt) == 0)
    {
        throw cmd::ExecutionNotPossible(_("None of the selected materials has a hotspot rect file."));
    }
}

}

void hotspotTextureCmd(const cmd::ArgumentList& args)
{
    runHotspotCommand(false);
}

void hotspotTextureAltCmd(const cmd::ArgumentList& args)
{
    runHotspotCommand(true);
}

}

}
