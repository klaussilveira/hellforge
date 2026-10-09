#pragma once

#include "HotspotSheet.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace hotspot
{

constexpr double DefaultDetectConsistency = 0.9;

namespace detail
{

struct SeamRun
{
    std::size_t start;
    std::size_t end;
    double strength;
    bool skip;
};

inline std::optional<std::pair<double, std::size_t>> findSeam(const std::vector<double>& fraction,
    std::size_t lineLength, double consistency, std::size_t minSize, std::size_t gap)
{
    auto count = fraction.size();
    double threshold = 1.0 - (1.0 - consistency) * std::min(1.0, lineLength / 64.0);

    std::vector<SeamRun> runs;

    for (std::size_t i = 0; i < count;)
    {
        auto value = std::max(fraction[i], 1.0 - fraction[i]);

        if (value < threshold)
        {
            ++i;
            continue;
        }

        bool dark = fraction[i] >= 0.5;
        auto start = i;
        double sum = 0;

        while (i < count && std::max(fraction[i], 1.0 - fraction[i]) >= threshold && (fraction[i] >= 0.5) == dark)
        {
            sum += std::max(fraction[i], 1.0 - fraction[i]);
            ++i;
        }

        runs.push_back({ start, i, sum / (i - start), false });
    }

    for (std::size_t i = 0; i < runs.size(); ++i)
    {
        if (runs[i].start == 0 || (i > 0 && runs[i - 1].skip && runs[i].start - runs[i - 1].end <= gap))
        {
            runs[i].skip = true;
        }
    }

    for (std::size_t i = runs.size(); i-- > 0;)
    {
        if (runs[i].end == count || (i + 1 < runs.size() && runs[i + 1].skip && runs[i + 1].start - runs[i].end <= gap))
        {
            runs[i].skip = true;
        }
    }

    std::optional<std::pair<double, std::size_t>> best;

    for (const auto& run : runs)
    {
        if (run.skip)
        {
            continue;
        }

        auto middle = (run.start + run.end) / 2;

        if (middle < minSize || count - middle < minSize)
        {
            continue;
        }

        if (!best || run.strength > best->first)
        {
            best = std::make_pair(run.strength, middle);
        }
    }

    return best;
}

}

inline std::vector<Rect> detectRects(const uint8_t* rgba, std::size_t fullWidth, std::size_t fullHeight,
    double consistency = DefaultDetectConsistency)
{
    std::vector<Rect> rects;

    if (fullWidth == 0 || fullHeight == 0)
    {
        return rects;
    }

    std::size_t factor = 1;

    while (std::max(fullWidth, fullHeight) / factor > 512)
    {
        factor *= 2;
    }

    auto width = fullWidth / factor;
    auto height = fullHeight / factor;

    std::vector<double> image(width * height, 0.0);

    for (std::size_t y = 0; y < height * factor; ++y)
    {
        for (std::size_t x = 0; x < width * factor; ++x)
        {
            const auto* pixel = rgba + (y * fullWidth + x) * 4;
            image[(y / factor) * width + x / factor] += 0.299 * pixel[0] + 0.587 * pixel[1] + 0.114 * pixel[2];
        }
    }

    auto radius = std::max<std::size_t>(4, std::min(width, height) / 32);

    std::vector<double> integral((width + 1) * (height + 1), 0.0);

    for (std::size_t y = 0; y < height; ++y)
    {
        double rowSum = 0;

        for (std::size_t x = 0; x < width; ++x)
        {
            rowSum += image[y * width + x];
            integral[(y + 1) * (width + 1) + x + 1] = integral[y * (width + 1) + x + 1] + rowSum;
        }
    }

    std::vector<int> columnPrefix((height + 1) * width, 0);
    std::vector<int> rowPrefix(height * (width + 1), 0);

    for (std::size_t y = 0; y < height; ++y)
    {
        auto y0 = y >= radius ? y - radius : 0;
        auto y1 = std::min(height, y + radius + 1);

        for (std::size_t x = 0; x < width; ++x)
        {
            auto x0 = x >= radius ? x - radius : 0;
            auto x1 = std::min(width, x + radius + 1);

            double sum = integral[y1 * (width + 1) + x1] - integral[y0 * (width + 1) + x1] -
                integral[y1 * (width + 1) + x0] + integral[y0 * (width + 1) + x0];
            double blurred = sum / static_cast<double>((y1 - y0) * (x1 - x0));

            int darker = image[y * width + x] < blurred ? 1 : 0;

            columnPrefix[(y + 1) * width + x] = columnPrefix[y * width + x] + darker;
            rowPrefix[y * (width + 1) + x + 1] = rowPrefix[y * (width + 1) + x] + darker;
        }
    }

    auto minSize = std::max<std::size_t>(4, std::min(width, height) / 64);
    auto gap = std::max<std::size_t>(2, radius / 2);

    std::function<void(std::size_t, std::size_t, std::size_t, std::size_t)> split =
        [&](std::size_t x0, std::size_t y0, std::size_t x1, std::size_t y1)
    {
        std::vector<double> columns(x1 - x0);
        std::vector<double> rows(y1 - y0);

        for (auto x = x0; x < x1; ++x)
        {
            columns[x - x0] = static_cast<double>(columnPrefix[y1 * width + x] - columnPrefix[y0 * width + x]) / (y1 - y0);
        }

        for (auto y = y0; y < y1; ++y)
        {
            rows[y - y0] = static_cast<double>(rowPrefix[y * (width + 1) + x1] - rowPrefix[y * (width + 1) + x0]) / (x1 - x0);
        }

        auto column = detail::findSeam(columns, y1 - y0, consistency, minSize, gap);
        auto row = detail::findSeam(rows, x1 - x0, consistency, minSize, gap);

        if (column && (!row || column->first >= row->first))
        {
            split(x0, y0, x0 + column->second, y1);
            split(x0 + column->second, y0, x1, y1);
        }
        else if (row)
        {
            split(x0, y0, x1, y0 + row->second);
            split(x0, y0 + row->second, x1, y1);
        }
        else
        {
            Rect rect;
            rect.minX = static_cast<int>(x0 * factor);
            rect.minY = static_cast<int>(y0 * factor);
            rect.maxX = static_cast<int>(x1 == width ? fullWidth : x1 * factor);
            rect.maxY = static_cast<int>(y1 == height ? fullHeight : y1 * factor);
            rects.push_back(rect);
        }
    };

    split(0, 0, width, height);

    return rects;
}

inline std::vector<Rect> snapRects(const std::vector<Rect>& rects, int grid, int width, int height)
{
    std::vector<Rect> snapped;

    if (grid <= 1)
    {
        return rects;
    }

    auto snap = [&](int value, int limit)
    {
        return std::clamp(static_cast<int>(std::lround(static_cast<double>(value) / grid)) * grid, 0, limit);
    };

    for (auto rect : rects)
    {
        rect.minX = snap(rect.minX, width);
        rect.minY = snap(rect.minY, height);
        rect.maxX = rect.maxX >= width ? width : snap(rect.maxX, width);
        rect.maxY = rect.maxY >= height ? height : snap(rect.maxY, height);

        if (rect.width() > 0 && rect.height() > 0)
        {
            snapped.push_back(rect);
        }
    }

    return snapped;
}

}
