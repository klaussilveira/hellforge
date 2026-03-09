#pragma once

#include "ipatch.h"
#include "scenelib.h"
#include "math/Vector3.h"
#include "math/pi.h"

#include <cmath>
#include <vector>
#include <random>
#include <sstream>

namespace cables
{

struct CableParams
{
    int count = 3;
    float density = 1.0f;
    float spacing = 4.0f;
    float spacingVariation = 0.0f;
    int spacingSeed = 0;
    float radius = 2.0f;
    float randomScale = 1.0f;
    float minScale = 0.5f;
    float maxScale = 1.5f;
    int lengthResolution = 16;
    int circResolution = 8;
    int circResVariation = 3;
    bool capEnds = true;
    std::string material = "_default";

    bool fixedSubdivisions = false;
    int subdivisionU = 4;
    int subdivisionV = 4;

    int simType = 0;
    float gravityMin = 0.1f;
    float gravityMax = 0.5f;
    int gravitySeed = 0;
    float gravityMultiplier = 1.0f;
};

struct Frame
{
    Vector3 position;
    Vector3 tangent;
    Vector3 normal;
    Vector3 binormal;
};

inline Frame buildFrame(const Vector3& position, const Vector3& tangent)
{
    Frame f;
    f.position = position;
    f.tangent = tangent.getNormalised();

    Vector3 up(0, 0, 1);
    if (std::abs(f.tangent.z()) > 0.99)
        up = Vector3(1, 0, 0);

    f.normal = f.tangent.cross(up).getNormalised();
    f.binormal = f.normal.cross(f.tangent).getNormalised();
    return f;
}

inline Vector3 catmullRom(const Vector3& p0, const Vector3& p1,
                          const Vector3& p2, const Vector3& p3, double t)
{
    double t2 = t * t;
    double t3 = t2 * t;
    return 0.5 * ((2.0 * p1) +
                   (-1.0 * p0 + p2) * t +
                   (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 +
                   (-1.0 * p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3);
}

inline std::vector<Vector3> sampleCatmullRomPath(
    const std::vector<Vector3>& controlPoints, int numSamples)
{
    std::vector<Vector3> result;
    if (controlPoints.size() < 2 || numSamples < 2)
        return result;

    std::vector<Vector3> pts;
    pts.push_back(controlPoints.front() * 2.0 - controlPoints[1]);
    for (auto& p : controlPoints)
        pts.push_back(p);
    pts.push_back(controlPoints.back() * 2.0 - controlPoints[controlPoints.size() - 2]);

    int numSegments = static_cast<int>(pts.size()) - 3;
    for (int i = 0; i < numSamples; ++i)
    {
        double globalT = static_cast<double>(i) / (numSamples - 1) * numSegments;
        int seg = std::min(static_cast<int>(globalT), numSegments - 1);
        double localT = globalT - seg;

        result.push_back(catmullRom(pts[seg], pts[seg + 1], pts[seg + 2], pts[seg + 3], localT));
    }
    return result;
}

inline std::vector<Vector3> generatePathBetweenPoints(
    const Vector3& start, const Vector3& end, int numSamples, double sagAmount)
{
    std::vector<Vector3> path;
    for (int i = 0; i < numSamples; ++i)
    {
        double t = static_cast<double>(i) / (numSamples - 1);
        Vector3 pos = start + (end - start) * t;
        double sag = sagAmount * 4.0 * t * (1.0 - t);
        pos.z() -= sag;
        path.push_back(pos);
    }
    return path;
}

inline std::vector<Vector3> parseCurveString(const std::string& value)
{
    std::vector<Vector3> points;
    if (value.empty())
        return points;

    std::istringstream stream(value);
    int count;
    stream >> count;

    char ch;
    while (stream >> ch && ch != '(') {}

    for (int i = 0; i < count; ++i)
    {
        double x, y, z;
        if (stream >> x >> y >> z)
            points.emplace_back(x, y, z);
    }
    return points;
}

inline std::vector<Frame> buildFramesAlongPath(const std::vector<Vector3>& path)
{
    std::vector<Frame> frames;
    if (path.size() < 2)
        return frames;

    Vector3 tangent = (path[1] - path[0]).getNormalised();
    Frame prev = buildFrame(path[0], tangent);
    frames.push_back(prev);

    for (size_t i = 1; i < path.size(); ++i)
    {
        Vector3 t;
        if (i < path.size() - 1)
            t = (path[i + 1] - path[i - 1]).getNormalised();
        else
            t = (path[i] - path[i - 1]).getNormalised();

        Vector3 prevT = prev.tangent;
        Vector3 rotAxis = prevT.cross(t);
        double rotLen = rotAxis.getLength();

        Frame f;
        f.position = path[i];
        f.tangent = t;

        if (rotLen > 1e-8)
        {
            rotAxis /= rotLen;
            double cosA = std::max(-1.0, std::min(1.0, prevT.dot(t)));
            double angle = std::acos(cosA);

            auto rotate = [&](const Vector3& v) -> Vector3 {
                double c = std::cos(angle);
                double s = std::sin(angle);
                return v * c + rotAxis.cross(v) * s + rotAxis * rotAxis.dot(v) * (1.0 - c);
            };

            f.normal = rotate(prev.normal).getNormalised();
            f.binormal = rotate(prev.binormal).getNormalised();
        }
        else
        {
            f.normal = prev.normal;
            f.binormal = prev.binormal;
        }

        frames.push_back(f);
        prev = f;
    }

    return frames;
}

inline std::vector<Vector3> offsetPath(
    const std::vector<Vector3>& path, const std::vector<Frame>& frames,
    double offsetN, double offsetB)
{
    std::vector<Vector3> result;
    for (size_t i = 0; i < path.size(); ++i)
        result.push_back(path[i] + frames[i].normal * offsetN + frames[i].binormal * offsetB);
    return result;
}

inline void generatePatchCable(
    const std::vector<Vector3>& path, double radius, int circRes,
    bool capEnds, const std::string& material,
    bool fixedSubdivisions, int subdivU, int subdivV,
    const scene::INodePtr& parent)
{
    auto frames = buildFramesAlongPath(path);
    if (frames.size() < 2)
        return;

    int rawWidth = circRes + 1;
    int patchWidth = (rawWidth % 2 == 0) ? rawWidth + 1 : rawWidth;
    if (patchWidth < 3) patchWidth = 3;

    int rawHeight = static_cast<int>(frames.size());
    int patchHeight = (rawHeight % 2 == 0) ? rawHeight + 1 : rawHeight;
    if (patchHeight < 3) patchHeight = 3;

    std::vector<Frame> usedFrames;
    if (patchHeight != rawHeight)
    {
        auto resampledPath = sampleCatmullRomPath(path, patchHeight);
        usedFrames = buildFramesAlongPath(resampledPath);
    }
    else
    {
        usedFrames = frames;
    }

    auto patchType = fixedSubdivisions ? patch::PatchDefType::Def3 : patch::PatchDefType::Def2;
    auto patchNode = GlobalPatchModule().createPatch(patchType);
    parent->addChildNode(patchNode);

    auto* patch = Node_getIPatch(patchNode);
    patch->setDims(static_cast<std::size_t>(patchWidth), static_cast<std::size_t>(patchHeight));
    patch->setShader(material);

    if (fixedSubdivisions)
        patch->setFixedSubdivisions(true, Subdivisions(subdivU, subdivV));

    for (int row = 0; row < patchHeight; ++row)
    {
        const Frame& f = usedFrames[row];

        for (int col = 0; col < patchWidth; ++col)
        {
            double angle = -2.0 * math::PI * col / (patchWidth - 1);
            Vector3 offset = f.normal * (radius * std::cos(angle)) +
                             f.binormal * (radius * std::sin(angle));

            patch->ctrlAt(row, col).vertex = f.position + offset;

            double u = static_cast<double>(col) / (patchWidth - 1);
            double v = static_cast<double>(row) / (patchHeight - 1);
            patch->ctrlAt(row, col).texcoord = Vector2(u, v);
        }
    }

    patch->controlPointsChanged();
    Node_setSelected(patchNode, true);

    if (capEnds)
    {
        for (int capIdx = 0; capIdx < 2; ++capIdx)
        {
            const Frame& f = (capIdx == 0) ? usedFrames.front() : usedFrames.back();

            auto capNode = GlobalPatchModule().createPatch(patchType);
            parent->addChildNode(capNode);

            auto* cap = Node_getIPatch(capNode);
            cap->setDims(static_cast<std::size_t>(patchWidth), 3);
            cap->setShader(material);

            if (fixedSubdivisions)
                cap->setFixedSubdivisions(true, Subdivisions(subdivU, subdivV));

            for (int col = 0; col < patchWidth; ++col)
            {
                double angle = -2.0 * math::PI * col / (patchWidth - 1);
                Vector3 rim = f.position + f.normal * (radius * std::cos(angle)) +
                              f.binormal * (radius * std::sin(angle));
                Vector3 mid = f.position + (rim - f.position) * 0.5;

                double u = static_cast<double>(col) / (patchWidth - 1);

                cap->ctrlAt(0, col).vertex = f.position;
                cap->ctrlAt(0, col).texcoord = Vector2(u, 0);
                cap->ctrlAt(1, col).vertex = mid;
                cap->ctrlAt(1, col).texcoord = Vector2(u, 0.5);
                cap->ctrlAt(2, col).vertex = rim;
                cap->ctrlAt(2, col).texcoord = Vector2(u, 1.0);
            }

            cap->controlPointsChanged();
            Node_setSelected(capNode, true);
        }
    }
}

inline void generateCablesBetweenPoints(
    const Vector3& start, const Vector3& end,
    const CableParams& params, const scene::INodePtr& parent)
{
    int numSamples = std::max(2, params.lengthResolution + 1);
    double distance = (end - start).getLength();

    float effectiveSpacing = params.spacing / std::max(0.01f, params.density);

    std::mt19937 rng(params.spacingSeed);
    std::uniform_real_distribution<float> spacingDist(-params.spacingVariation, params.spacingVariation);

    std::mt19937 gravityRng(params.gravitySeed);
    std::uniform_real_distribution<float> gravityDist(params.gravityMin, params.gravityMax);

    std::mt19937 scaleRng(params.spacingSeed + 12345);
    std::uniform_real_distribution<float> scaleDist(params.minScale, params.maxScale);

    std::mt19937 circRng(params.spacingSeed + 67890);
    int minCirc = std::max(3, params.circResolution - params.circResVariation);
    int maxCirc = params.circResolution + params.circResVariation;
    std::uniform_int_distribution<int> circDist(minCirc, maxCirc);

    for (int c = 0; c < params.count; ++c)
    {
        double baseOffset = (c - (params.count - 1) * 0.5) * effectiveSpacing;
        double variation = (params.spacingVariation > 0) ? spacingDist(rng) : 0.0;
        double lateralOffset = baseOffset + variation;

        double sagAmount = 0.0;
        if (params.simType == 1)
        {
            double gravFactor = gravityDist(gravityRng);
            sagAmount = distance * gravFactor * params.gravityMultiplier;
        }

        double scale = 1.0;
        if (params.randomScale > 0)
        {
            scale = scaleDist(scaleRng);
            scale = std::max(static_cast<double>(params.minScale),
                             std::min(static_cast<double>(params.maxScale), scale));
        }
        double cableRadius = params.radius * scale;

        int circRes = params.circResolution;
        if (params.circResVariation > 0)
            circRes = circDist(circRng);

        auto cablePath = generatePathBetweenPoints(start, end, numSamples, sagAmount);
        auto cableFrames = buildFramesAlongPath(cablePath);
        auto finalPath = offsetPath(cablePath, cableFrames, lateralOffset, 0);

        generatePatchCable(finalPath, cableRadius, circRes, params.capEnds,
                           params.material, params.fixedSubdivisions,
                           params.subdivisionU, params.subdivisionV, parent);
    }
}

inline void generateCablesAlongPath(
    const std::vector<Vector3>& controlPoints,
    const CableParams& params, const scene::INodePtr& parent)
{
    int numSamples = std::max(2, params.lengthResolution + 1);
    auto basePath = sampleCatmullRomPath(controlPoints, numSamples);
    if (basePath.size() < 2)
        return;

    auto baseFrames = buildFramesAlongPath(basePath);

    float effectiveSpacing = params.spacing / std::max(0.01f, params.density);

    std::mt19937 rng(params.spacingSeed);
    std::uniform_real_distribution<float> spacingDist(-params.spacingVariation, params.spacingVariation);

    std::mt19937 scaleRng(params.spacingSeed + 12345);
    std::uniform_real_distribution<float> scaleDist(params.minScale, params.maxScale);

    std::mt19937 circRng(params.spacingSeed + 67890);
    int minCirc = std::max(3, params.circResolution - params.circResVariation);
    int maxCirc = params.circResolution + params.circResVariation;
    std::uniform_int_distribution<int> circDist(minCirc, maxCirc);

    for (int c = 0; c < params.count; ++c)
    {
        double baseOffset = (c - (params.count - 1) * 0.5) * effectiveSpacing;
        double variation = (params.spacingVariation > 0) ? spacingDist(rng) : 0.0;
        double lateralOffset = baseOffset + variation;

        double scale = 1.0;
        if (params.randomScale > 0)
        {
            scale = scaleDist(scaleRng);
            scale = std::max(static_cast<double>(params.minScale),
                             std::min(static_cast<double>(params.maxScale), scale));
        }
        double cableRadius = params.radius * scale;

        int circRes = params.circResolution;
        if (params.circResVariation > 0)
            circRes = circDist(circRng);

        auto finalPath = offsetPath(basePath, baseFrames, lateralOffset, 0);

        generatePatchCable(finalPath, cableRadius, circRes, params.capEnds,
                           params.material, params.fixedSubdivisions,
                           params.subdivisionU, params.subdivisionV, parent);
    }
}

} // namespace cables
