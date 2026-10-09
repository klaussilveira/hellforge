#pragma once

#include "parser/DefTokeniser.h"
#include "string/case_conv.h"
#include "string/convert.h"

#include <fmt/format.h>

#include <sstream>
#include <string>
#include <vector>

namespace hotspot
{

struct Rect
{
    int minX = 0;
    int minY = 0;
    int maxX = 0;
    int maxY = 0;
    bool rotate = false;
    bool reflect = false;
    bool alt = false;

    int width() const { return maxX - minX; }
    int height() const { return maxY - minY; }
};

struct Sheet
{
    int width = 0;
    int height = 0;
    double scale = 0;
    double inset = 0;
    std::vector<Rect> rects;
};

namespace detail
{

inline void parsePair(const std::string& value, int& a, int& b)
{
    std::istringstream stream(value);
    stream >> a >> b;
}

inline bool isValid(const Rect& rect)
{
    return rect.width() > 0 && rect.height() > 0;
}

}

inline Sheet parseText(const std::string& text)
{
    Sheet sheet;
    parser::BasicDefTokeniser<std::string> tok(text);

    tok.nextToken();
    tok.assertNextToken("{");

    while (tok.hasMoreTokens())
    {
        auto key = string::to_lower_copy(tok.nextToken());

        if (key == "}")
        {
            break;
        }

        if (key == "rectangle")
        {
            tok.assertNextToken("{");

            Rect rect;

            for (auto field = string::to_lower_copy(tok.nextToken()); field != "}";
                 field = string::to_lower_copy(tok.nextToken()))
            {
                auto value = tok.nextToken();

                if (field == "min") detail::parsePair(value, rect.minX, rect.minY);
                else if (field == "max") detail::parsePair(value, rect.maxX, rect.maxY);
                else if (field == "rotate") rect.rotate = string::convert<int>(value) != 0;
                else if (field == "reflect") rect.reflect = string::convert<int>(value) != 0;
                else if (field == "alt") rect.alt = string::convert<int>(value) != 0;
            }

            if (detail::isValid(rect))
            {
                sheet.rects.push_back(rect);
            }

            continue;
        }

        auto value = tok.nextToken();

        if (key == "resolution") detail::parsePair(value, sheet.width, sheet.height);
        else if (key == "scale") sheet.scale = string::convert<double>(value);
        else if (key == "inset") sheet.inset = string::convert<double>(value);
    }

    return sheet;
}

inline Sheet parseBinary(const std::string& data)
{
    Sheet sheet;

    if (data.size() < 4 || data[0] != 1)
    {
        return sheet;
    }

    auto byte = [&](std::size_t offset) { return static_cast<unsigned char>(data[offset]); };
    auto u16 = [&](std::size_t offset) { return static_cast<int>(byte(offset) | (byte(offset + 1) << 8)); };

    auto count = static_cast<std::size_t>(u16(2));

    for (std::size_t i = 0, offset = 4; i < count && offset + 9 <= data.size(); ++i, offset += 9)
    {
        Rect rect;
        auto flags = byte(offset);

        rect.rotate = (flags & 0x1) != 0;
        rect.reflect = (flags & 0x2) != 0;
        rect.alt = (flags & 0x4) != 0;
        rect.minX = u16(offset + 1);
        rect.minY = u16(offset + 3);
        rect.maxX = u16(offset + 5);
        rect.maxY = u16(offset + 7);

        if (detail::isValid(rect))
        {
            sheet.rects.push_back(rect);
        }
    }

    return sheet;
}

inline Sheet parse(const std::string& data)
{
    if (!data.empty() && data[0] == 1)
    {
        return parseBinary(data);
    }

    return parseText(data);
}

inline std::string writeText(const Sheet& sheet)
{
    std::string out = "Rectangles\n{\n";

    if (sheet.width > 0 && sheet.height > 0)
    {
        out += fmt::format("\tresolution\t\"{0} {1}\"\n", sheet.width, sheet.height);
    }

    if (sheet.scale > 0)
    {
        out += fmt::format("\tscale\t\"{0}\"\n", sheet.scale);
    }

    if (sheet.inset > 0)
    {
        out += fmt::format("\tinset\t\"{0}\"\n", sheet.inset);
    }

    for (const auto& rect : sheet.rects)
    {
        out += "\n\trectangle\n\t{\n";
        out += fmt::format("\t\tmin\t\"{0} {1}\"\n", rect.minX, rect.minY);
        out += fmt::format("\t\tmax\t\"{0} {1}\"\n", rect.maxX, rect.maxY);

        if (rect.rotate) out += "\t\trotate\t1\n";
        if (rect.reflect) out += "\t\treflect\t1\n";
        if (rect.alt) out += "\t\talt\t1\n";

        out += "\t}\n";
    }

    out += "}\n";

    return out;
}

}
