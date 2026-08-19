#pragma once

#include "ibrush.h"
#include "ieclass.h"
#include "ientity.h"
#include "ipatch.h"
#include "iscenegraph.h"
#include "scene/Entity.h"
#include "scene/EntityNode.h"
#include "scenelib.h"
#include "gamelib.h"
#include "texturelib.h"
#include "math/AABB.h"
#include "math/Matrix3.h"
#include "math/Matrix4.h"
#include "math/Plane3.h"
#include "math/Vector3.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace facade
{
const double GRID = 20.0;

enum PresetType
{
    PRESET_BLANK = 0,
    PRESET_HONGKONG = 1,
    PRESET_NEWYORK = 2,
    PRESET_CURTAIN = 3,
    PRESET_BRUTALIST = 4,
    PRESET_WAREHOUSE = 5,
    PRESET_RIBBON = 6,
};

enum FrontSide
{
    FRONT_AUTO = 0,
    FRONT_XPOS = 1,
    FRONT_XNEG = 2,
    FRONT_YPOS = 3,
    FRONT_YNEG = 4,
};

enum BandKind
{
    BAND_PLINTH = 0,
    BAND_GROUND = 1,
    BAND_UPPER = 2,
    BAND_CORNICE = 3,
    BAND_PARAPET = 4,
    BAND_TOP = 5,
};

inline double snapTo(double value, double increment)
{
    if (increment <= 0)
    {
        return value;
    }

    return std::floor(value / increment + 0.5) * increment;
}

inline double getTextureScale()
{
    return game::current::getValue<double>("/generators/texScale", 1.0 / 128.0);
}

struct BandStyle
{
    double height = 120;
    double frontOffset = 0;
    int bayCount = 0;
    double bayPitch = 120;
    double openingWidth = 60;
    double openingHeight = 80;
    double sillHeight = 40;
};

struct FacadeParams
{
    bool fitToSource = true;
    int floorCount = 3;

    double wallThickness = 20;
    double minEndPier = 20;

    double plinthHeight = 0;
    double plinthProud = 0;

    BandStyle ground;
    BandStyle upper;
    BandStyle top;
    bool hasTopFloor = false;

    bool groundDoor = true;
    double doorWidth = 60;
    double doorHeight = 100;
    bool arcadeColumns = false;

    double pierProud = 0;
    double trimProud = 0;
    double courseHeight = 0;
    double courseProud = 0;
    double corniceHeight = 0;
    double corniceProud = 0;
    int corniceSteps = 2;
    double parapetHeight = 0;

    bool solidBody = true;
    bool trimAsEntity = true;
    double tileUnits = 40;

    std::string wallMaterial = "_default";
    std::string trimMaterial = "_default";
    std::string revealMaterial = "_default";
    std::string hiddenMaterial = "textures/common/caulk";
};

struct FacadePath
{
    std::vector<Vector3> points;
    double baseZ = 0;
    double topZ = 0;
    double bodyDepth = 0;
};

struct FaceMaterials
{
    std::string front;
    std::string back;
    std::string sides;
    std::string top;
    std::string bottom;
};

struct PathFrame
{
    Vector3 origin;
    Vector3 tangent;
    Vector3 normal;
    double a0 = 0;
    double a1 = 0;
};

struct PlaneFace
{
    Plane3 plane;
    std::string material;
};

struct Band
{
    double z0 = 0;
    double z1 = 0;
    double frontOffset = 0;
    int kind = BAND_UPPER;
};

struct OpeningRun
{
    std::size_t first = 0;
    std::size_t last = 0;
    double z0 = 0;
    double z1 = 0;
};

struct Opening
{
    double a0 = 0;
    double a1 = 0;
    double z0 = 0;
    double z1 = 0;
};

inline std::vector<PathFrame> buildFrames(const FacadePath& path)
{
    std::vector<PathFrame> frames;
    double arc = 0;

    for (std::size_t i = 0; i + 1 < path.points.size(); ++i)
    {
        Vector3 delta = path.points[i + 1] - path.points[i];
        delta.z() = 0;

        double length = delta.getLength();

        if (length < 0.5)
        {
            continue;
        }

        PathFrame frame;
        frame.origin = path.points[i];
        frame.origin.z() = 0;
        frame.tangent = delta / length;
        frame.normal = Vector3(-frame.tangent.y(), frame.tangent.x(), 0);
        frame.a0 = arc;
        frame.a1 = arc + length;

        frames.push_back(frame);
        arc = frame.a1;
    }

    return frames;
}

inline double pathLength(const std::vector<PathFrame>& frames)
{
    return frames.empty() ? 0.0 : frames.back().a1;
}

inline scene::INodePtr createTrimEntity()
{
    auto eclass = GlobalEntityClassManager().findClass("func_static");

    if (!eclass)
    {
        return scene::INodePtr();
    }

    scene::INodePtr node = std::static_pointer_cast<scene::INode>(
        GlobalEntityModule().createEntity(eclass));

    if (!node)
    {
        return scene::INodePtr();
    }

    scene::addNodeToContainer(node, GlobalSceneGraph().root());

    if (Entity* entity = node->tryGetEntity())
    {
        entity->setKeyValue("model", entity->getKeyValue("name"));
        entity->setKeyValue("origin", "0 0 0");
    }

    return node;
}

inline void fitBrushTextures(IBrush& brush, double tileUnits)
{
    if (tileUnits <= 0)
    {
        return;
    }

    for (std::size_t i = 0; i < brush.getNumFaces(); ++i)
    {
        IFace& face = brush.getFace(i);
        const IWinding& winding = face.getWinding();

        if (winding.size() < 3)
        {
            continue;
        }

        Matrix4 basis = getBasisTransformForNormal(face.getPlane3().normal());
        AABB bounds;

        for (const WindingVertex& vertex : winding)
        {
            bounds.includePoint(basis.transformPoint(vertex.vertex));
        }

        double width = bounds.extents.x() * 2.0;
        double height = bounds.extents.y() * 2.0;

        if (width < 0.5 || height < 0.5)
        {
            continue;
        }

        face.fitTexture(static_cast<float>(width / tileUnits),
                        static_cast<float>(height / tileUnits));
    }
}

inline scene::INodePtr createBrush(const std::vector<PlaneFace>& faces, double tileUnits,
                                   const scene::INodePtr& parent)
{
    auto brushNode = GlobalBrushCreator().createBrush();
    auto& brush = *Node_getIBrush(brushNode);

    double texScale = getTextureScale();
    Matrix3 projection = Matrix3::getIdentity();
    projection.xx() = texScale;
    projection.yy() = texScale;

    for (const PlaneFace& face : faces)
    {
        brush.addFace(face.plane, projection, face.material);
    }

    brush.evaluateBRep();
    brush.removeEmptyFaces();

    if (!brush.hasContributingFaces())
    {
        return scene::INodePtr();
    }

    scene::addNodeToContainer(brushNode, parent);

    fitBrushTextures(brush, tileUnits);

    return brushNode;
}

inline bool miterNormal(const Vector3& before, const Vector3& after, Vector3& result)
{
    Vector3 sum = before + after;

    if (sum.getLength() < 0.01)
    {
        return false;
    }

    result = sum.getNormalised();

    return true;
}

inline void emitSpan(const std::vector<PathFrame>& frames,
                     double a0, double a1, double z0, double z1,
                     double back, double front,
                     const FaceMaterials& materials, double tileUnits,
                     const scene::INodePtr& parent,
                     std::vector<scene::INodePtr>& result)
{
    if (a1 - a0 < 0.5 || z1 - z0 < 0.5 || front - back < 0.5)
    {
        return;
    }

    for (std::size_t i = 0; i < frames.size(); ++i)
    {
        const PathFrame& frame = frames[i];

        double s0 = std::max(a0, frame.a0);
        double s1 = std::min(a1, frame.a1);

        if (s1 - s0 < 0.5)
        {
            continue;
        }

        const Vector3& normal = frame.normal;
        double base = normal.dot(frame.origin);

        std::vector<PlaneFace> faces;
        faces.push_back({Plane3(normal, base + front), materials.front});
        faces.push_back({Plane3(-normal, -(base + back)), materials.back});
        faces.push_back({Plane3(0, 0, 1, z1), materials.top});
        faces.push_back({Plane3(0, 0, -1, -z0), materials.bottom});

        Vector3 miter;

        if (s0 > a0 + 0.01 && i > 0 &&
            miterNormal(frames[i - 1].tangent, frame.tangent, miter))
        {
            faces.push_back({Plane3(-miter, -miter.dot(frame.origin)), materials.sides});
        }
        else
        {
            Vector3 point = frame.origin + frame.tangent * (s0 - frame.a0);
            faces.push_back(
                {Plane3(-frame.tangent, -frame.tangent.dot(point)), materials.sides});
        }

        if (s1 < a1 - 0.01 && i + 1 < frames.size() &&
            miterNormal(frame.tangent, frames[i + 1].tangent, miter))
        {
            Vector3 vertex = frame.origin + frame.tangent * (frame.a1 - frame.a0);
            faces.push_back({Plane3(miter, miter.dot(vertex)), materials.sides});
        }
        else
        {
            Vector3 point = frame.origin + frame.tangent * (s1 - frame.a0);
            faces.push_back({Plane3(frame.tangent, frame.tangent.dot(point)), materials.sides});
        }

        auto node = createBrush(faces, tileUnits, parent);

        if (node)
        {
            result.push_back(node);
        }
    }
}

inline std::vector<Opening> layoutOpenings(double length, double bandZ0,
                                           const BandStyle& style, double minEndPier,
                                           bool door, double doorWidth, double doorHeight)
{
    std::vector<Opening> openings;

    if (style.openingWidth <= 0 || style.openingHeight <= 0 || length <= 0)
    {
        return openings;
    }

    int count;
    double pitch;
    double left;

    if (style.bayCount > 0)
    {
        double margin = std::max(0.0, minEndPier);
        double usable = length - 2 * margin;

        if (usable < style.openingWidth + GRID)
        {
            return openings;
        }

        int maxCount = static_cast<int>(usable / (style.openingWidth + GRID));
        count = std::min(style.bayCount, std::max(1, maxCount));
        pitch = usable / count;
        left = margin;
    }
    else
    {
        if (style.bayPitch <= 0)
        {
            return openings;
        }

        pitch = style.bayPitch;
        count = static_cast<int>((length - 2 * minEndPier) / pitch);

        if (count < 1)
        {
            return openings;
        }

        double leftover = length - count * pitch;
        left = std::max(0.0, std::min(snapTo(leftover * 0.5, GRID), leftover));
    }

    if (style.openingWidth >= pitch)
    {
        return openings;
    }

    int doorIndex = door ? count / 2 : -1;
    double endLimit = (style.bayCount > 0) ? length - std::max(0.0, minEndPier) : length;
    double previousEnd = left;

    for (int i = 0; i < count; ++i)
    {
        double centre = left + (i + 0.5) * pitch;

        bool isDoor = (i == doorIndex) && doorWidth > 0 && doorWidth < pitch && doorHeight > 0;
        double width = isDoor ? doorWidth : style.openingWidth;

        double a0 = snapTo(centre - width * 0.5, GRID);

        double lowLimit = (i == 0) ? previousEnd : previousEnd + GRID;

        if (a0 < lowLimit)
        {
            a0 = std::ceil(lowLimit / GRID - 0.001) * GRID;
        }

        double a1 = a0 + width;

        if (a1 > endLimit + 0.001)
        {
            continue;
        }

        Opening opening;
        opening.a0 = a0;
        opening.a1 = a1;

        if (isDoor)
        {
            opening.z0 = bandZ0;
            opening.z1 = bandZ0 + doorHeight;
        }
        else
        {
            opening.z0 = bandZ0 + style.sillHeight;
            opening.z1 = opening.z0 + style.openingHeight;
        }

        openings.push_back(opening);
        previousEnd = a1;
    }

    return openings;
}

enum SpanRole
{
    SPAN_PIER = 0,
    SPAN_SILL_BAND = 1,
    SPAN_HEAD_BAND = 2,
    SPAN_MULLION = 3,
    SPAN_PILASTER = 4,
    SPAN_COLUMN = 5,
    SPAN_SILL_TRIM = 6,
    SPAN_HEAD_TRIM = 7,
    SPAN_COURSE = 8,
    SPAN_PLINTH = 9,
    SPAN_CORNICE = 10,
    SPAN_PARAPET = 11,
    SPAN_BODY = 12,
};

inline bool spanIsTrim(int role)
{
    return role == SPAN_SILL_TRIM || role == SPAN_HEAD_TRIM || role == SPAN_COURSE ||
           role == SPAN_CORNICE;
}

struct WallSpan
{
    double a0 = 0;
    double a1 = 0;
    double z0 = 0;
    double z1 = 0;
    double back = 0;
    double front = 0;
    int role = SPAN_PIER;
};

inline std::vector<WallSpan> planWallBand(double length, const Band& band,
                                          const std::vector<Opening>& openings,
                                          const FacadeParams& params,
                                          bool columns, bool hasCourse)
{
    std::vector<WallSpan> spans;

    double front = band.frontOffset;
    double back = -params.wallThickness;

    double pilaster = columns ? 0.0 : std::max(0.0, params.pierProud);
    double courseTop = hasCourse ? band.z0 + params.courseHeight : band.z0;

    auto add = [&](double a0, double a1, double z0, double z1,
                   double spanBack, double spanFront, int role)
    {
        if (a1 - a0 < 0.5 || z1 - z0 < 0.5 || spanFront - spanBack < 0.5)
        {
            return;
        }

        spans.push_back({a0, a1, z0, z1, spanBack, spanFront, role});
    };

    std::vector<Opening> usable;

    for (const Opening& opening : openings)
    {
        Opening clamped = opening;
        clamped.z0 = std::max(band.z0, opening.z0);
        clamped.z1 = std::min(band.z1 - GRID, opening.z1);

        if (clamped.z1 - clamped.z0 >= GRID && clamped.a1 - clamped.a0 >= GRID)
        {
            usable.push_back(clamped);
        }
    }

    std::vector<OpeningRun> runs;

    for (std::size_t i = 0; i < usable.size(); ++i)
    {
        if (!runs.empty() && std::abs(runs.back().z0 - usable[i].z0) < 0.01 &&
            std::abs(runs.back().z1 - usable[i].z1) < 0.01)
        {
            runs.back().last = i;
        }
        else
        {
            runs.push_back({i, i, usable[i].z0, usable[i].z1});
        }
    }

    double cursor = 0;

    for (const OpeningRun& run : runs)
    {
        double runA0 = usable[run.first].a0;
        double runA1 = usable[run.last].a1;

        add(cursor, runA0, band.z0, band.z1, back, front, SPAN_PIER);
        add(runA0, runA1, band.z0, run.z0, back, front, SPAN_SILL_BAND);
        add(runA0, runA1, run.z1, band.z1, back, front, SPAN_HEAD_BAND);

        for (std::size_t i = run.first; i < run.last; ++i)
        {
            add(usable[i].a1, usable[i + 1].a0, run.z0, run.z1, back, front,
                SPAN_MULLION);
        }

        cursor = runA1;
    }

    add(cursor, length, band.z0, band.z1, back, front, SPAN_PIER);

    if (pilaster > 0.5 || columns)
    {
        double outer = columns ? params.upper.frontOffset : front + pilaster;
        int role = columns ? SPAN_COLUMN : SPAN_PILASTER;
        double strip = 0;

        for (const Opening& opening : usable)
        {
            add(strip, opening.a0, band.z0, band.z1, front, outer, role);
            strip = opening.a1;
        }

        add(strip, length, band.z0, band.z1, front, outer, role);
    }

    if (params.trimProud > 0.5)
    {
        double reach = params.trimProud;

        double side = (columns || pilaster > 0.5) ? 0.0 : reach;

        for (std::size_t i = 0; i < usable.size(); ++i)
        {
            const Opening& opening = usable[i];

            auto clampSide = [&](double gap)
            {
                double half = std::floor(gap * 0.5 / GRID) * GRID;
                return std::max(0.0, std::min(side, half));
            };

            double sideLeft = (i == 0) ? side
                                       : clampSide(opening.a0 - usable[i - 1].a1);
            double sideRight = (i + 1 == usable.size())
                                   ? side
                                   : clampSide(usable[i + 1].a0 - opening.a1);

            if (opening.z0 - reach >= courseTop)
            {
                add(opening.a0 - sideLeft, opening.a1 + sideRight, opening.z0 - reach,
                    opening.z0, front, front + reach, SPAN_SILL_TRIM);
            }

            if (opening.z1 + reach <= band.z1 + 0.01)
            {
                add(opening.a0 - sideLeft, opening.a1 + sideRight, opening.z1,
                    opening.z1 + reach, front, front + reach, SPAN_HEAD_TRIM);
            }
        }
    }

    if (hasCourse)
    {
        double courseBack = front + pilaster;

        add(0, length, band.z0, band.z0 + params.courseHeight, courseBack,
            courseBack + params.courseProud, SPAN_COURSE);
    }

    return spans;
}

inline std::vector<Band> buildBands(const FacadeParams& params, double baseZ, double topZ)
{
    std::vector<Band> bands;

    bool fit = params.fitToSource && (topZ - baseZ) >= GRID;
    double z = baseZ;
    double capHeight = params.corniceHeight + params.parapetHeight;

    if (params.plinthHeight > 0)
    {
        bands.push_back({z, z + params.plinthHeight,
                         params.ground.frontOffset + params.plinthProud, BAND_PLINTH});
        z += params.plinthHeight;
    }

    double groundHeight = std::max(GRID, params.ground.height);
    bands.push_back({z, z + groundHeight, params.ground.frontOffset, BAND_GROUND});
    z += groundHeight;

    int count = std::max(0, params.floorCount);
    double height = std::max(GRID, params.upper.height);

    if (params.hasTopFloor)
    {
        capHeight += std::max(GRID, params.top.height);
    }

    if (fit)
    {
        double remaining = topZ - z - capHeight;

        if (remaining < GRID)
        {
            count = 0;
        }
        else
        {
            count = std::max(1, static_cast<int>(std::floor(remaining / height + 0.5)));
            height = std::max(GRID, std::floor(remaining / count / GRID) * GRID);

            while (count > 1 && count * height > remaining + 0.01)
            {
                --count;
            }
        }
    }

    for (int i = 0; i < count; ++i)
    {
        bands.push_back({z, z + height, params.upper.frontOffset, BAND_UPPER});
        z += height;
    }

    if (params.hasTopFloor)
    {
        double topHeight = std::max(GRID, params.top.height);
        bands.push_back({z, z + topHeight, params.upper.frontOffset, BAND_TOP});
        z += topHeight;
    }

    if (params.corniceHeight > 0)
    {
        bands.push_back({z, z + params.corniceHeight, params.upper.frontOffset, BAND_CORNICE});
        z += params.corniceHeight;
    }

    double parapetHeight = params.parapetHeight;

    if (fit)
    {
        double residual = topZ - z;
        parapetHeight = residual >= GRID ? residual : 0;
    }

    if (parapetHeight > 0.5)
    {
        bands.push_back({z, z + parapetHeight, params.upper.frontOffset, BAND_PARAPET});
    }

    return bands;
}

inline std::vector<WallSpan> planFacade(double length, double baseZ, double topZ,
                                        double bodyDepth, const FacadeParams& params)
{
    std::vector<WallSpan> spans;

    if (length < GRID || params.wallThickness < 1)
    {
        return spans;
    }

    double back = -params.wallThickness;
    double bodyTop = baseZ;

    for (const Band& band : buildBands(params, baseZ, topZ))
    {
        double front = band.frontOffset;

        if (band.kind == BAND_PLINTH)
        {
            spans.push_back({0, length, band.z0, band.z1, back, front, SPAN_PLINTH});
            continue;
        }

        if (band.kind == BAND_PARAPET)
        {
            spans.push_back({0, length, band.z0, band.z1, back, front, SPAN_PARAPET});
            continue;
        }

        if (band.kind == BAND_CORNICE)
        {
            int steps = std::max(1, std::min(4, params.corniceSteps));
            double stepHeight = (band.z1 - band.z0) / steps;

            for (int step = 0; step < steps; ++step)
            {
                double proud =
                    std::max(0.0, params.corniceProud - (steps - 1 - step) * GRID);

                spans.push_back({0, length, band.z0 + step * stepHeight,
                                 band.z0 + (step + 1) * stepHeight, back, front + proud,
                                 SPAN_CORNICE});
            }
            continue;
        }

        bodyTop = band.z1;

        const BandStyle& style = (band.kind == BAND_GROUND) ? params.ground
                               : (band.kind == BAND_TOP) ? params.top
                               : params.upper;

        bool door = (band.kind == BAND_GROUND) && params.groundDoor;
        bool columns = params.arcadeColumns && (band.kind == BAND_GROUND) &&
                       params.upper.frontOffset > front + 0.5;
        bool hasCourse = (band.kind == BAND_UPPER || band.kind == BAND_TOP) &&
                         params.courseHeight > 0.5 && params.courseProud > 0.5;

        auto openings = layoutOpenings(length, band.z0, style, params.minEndPier,
                                       door, params.doorWidth, params.doorHeight);

        for (const WallSpan& span :
             planWallBand(length, band, openings, params, columns, hasCourse))
        {
            spans.push_back(span);
        }
    }

    if (params.solidBody && bodyDepth > 0.5 && bodyTop > baseZ + 0.5)
    {
        spans.push_back({0, length, baseZ, bodyTop, back - bodyDepth, back, SPAN_BODY});
    }

    return spans;
}

inline std::vector<scene::INodePtr> generateFacade(const FacadePath& path,
                                                   const FacadeParams& params,
                                                   const scene::INodePtr& parent)
{
    std::vector<scene::INodePtr> result;

    auto frames = buildFrames(path);
    double length = pathLength(frames);

    if (length < GRID || params.wallThickness < 1)
    {
        return result;
    }

    double tile = params.tileUnits;

    const std::string& wall = params.wallMaterial;
    const std::string& trim = params.trimMaterial;
    const std::string& reveal = params.revealMaterial;
    const std::string& hidden = params.hiddenMaterial;

    FaceMaterials pierMaterials{wall, hidden, reveal, hidden, hidden};
    FaceMaterials spandrelMaterials{wall, hidden, reveal, reveal, hidden};
    FaceMaterials lintelMaterials{wall, hidden, reveal, hidden, reveal};
    FaceMaterials trimMaterials{trim, hidden, trim, trim, trim};
    FaceMaterials capMaterials{trim, trim, trim, trim, trim};
    FaceMaterials plinthMaterials{trim, hidden, trim, trim, hidden};
    FaceMaterials bodyMaterials{hidden, wall, wall, wall, hidden};

    scene::INodePtr trimParent;

    auto trimTarget = [&]() -> scene::INodePtr
    {
        if (!params.trimAsEntity)
        {
            return parent;
        }

        if (!trimParent)
        {
            trimParent = createTrimEntity();
        }

        return trimParent ? trimParent : parent;
    };

    for (const WallSpan& span :
         planFacade(length, path.baseZ, path.topZ, path.bodyDepth, params))
    {
        const FaceMaterials* materials = &pierMaterials;

        switch (span.role)
        {
        case SPAN_SILL_BAND: materials = &spandrelMaterials; break;
        case SPAN_HEAD_BAND: materials = &lintelMaterials; break;
        case SPAN_SILL_TRIM:
        case SPAN_HEAD_TRIM:
        case SPAN_COURSE:    materials = &trimMaterials; break;
        case SPAN_CORNICE:
        case SPAN_PARAPET:   materials = &capMaterials; break;
        case SPAN_PLINTH:    materials = &plinthMaterials; break;
        case SPAN_BODY:      materials = &bodyMaterials; break;
        default: break;
        }

        emitSpan(frames, span.a0, span.a1, span.z0, span.z1, span.back, span.front,
                 *materials, tile, spanIsTrim(span.role) ? trimTarget() : parent,
                 result);
    }

    return result;
}

inline FacadeParams getPreset(int preset)
{
    FacadeParams params;

    params.wallThickness = 20;
    params.minEndPier = 20;

    switch (preset)
    {
    case PRESET_HONGKONG:
        params.ground.height = 160;
        params.ground.frontOffset = 0;
        params.ground.bayPitch = 160;
        params.ground.openingWidth = 120;
        params.ground.openingHeight = 100;
        params.ground.sillHeight = 20;

        params.upper.height = 120;
        params.upper.frontOffset = 40;
        params.upper.bayPitch = 80;
        params.upper.openingWidth = 40;
        params.upper.openingHeight = 80;
        params.upper.sillHeight = 20;

        params.groundDoor = true;
        params.doorWidth = 60;
        params.doorHeight = 100;
        params.arcadeColumns = true;

        params.trimProud = 0;
        params.courseHeight = 20;
        params.courseProud = 20;
        params.corniceHeight = 0;
        params.corniceProud = 0;
        params.parapetHeight = 60;
        break;

    case PRESET_NEWYORK:
        params.plinthHeight = 40;
        params.plinthProud = 20;

        params.ground.height = 160;
        params.ground.frontOffset = 0;
        params.ground.bayPitch = 120;
        params.ground.openingWidth = 60;
        params.ground.openingHeight = 100;
        params.ground.sillHeight = 40;

        params.upper.height = 120;
        params.upper.frontOffset = 0;
        params.upper.bayPitch = 120;
        params.upper.openingWidth = 60;
        params.upper.openingHeight = 60;
        params.upper.sillHeight = 40;

        params.groundDoor = true;
        params.doorWidth = 60;
        params.doorHeight = 120;
        params.arcadeColumns = false;

        params.hasTopFloor = true;
        params.top.height = 100;
        params.top.bayPitch = 120;
        params.top.openingWidth = 40;
        params.top.openingHeight = 40;
        params.top.sillHeight = 40;

        params.trimProud = 20;
        params.courseHeight = 20;
        params.courseProud = 20;
        params.corniceHeight = 60;
        params.corniceProud = 40;
        params.corniceSteps = 3;
        params.parapetHeight = 40;
        break;

    case PRESET_BRUTALIST:
        params.ground.height = 160;
        params.ground.frontOffset = 0;
        params.ground.bayPitch = 160;
        params.ground.openingWidth = 80;
        params.ground.openingHeight = 100;
        params.ground.sillHeight = 40;

        params.upper.height = 120;
        params.upper.frontOffset = 0;
        params.upper.bayPitch = 120;
        params.upper.openingWidth = 40;
        params.upper.openingHeight = 60;
        params.upper.sillHeight = 40;

        params.groundDoor = true;
        params.doorWidth = 80;
        params.doorHeight = 120;
        params.arcadeColumns = false;

        params.pierProud = 20;
        params.trimProud = 0;
        params.courseHeight = 20;
        params.courseProud = 40;
        params.corniceHeight = 0;
        params.corniceProud = 0;
        params.parapetHeight = 40;
        break;

    case PRESET_WAREHOUSE:
        params.plinthHeight = 40;
        params.plinthProud = 20;

        params.ground.height = 200;
        params.ground.frontOffset = 0;
        params.ground.bayPitch = 160;
        params.ground.openingWidth = 80;
        params.ground.openingHeight = 120;
        params.ground.sillHeight = 40;

        params.upper.height = 160;
        params.upper.frontOffset = 0;
        params.upper.bayPitch = 160;
        params.upper.openingWidth = 80;
        params.upper.openingHeight = 100;
        params.upper.sillHeight = 40;

        params.groundDoor = true;
        params.doorWidth = 120;
        params.doorHeight = 160;
        params.arcadeColumns = false;

        params.hasTopFloor = true;
        params.top.height = 120;
        params.top.bayPitch = 160;
        params.top.openingWidth = 80;
        params.top.openingHeight = 40;
        params.top.sillHeight = 40;

        params.trimProud = 20;
        params.courseHeight = 0;
        params.courseProud = 0;
        params.corniceHeight = 40;
        params.corniceProud = 20;
        params.corniceSteps = 2;
        params.parapetHeight = 40;
        break;

    case PRESET_RIBBON:
        params.ground.height = 160;
        params.ground.frontOffset = 0;
        params.ground.bayPitch = 160;
        params.ground.openingWidth = 120;
        params.ground.openingHeight = 100;
        params.ground.sillHeight = 20;

        params.upper.height = 120;
        params.upper.frontOffset = 0;
        params.upper.bayPitch = 120;
        params.upper.openingWidth = 100;
        params.upper.openingHeight = 60;
        params.upper.sillHeight = 40;

        params.groundDoor = true;
        params.doorWidth = 80;
        params.doorHeight = 120;
        params.arcadeColumns = false;

        params.pierProud = 20;
        params.trimProud = 0;
        params.courseHeight = 0;
        params.courseProud = 0;
        params.corniceHeight = 0;
        params.corniceProud = 0;
        params.parapetHeight = 40;
        break;

    case PRESET_CURTAIN:
        params.ground.height = 160;
        params.ground.frontOffset = 0;
        params.ground.bayPitch = 160;
        params.ground.openingWidth = 120;
        params.ground.openingHeight = 120;
        params.ground.sillHeight = 20;

        params.upper.height = 120;
        params.upper.frontOffset = 0;
        params.upper.bayPitch = 120;
        params.upper.openingWidth = 80;
        params.upper.openingHeight = 60;
        params.upper.sillHeight = 40;

        params.groundDoor = true;
        params.doorWidth = 80;
        params.doorHeight = 120;
        params.arcadeColumns = false;

        params.trimProud = 0;
        params.courseHeight = 40;
        params.courseProud = 20;
        params.corniceHeight = 0;
        params.corniceProud = 0;
        params.parapetHeight = 40;
        break;

    default:
        params.ground.height = 160;
        params.ground.bayPitch = 120;
        params.ground.openingWidth = 60;
        params.ground.openingHeight = 100;
        params.ground.sillHeight = 40;

        params.upper.height = 120;
        params.upper.bayPitch = 120;
        params.upper.openingWidth = 60;
        params.upper.openingHeight = 80;
        params.upper.sillHeight = 20;

        params.groundDoor = false;
        params.arcadeColumns = false;
        params.trimProud = 0;
        params.courseHeight = 0;
        params.courseProud = 0;
        params.corniceHeight = 0;
        params.corniceProud = 0;
        params.parapetHeight = 0;
        break;
    }

    return params;
}

inline FacadePath pathFromBounds(const AABB& bounds, int front, double wallThickness)
{
    Vector3 mins = bounds.origin - bounds.extents;
    Vector3 maxs = bounds.origin + bounds.extents;

    FacadePath path;
    path.baseZ = mins.z();
    path.topZ = maxs.z();

    switch (front)
    {
    case FRONT_YPOS:
        path.points.push_back(Vector3(mins.x(), maxs.y(), 0));
        path.points.push_back(Vector3(maxs.x(), maxs.y(), 0));
        path.bodyDepth = (maxs.y() - mins.y()) - wallThickness;
        break;

    case FRONT_YNEG:
        path.points.push_back(Vector3(maxs.x(), mins.y(), 0));
        path.points.push_back(Vector3(mins.x(), mins.y(), 0));
        path.bodyDepth = (maxs.y() - mins.y()) - wallThickness;
        break;

    case FRONT_XPOS:
        path.points.push_back(Vector3(maxs.x(), maxs.y(), 0));
        path.points.push_back(Vector3(maxs.x(), mins.y(), 0));
        path.bodyDepth = (maxs.x() - mins.x()) - wallThickness;
        break;

    default:
        path.points.push_back(Vector3(mins.x(), mins.y(), 0));
        path.points.push_back(Vector3(mins.x(), maxs.y(), 0));
        path.bodyDepth = (maxs.x() - mins.x()) - wallThickness;
        break;
    }

    return path;
}

inline bool pathFromPatch(IPatch& source, FacadePath& path)
{
    PatchMesh mesh = source.getTesselatedPatchMesh();

    if (mesh.width < 2 || mesh.height < 2)
    {
        return false;
    }

    auto at = [&](std::size_t row, std::size_t col) -> const VertexNT& {
        return mesh.vertices[row * mesh.width + col];
    };

    AABB bounds;
    Vector3 averageNormal(0, 0, 0);

    for (const VertexNT& vertex : mesh.vertices)
    {
        bounds.includePoint(vertex.vertex);
        averageNormal += vertex.normal;
    }

    path.baseZ = bounds.origin.z() - bounds.extents.z();
    path.topZ = bounds.origin.z() + bounds.extents.z();
    path.bodyDepth = 0;
    path.points.clear();

    double rowSpan = std::abs(at(mesh.height - 1, 0).vertex.z() - at(0, 0).vertex.z());
    double colSpan = std::abs(at(0, mesh.width - 1).vertex.z() - at(0, 0).vertex.z());

    if (rowSpan >= colSpan)
    {
        std::size_t row = at(0, 0).vertex.z() <= at(mesh.height - 1, 0).vertex.z()
                              ? 0 : mesh.height - 1;

        for (std::size_t col = 0; col < mesh.width; ++col)
        {
            path.points.push_back(at(row, col).vertex);
        }
    }
    else
    {
        std::size_t col = at(0, 0).vertex.z() <= at(0, mesh.width - 1).vertex.z()
                              ? 0 : mesh.width - 1;

        for (std::size_t row = 0; row < mesh.height; ++row)
        {
            path.points.push_back(at(row, col).vertex);
        }
    }

    for (Vector3& point : path.points)
    {
        point.z() = 0;
    }

    if (path.points.size() < 2)
    {
        return false;
    }

    auto frames = buildFrames(path);

    if (frames.empty())
    {
        return false;
    }

    averageNormal.z() = 0;

    if (averageNormal.getLength() > 0.01 && averageNormal.dot(frames.front().normal) < 0)
    {
        std::reverse(path.points.begin(), path.points.end());
    }

    return true;
}

} // namespace facade
