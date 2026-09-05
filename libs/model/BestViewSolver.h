#pragma once

#include "imodel.h"
#include "imodelsurface.h"

#include "math/Vector3.h"
#include "math/pi.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace model
{

constexpr double DefaultViewElevation = 34;
constexpr double DefaultViewAzimuth = 45;

namespace detail
{

constexpr double SurfaceBonus = 0.05;
constexpr double DeviationThreshold = 1.15;
constexpr double CandidateAzimuthStep = 22.5;
constexpr double CandidateElevations[] = { 15, 30, 45, 70 };

struct ViewTriangle
{
    Vector3 normal;
    double area;
    int surface;
};

inline std::vector<ViewTriangle> collectViewTriangles(const IModel& model)
{
    std::vector<ViewTriangle> triangles;

    for (int surfaceNum = 0; surfaceNum < model.getSurfaceCount(); ++surfaceNum)
    {
        const auto& surface = model.getSurface(static_cast<unsigned>(surfaceNum));

        for (int polygonNum = 0; polygonNum < surface.getNumTriangles(); ++polygonNum)
        {
            auto polygon = surface.getPolygon(polygonNum);

            auto edges = (polygon.b.vertex - polygon.a.vertex).cross(
                polygon.c.vertex - polygon.a.vertex);
            auto normal = polygon.a.normal + polygon.b.normal + polygon.c.normal;

            double area = edges.getLength() * 0.5;
            double normalLength = normal.getLength();

            if (area <= 0 || normalLength <= 0) continue;

            triangles.push_back({ normal / normalLength, area, surfaceNum });
        }
    }

    return triangles;
}

struct ViewScore
{
    double area = 0;
    double score = 0;
};

inline ViewScore scoreDirection(const std::vector<ViewTriangle>& triangles, int surfaceCount,
    const Vector3& direction)
{
    double area = 0;
    std::vector<bool> surfaceVisible(surfaceCount, false);

    for (const auto& triangle : triangles)
    {
        double facing = triangle.normal.dot(direction);

        if (facing <= 0) continue;

        area += triangle.area * facing;
        surfaceVisible[triangle.surface] = true;
    }

    auto visibleSurfaces = std::count(surfaceVisible.begin(), surfaceVisible.end(), true);

    return { area, area * (1 + SurfaceBonus * visibleSurfaces) };
}

}

inline Vector3 getViewDirection(double elevation, double azimuth)
{
    double e = degrees_to_radians(elevation);
    double a = degrees_to_radians(azimuth);

    return Vector3(cos(a) * cos(e), sin(a) * cos(e), sin(e));
}

inline Vector3 getViewDirection(const Vector3& viewAngles)
{
    return getViewDirection(viewAngles[0], -(viewAngles[1] + 180));
}

inline Vector3 getViewAngles(double elevation, double azimuth)
{
    double yaw = std::fmod(-(azimuth + 180), 360.0);

    if (yaw < 0)
    {
        yaw += 360;
    }

    return Vector3(elevation, yaw, 0);
}

inline Vector3 getDefaultViewAngles()
{
    return getViewAngles(DefaultViewElevation, DefaultViewAzimuth);
}

inline Vector3 calculateBestViewAngles(const IModel& model)
{
    auto defaultAngles = getDefaultViewAngles();
    auto triangles = detail::collectViewTriangles(model);

    if (triangles.empty()) return defaultAngles;

    int surfaceCount = model.getSurfaceCount();

    auto defaultScore = detail::scoreDirection(triangles, surfaceCount,
        getViewDirection(defaultAngles));

    Vector3 bestAngles = defaultAngles;
    detail::ViewScore best;

    for (double elevation : detail::CandidateElevations)
    {
        for (double azimuth = 0; azimuth < 360; azimuth += detail::CandidateAzimuthStep)
        {
            auto candidate = detail::scoreDirection(triangles, surfaceCount,
                getViewDirection(elevation, azimuth));

            if (candidate.score > best.score)
            {
                best = candidate;
                bestAngles = getViewAngles(elevation, azimuth);
            }
        }
    }

    if (best.area <= 0 || best.area < defaultScore.area * detail::DeviationThreshold)
    {
        return defaultAngles;
    }

    return bestAngles;
}

inline double calculateVisibleArea(const IModel& model, const Vector3& direction)
{
    double area = 0;

    for (const auto& triangle : detail::collectViewTriangles(model))
    {
        double facing = triangle.normal.dot(direction);

        if (facing > 0)
        {
            area += triangle.area * facing;
        }
    }

    return area;
}

}
