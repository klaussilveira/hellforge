#pragma once

#include "HotspotSheet.h"
#include "math/Vector2.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <random>
#include <vector>

namespace hotspot
{

struct Placement
{
    Vector2 topLeft;
    Vector2 topRight;
    Vector2 bottomLeft;
    std::size_t rect = 0;
};

constexpr double TieTolerance = 0.02;
constexpr double RotationPenalty = 0.05;

inline bool spansWidth(const Sheet& sheet, const Rect& rect)
{
    return rect.minX <= 0 && rect.maxX >= sheet.width;
}

inline bool spansHeight(const Sheet& sheet, const Rect& rect)
{
    return rect.minY <= 0 && rect.maxY >= sheet.height;
}

inline void getBounds(const Placement& placement, Vector2& mins, Vector2& maxs)
{
    auto bottomRight = placement.topRight + placement.bottomLeft - placement.topLeft;

    mins = maxs = placement.topLeft;

    for (const auto& p : { placement.topRight, placement.bottomLeft, bottomRight })
    {
        mins.x() = std::min(mins.x(), p.x());
        mins.y() = std::min(mins.y(), p.y());
        maxs.x() = std::max(maxs.x(), p.x());
        maxs.y() = std::max(maxs.y(), p.y());
    }
}

inline bool sameBounds(const Placement& a, const Placement& b)
{
    Vector2 aMin, aMax, bMin, bMax;
    getBounds(a, aMin, aMax);
    getBounds(b, bMin, bMax);

    const double epsilon = 1e-4;

    return std::abs(aMin.x() - bMin.x()) < epsilon && std::abs(aMin.y() - bMin.y()) < epsilon &&
           std::abs(aMax.x() - bMax.x()) < epsilon && std::abs(aMax.y() - bMax.y()) < epsilon;
}

inline std::vector<Placement> findBestPlacements(const Sheet& sheet, double faceWidth, double faceHeight,
    double unitsPerPixel, bool alt, bool allowRotation = false)
{
    std::vector<std::pair<double, Placement>> scored;
    std::vector<Placement> best;

    if (sheet.width <= 0 || sheet.height <= 0 || faceWidth <= 0 || faceHeight <= 0 || unitsPerPixel <= 0)
    {
        return best;
    }

    bool haveGroup = std::any_of(sheet.rects.begin(), sheet.rects.end(),
        [&](const Rect& rect) { return rect.alt == alt; });

    for (std::size_t i = 0; i < sheet.rects.size(); ++i)
    {
        const auto& rect = sheet.rects[i];

        if (haveGroup && rect.alt != alt)
        {
            continue;
        }

        bool tileU = spansWidth(sheet, rect);
        bool tileV = spansHeight(sheet, rect);
        double width = rect.width() * unitsPerPixel;
        double height = rect.height() * unitsPerPixel;

        for (bool rotated : { false, true })
        {
            if (rotated && !rect.rotate && !allowRotation)
            {
                continue;
            }

            auto repeat = [](bool tiles, double faceSize, double rectSize)
            {
                return tiles ? std::max(1.0, std::round(faceSize / rectSize)) : 1.0;
            };

            double repeatU = rotated ? repeat(tileU, faceHeight, width) : repeat(tileU, faceWidth, width);
            double repeatV = rotated ? repeat(tileV, faceWidth, height) : repeat(tileV, faceHeight, height);

            double extentW = rotated ? height * repeatV : width * repeatU;
            double extentH = rotated ? width * repeatU : height * repeatV;

            double score = 2 * std::abs(std::log((faceWidth / faceHeight) / (extentW / extentH))) +
                std::abs(std::log((faceWidth * faceHeight) / (extentW * extentH))) + (rotated ? RotationPenalty : 0);

            double insetU = tileU ? 0 : sheet.inset / sheet.width;
            double insetV = tileV ? 0 : sheet.inset / sheet.height;

            double u0 = static_cast<double>(rect.minX) / sheet.width + insetU;
            double v0 = static_cast<double>(rect.minY) / sheet.height + insetV;
            double u1 = u0 + (static_cast<double>(rect.width()) / sheet.width - 2 * insetU) * repeatU;
            double v1 = v0 + (static_cast<double>(rect.height()) / sheet.height - 2 * insetV) * repeatV;

            Placement placement;
            placement.rect = i;

            if (rotated)
            {
                placement.topLeft = Vector2(u0, v1);
                placement.topRight = Vector2(u0, v0);
                placement.bottomLeft = Vector2(u1, v1);
            }
            else
            {
                placement.topLeft = Vector2(u0, v0);
                placement.topRight = Vector2(u1, v0);
                placement.bottomLeft = Vector2(u0, v1);
            }

            scored.emplace_back(score, placement);
        }
    }

    if (scored.empty())
    {
        return best;
    }

    auto bestScore = std::min_element(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; })->first;

    for (const auto& [score, placement] : scored)
    {
        if (score <= bestScore + TieTolerance)
        {
            best.push_back(placement);
        }
    }

    return best;
}

inline std::optional<Placement> choosePlacement(const Sheet& sheet, double faceWidth, double faceHeight,
    double unitsPerPixel, bool alt, const std::optional<Placement>& current, std::mt19937& rng,
    bool allowRotation = false)
{
    auto candidates = findBestPlacements(sheet, faceWidth, faceHeight, unitsPerPixel, alt, allowRotation);

    if (candidates.empty())
    {
        return std::nullopt;
    }

    if (current && candidates.size() > 1)
    {
        std::vector<Placement> others;

        for (const auto& candidate : candidates)
        {
            if (!sameBounds(candidate, *current))
            {
                others.push_back(candidate);
            }
        }

        if (!others.empty())
        {
            candidates.swap(others);
        }
    }

    auto placement = candidates[std::uniform_int_distribution<std::size_t>(0, candidates.size() - 1)(rng)];

    if (sheet.rects[placement.rect].reflect && std::uniform_int_distribution<int>(0, 1)(rng) == 1)
    {
        auto across = placement.topRight - placement.topLeft;
        std::swap(placement.topLeft, placement.topRight);
        placement.bottomLeft += across;
    }

    return placement;
}

}
