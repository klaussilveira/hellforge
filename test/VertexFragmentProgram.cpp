#include "gtest/gtest.h"

#include "rendersystem/backend/glprogram/GenericVFPProgram.h"

#include <sstream>

namespace test
{

namespace
{

const char* const TWO_SECTION_VFP =
    "!!ARBvp1.0\n"
    "OPTION ARB_position_invariant;\n"
    "MOV result.color, vertex.color;\n"
    "END\n"
    "\n"
    "!!ARBfp1.0\n"
    "MOV weight, fragment.color;\n"
    "END\n";

std::string extract(const std::string& source, const std::string& marker)
{
    std::stringstream stream(source);
    return render::GenericVFPProgram::extractProgramSection(stream, marker);
}

}

TEST(VertexFragmentProgram, ExtractsVertexSection)
{
    auto section = extract(TWO_SECTION_VFP, render::GenericVFPProgram::VERTEX_MARKER);

    EXPECT_NE(section.find("OPTION ARB_position_invariant;"), std::string::npos);
    EXPECT_NE(section.find("MOV result.color, vertex.color;"), std::string::npos);
    EXPECT_EQ(section.find("fragment.color"), std::string::npos) << "Fragment half leaked into the vertex section";
}

TEST(VertexFragmentProgram, ExtractsFragmentSection)
{
    auto section = extract(TWO_SECTION_VFP, render::GenericVFPProgram::FRAGMENT_MARKER);

    EXPECT_NE(section.find("MOV weight, fragment.color;"), std::string::npos);
    EXPECT_EQ(section.find("vertex.color"), std::string::npos) << "Vertex half leaked into the fragment section";
}

TEST(VertexFragmentProgram, SectionStartsWithItsOwnMarker)
{
    auto vertex = extract(TWO_SECTION_VFP, render::GenericVFPProgram::VERTEX_MARKER);
    auto fragment = extract(TWO_SECTION_VFP, render::GenericVFPProgram::FRAGMENT_MARKER);

    EXPECT_EQ(vertex.rfind(render::GenericVFPProgram::VERTEX_MARKER, 0), 0u);
    EXPECT_EQ(fragment.rfind(render::GenericVFPProgram::FRAGMENT_MARKER, 0), 0u);
}

TEST(VertexFragmentProgram, ReturnsEmptyWhenSectionIsMissing)
{
    std::string vertexOnly = "!!ARBvp1.0\nEND\n";

    EXPECT_TRUE(extract(vertexOnly, render::GenericVFPProgram::FRAGMENT_MARKER).empty());
    EXPECT_TRUE(extract("", render::GenericVFPProgram::VERTEX_MARKER).empty());
}

TEST(VertexFragmentProgram, IgnoresContentBeforeTheFirstMarker)
{
    std::string withPreamble = "# a comment before anything else\n" + std::string(TWO_SECTION_VFP);

    auto section = extract(withPreamble, render::GenericVFPProgram::VERTEX_MARKER);

    EXPECT_EQ(section.find("a comment before anything else"), std::string::npos);
    EXPECT_EQ(section.rfind(render::GenericVFPProgram::VERTEX_MARKER, 0), 0u);
}

}
