#include "DeclarationFolderParser.h"

#include <iterator>
#include "DeclarationManager.h"
#include "parser/DefBlockSyntaxParser.h"
#include "string/trim.h"

namespace decl
{

namespace
{
    DeclarationBlockSource createBlock(const parser::DefBlockSyntax& block,
        const vfs::FileInfo& fileInfo, const std::string& modName)
    {
        DeclarationBlockSource syntax;

        const auto& nameSyntax = block.getName();
        const auto& typeSyntax = block.getType();

        syntax.typeName = typeSyntax ? typeSyntax->getToken().value : "";
        syntax.name = nameSyntax ? nameSyntax->getToken().value : "";
        syntax.contents = block.getBlockContents();
        syntax.modName = modName;
        syntax.fileInfo = fileInfo;

        return syntax;
    }
}

DeclarationFolderParser::DeclarationFolderParser(DeclarationManager& owner, Type declType,
    const std::string& baseDir, const std::string& extension,
    const std::map<std::string, Type, string::ILess>& typeMapping,
    IDeclarationManager::DeclFilePreprocessor preprocessor) :
    ThreadedDeclParser<void>(declType, baseDir, extension, 8),
    _owner(owner),
    _typeMapping(typeMapping),
    _defaultDeclType(declType),
    _preprocessor(std::move(preprocessor))
{}

void DeclarationFolderParser::parse(std::istream& stream, const vfs::FileInfo& fileInfo, const std::string& modDir)
{
    std::shared_ptr<parser::DefSyntaxTree> syntaxTree;

    if (_preprocessor)
    {
        std::string content((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        content = _preprocessor(content);

        parser::DefBlockSyntaxParser<const std::string> parser(content);
        syntaxTree = parser.parse();
    }
    else
    {
        parser::DefBlockSyntaxParser<std::istream> parser(stream);
        syntaxTree = parser.parse();
    }

    for (const auto& node : syntaxTree->getRoot()->getChildren())
    {
        if (node->getType() != parser::DefSyntaxNode::Type::DeclBlock)
        {
            continue;
        }

        const auto& blockNode = static_cast<const parser::DefBlockSyntax&>(*node);

        // Convert the incoming block to a DeclarationBlockSource
        auto blockSyntax = createBlock(blockNode, fileInfo, modDir);

        // Move the block in the correct bucket
        auto declType = determineBlockType(blockSyntax);
        auto& blockList = _parsedBlocks.try_emplace(declType).first->second;
        blockList.emplace_back(std::move(blockSyntax));
    }
}

void DeclarationFolderParser::onFinishParsing()
{
    // Submit all parsed declarations to the decl manager
    _owner.onParserFinished(_defaultDeclType, _parsedBlocks);
}

Type DeclarationFolderParser::determineBlockType(const DeclarationBlockSource& block)
{
    if (block.typeName.empty())
    {
        return _defaultDeclType;
    }

    auto foundType = _typeMapping.find(block.typeName);

    return foundType != _typeMapping.end() ? foundType->second : Type::Undetermined;
}

}
