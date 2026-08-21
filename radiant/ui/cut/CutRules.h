#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace cut
{

enum RuleType
{
    RULE_EQUAL_PARTS = 0,
    RULE_SPACING = 1,
    RULE_PATTERN = 2,
};

enum AnchorType
{
    ANCHOR_BRUSH_MIN = 0,
    ANCHOR_BRUSH_CENTER = 1,
    ANCHOR_WORLD_ORIGIN = 2,
};

const double POSITION_EPSILON = 0.001;
const std::size_t MAX_CUTS = 256;
const int MIN_PARTS = 2;
const std::size_t MAX_PARTS = MAX_CUTS + 1;
const std::size_t SUPPRESSED_ENDPOINTS = 2;

struct CutRule
{
    int type = RULE_EQUAL_PARTS;
    int parts = 2;
    double step = 64;
    int subdivisions = 0;
    double offset = 0;
    int anchor = ANCHOR_BRUSH_MIN;
    std::vector<double> pattern;
};

inline double anchorBase(int anchor, double min, double max)
{
    if (anchor == ANCHOR_BRUSH_CENTER)
    {
        return (min + max) * 0.5;
    }

    if (anchor == ANCHOR_WORLD_ORIGIN)
    {
        return 0.0;
    }

    return min;
}

inline void appendPosition(std::vector<double>& positions, double value, double min, double max)
{
    if (value <= min + POSITION_EPSILON || value >= max - POSITION_EPSILON)
    {
        return;
    }

    positions.push_back(value);
}

inline void finalisePositions(std::vector<double>& positions)
{
    std::sort(positions.begin(), positions.end());

    auto duplicates = std::unique(positions.begin(), positions.end(),
        [](double a, double b) { return std::abs(a - b) <= POSITION_EPSILON; });

    positions.erase(duplicates, positions.end());
}

inline std::vector<double> computeCutPositions(const CutRule& rule, double min, double max)
{
    std::vector<double> positions;

    if (max - min <= POSITION_EPSILON)
    {
        return positions;
    }

    double base = anchorBase(rule.anchor, min, max);

    if (rule.type == RULE_EQUAL_PARTS)
    {
        if (rule.parts < MIN_PARTS || static_cast<std::size_t>(rule.parts) > MAX_PARTS)
        {
            return positions;
        }

        double span = (max - min) / rule.parts;

        for (int i = 1; i < rule.parts; ++i)
        {
            appendPosition(positions, min + span * i, min, max);
        }
    }
    else if (rule.type == RULE_SPACING)
    {
        int subdivisions = rule.subdivisions > 0 ? rule.subdivisions : 0;
        double step = rule.step / (subdivisions + 1);

        if (step <= POSITION_EPSILON)
        {
            return positions;
        }

        double start = base + rule.offset;
        double first = std::ceil((min - start) / step);
        double last = std::floor((max - start) / step);

        if (last - first + 1 > static_cast<double>(MAX_CUTS + SUPPRESSED_ENDPOINTS))
        {
            return positions;
        }

        for (double i = first; i <= last; i += 1.0)
        {
            appendPosition(positions, start + i * step, min, max);
        }
    }
    else if (rule.type == RULE_PATTERN)
    {
        double period = 0;

        for (double length : rule.pattern)
        {
            if (length <= POSITION_EPSILON)
            {
                return positions;
            }

            period += length;
        }

        if (period <= POSITION_EPSILON)
        {
            return positions;
        }

        double cursor = base + rule.offset;
        cursor -= std::ceil((cursor - min) / period) * period;

        while (cursor < max && positions.size() <= MAX_CUTS)
        {
            for (double length : rule.pattern)
            {
                cursor += length;
                appendPosition(positions, cursor, min, max);
            }
        }
    }

    finalisePositions(positions);

    if (positions.size() > MAX_CUTS)
    {
        positions.clear();
    }

    return positions;
}

} // namespace cut
