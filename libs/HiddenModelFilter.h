#pragma once

#include <regex>
#include <string>
#include <vector>

#include "igame.h"
#include "itextstream.h"

namespace game
{

/**
 * \brief Filename patterns the current game keeps out of its model lists.
 *
 * A game can ship generated siblings next to a model that are not models a
 * mapper should ever place: HellCore's asset pipeline writes a convex trace
 * model as <name>_coll.ase and the artist's reduced meshes as <name>_lod1.ase,
 * _lod2.ase and so on, several thousand of them. They are declared under
 * <modelBrowser> in the .game file:
 *
 *     <modelBrowser>
 *         <hidden match="_coll\.ase$" />
 *     </modelBrowser>
 *
 * Games that declare no patterns match nothing, so this costs them one empty
 * vector. Construct once per population pass, not per file: the patterns are
 * compiled in the constructor.
 */
class HiddenModelFilter
{
    std::vector<std::regex> _patterns;

public:
    static constexpr const char* XPATH = "/modelBrowser//hidden";

    HiddenModelFilter() :
        HiddenModelFilter(GlobalGameManager().currentGame()->getLocalXPath(XPATH))
    {}

    explicit HiddenModelFilter(const xml::NodeList& nodes)
    {
        for (const auto& node : nodes)
        {
            auto match = node.getAttributeValue("match");

            if (match.empty())
            {
                continue;
            }

            try
            {
                _patterns.emplace_back(match, std::regex::icase | std::regex::optimize);
            }
            catch (const std::regex_error& ex)
            {
                rWarning() << "HiddenModelFilter: bad pattern '" << match << "': "
                    << ex.what() << std::endl;
            }
        }
    }

    // True if this model file should be kept out of the model lists
    bool isHidden(const std::string& fileName) const
    {
        for (const auto& pattern : _patterns)
        {
            if (std::regex_search(fileName, pattern))
            {
                return true;
            }
        }

        return false;
    }
};

}
