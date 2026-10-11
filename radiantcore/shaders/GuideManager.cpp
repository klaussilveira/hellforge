#include "GuideManager.h"

#include <cctype>
#include <iterator>
#include <sstream>
#include "ideclmanager.h"
#include "ifilesystem.h"
#include "itextstream.h"
#include "parser/DefBlockSyntaxParser.h"
#include "string/replace.h"
#include "string/trim.h"

namespace shaders
{

std::string GuideManager::parseTemplateName(const std::string& token)
{
    auto parenPos = token.find('(');
    return string::trim_copy(parenPos != std::string::npos ? token.substr(0, parenPos) : token);
}

std::vector<std::string> GuideManager::parseCommaSeparated(const std::string& token, bool stripQuotes)
{
    std::vector<std::string> result;
    auto open = token.find('(');
    auto close = token.rfind(')');
    if (open == std::string::npos || close == std::string::npos || close <= open) return result;

    std::string inner = token.substr(open + 1, close - open - 1);
    std::string current;
    bool inQuote = false;

    for (char c : inner)
    {
        if (stripQuotes && c == '"')
        {
            inQuote = !inQuote;
            continue;
        }
        if (c == ',' && !inQuote)
        {
            current.erase(0, current.find_first_not_of(" \t"));
            current.erase(current.find_last_not_of(" \t") + 1);
            if (!current.empty()) result.push_back(current);
            current.clear();
        }
        else
        {
            current += c;
        }
    }

    current.erase(0, current.find_first_not_of(" \t"));
    current.erase(current.find_last_not_of(" \t") + 1);
    if (!current.empty()) result.push_back(current);

    return result;
}

void GuideManager::parseGuideFile(std::istream& stream)
{
    std::string content((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

    // Guide template headers like "guide generic_typeshader(TextureParm, TypeParm )"
    // contain spaces that would split the parameter list across multiple tokens.
    // The DefBlockSyntaxParser expects at most 2 tokens before '{', so we need to
    // collapse each "guide name(params)" header into exactly 2 whitespace-free tokens.
    // Strategy: on lines starting with "guide " or "inlineGuide ", remove all spaces
    // between the first '(' and the matching ')'.
    std::string normalized;
    normalized.reserve(content.size());

    std::istringstream lines(content);
    std::string line;

    while (std::getline(lines, line))
    {
        auto trimPos = line.find_first_not_of(" \t");

        if (trimPos != std::string::npos &&
            (line.compare(trimPos, 6, "guide ") == 0 ||
             line.compare(trimPos, 12, "inlineGuide ") == 0))
        {
            auto openParen = line.find('(');
            auto closeParen = line.rfind(')');

            if (openParen != std::string::npos && closeParen != std::string::npos && closeParen > openParen)
            {
                std::string before = line.substr(0, openParen);
                std::string inside = line.substr(openParen, closeParen - openParen + 1);
                std::string after = line.substr(closeParen + 1);

                // Remove space before '(' if present
                if (!before.empty() && before.back() == ' ')
                    before.pop_back();

                // Remove all spaces/tabs inside the parentheses
                std::string compacted;
                for (char c : inside)
                {
                    if (c != ' ' && c != '\t')
                        compacted += c;
                }

                normalized += before + compacted + after + "\n";
                continue;
            }
        }

        normalized += line + "\n";
    }

    parser::DefBlockSyntaxParser<const std::string> parser(normalized);
    auto syntaxTree = parser.parse();

    for (const auto& node : syntaxTree->getRoot()->getChildren())
    {
        if (node->getType() != parser::DefSyntaxNode::Type::DeclBlock) continue;

        const auto& block = static_cast<const parser::DefBlockSyntax&>(*node);
        const auto& typeSyntax = block.getType();
        const auto& nameSyntax = block.getName();

        if (!typeSyntax || !nameSyntax) continue;

        std::string typeName = typeSyntax->getToken().value;
        if (typeName != "guide" && typeName != "inlineGuide") continue;

        std::string fullName = nameSyntax->getToken().value;
        std::string templateName = parseTemplateName(fullName);
        auto params = parseCommaSeparated(fullName, false);

        GuideTemplate tmpl;
        tmpl.name = templateName;
        tmpl.parameters = std::move(params);
        tmpl.body = block.getBlockContents();

        _templates[templateName] = std::move(tmpl);
    }
}

GuideManager::~GuideManager()
{
    _reloadConn.disconnect();
}

void GuideManager::connectToReloadSignal()
{
    _reloadConn = GlobalDeclarationManager().signal_DeclsReloading(decl::Type::Material).connect(
        sigc::mem_fun(*this, &GuideManager::loadFromVfs)
    );
}

void GuideManager::loadFromVfs()
{
    _templates.clear();

    GlobalFileSystem().forEachFile("guides/", "guide", [this](const vfs::FileInfo& info)
    {
        auto file = GlobalFileSystem().openTextFile(info.fullPath());
        if (!file) return;

        std::istream stream(&file->getInputStream());
        parseGuideFile(stream);
    }, 8);

    rMessage() << "[GuideManager] Loaded " << _templates.size() << " guide templates" << std::endl;
}

bool GuideManager::instantiate(const std::string& invocation, std::string& body) const
{
    auto it = _templates.find(parseTemplateName(invocation));

    if (it == _templates.end()) return false;

    auto args = parseCommaSeparated(invocation, true);
    body = it->second.body;

    for (std::size_t i = 0; i < it->second.parameters.size() && i < args.size(); ++i)
    {
        body = string::replace_all_copy(body, it->second.parameters[i], args[i]);
    }

    return true;
}

std::string GuideManager::expandGuides(const std::string& input) const
{
    if (_templates.empty()) return input;

    std::string result;
    result.reserve(input.size());

    std::istringstream stream(input);
    std::string line;

    while (std::getline(stream, line))
    {
        std::string trimmed = line;
        auto firstNonSpace = trimmed.find_first_not_of(" \t");

        if (firstNonSpace != std::string::npos &&
            trimmed.compare(firstNonSpace, 11, "inlineGuide") == 0 &&
            firstNonSpace + 11 < trimmed.size() && std::isspace(static_cast<unsigned char>(trimmed[firstNonSpace + 11])))
        {
            std::string body;

            if (instantiate(trimmed.substr(firstNonSpace + 12), body))
            {
                result += body + "\n";
                continue;
            }
        }

        if (firstNonSpace != std::string::npos &&
            trimmed.compare(firstNonSpace, 5, "guide") == 0 &&
            firstNonSpace + 5 < trimmed.size() && std::isspace(static_cast<unsigned char>(trimmed[firstNonSpace + 5])))
        {
            std::string content = trimmed.substr(firstNonSpace + 6);
            content.erase(0, content.find_first_not_of(" \t"));

            auto spacePos = content.find_first_of(" \t");
            if (spacePos != std::string::npos)
            {
                std::string materialName = content.substr(0, spacePos);
                std::string invocation = content.substr(spacePos + 1);
                invocation.erase(0, invocation.find_first_not_of(" \t"));

                std::string body;

                if (instantiate(invocation, body))
                {
                    result += materialName + "\n{\n" + body + "\n}\n";
                    continue;
                }
            }
        }

        result += line + "\n";
    }

    return result;
}

}
