#pragma once

#include <map>
#include <string>
#include <vector>

namespace shaders
{

struct GuideTemplate
{
    std::string name;
    std::vector<std::string> parameters;
    std::string body;
};

// Support for guide() from Quake 4
class GuideManager
{
    std::map<std::string, GuideTemplate> _templates;

public:
    void loadFromVfs();
    std::string expandGuides(const std::string& input) const;

private:
    void parseGuideFile(std::istream& stream);

    static std::string parseTemplateName(const std::string& token);
    static std::vector<std::string> parseCommaSeparated(const std::string& token, bool stripQuotes);
};

}
