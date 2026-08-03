#include "GenericVFPProgram.h"

#include "ifilesystem.h"
#include "itextstream.h"
#include "debugging/gl.h"

#include <istream>
#include <sstream>

namespace render
{

const char* const GenericVFPProgram::VERTEX_MARKER = "!!ARBvp1.0";
const char* const GenericVFPProgram::FRAGMENT_MARKER = "!!ARBfp1.0";

std::string GenericVFPProgram::extractProgramSection(std::istream& stream, const std::string& marker)
{
    std::stringstream section;
    bool inSection = false;

    for (std::string line; std::getline(stream, line); )
    {
        if (line.rfind(VERTEX_MARKER, 0) == 0 || line.rfind(FRAGMENT_MARKER, 0) == 0)
        {
            if (inSection) break;

            inSection = line.rfind(marker, 0) == 0;
        }

        if (inSection)
        {
            section << line << '\n';
        }
    }

    return section.str();
}

namespace
{

std::string loadProgramSection(const std::string& filename, const std::string& marker)
{
    auto file = GlobalFileSystem().openTextFile("glprogs/" + filename);

    if (!file)
    {
        rError() << "Cannot find GL program glprogs/" << filename << std::endl;
        return {};
    }

    std::istream stream(&file->getInputStream());
    auto section = GenericVFPProgram::extractProgramSection(stream, marker);

    if (section.empty())
    {
        rError() << "No " << marker << " section in glprogs/" << filename << std::endl;
    }

    return section;
}

GLuint compileProgram(GLenum target, const std::string& source, const std::string& filename)
{
    if (source.empty()) return 0;

    GLuint programId = 0;
    glGenProgramsARB(1, &programId);
    glBindProgramARB(target, programId);

    glProgramStringARB(target, GL_PROGRAM_FORMAT_ASCII_ARB,
        static_cast<GLsizei>(source.length()), source.c_str());

    GLint errorPos = -1;
    glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &errorPos);

    if (errorPos != -1)
    {
        auto message = reinterpret_cast<const char*>(glGetString(GL_PROGRAM_ERROR_STRING_ARB));

        rError() << "Error in GL program glprogs/" << filename << " at character " << errorPos
            << ": " << (message ? message : "unknown error") << std::endl;

        glDeleteProgramsARB(1, &programId);
        return 0;
    }

    return programId;
}

}

GenericVFPProgram::GenericVFPProgram(const std::string& vertexProgramFilename,
                                     const std::string& fragmentProgramFilename) :
    _vertexProgram(0),
    _fragmentProgram(0)
{
    if (!isSupported())
    {
        rWarning() << "This driver cannot run ARB programs, glprogs/" << fragmentProgramFilename
            << " will not be rendered" << std::endl;
        return;
    }

    _vertexProgram = compileProgram(GL_VERTEX_PROGRAM_ARB,
        loadProgramSection(vertexProgramFilename, VERTEX_MARKER), vertexProgramFilename);

    _fragmentProgram = compileProgram(GL_FRAGMENT_PROGRAM_ARB,
        loadProgramSection(fragmentProgramFilename, FRAGMENT_MARKER), fragmentProgramFilename);

    debug::assertNoGlErrors();
}

GenericVFPProgram::~GenericVFPProgram()
{
    if (_vertexProgram != 0)
    {
        glDeleteProgramsARB(1, &_vertexProgram);
    }

    if (_fragmentProgram != 0)
    {
        glDeleteProgramsARB(1, &_fragmentProgram);
    }
}

bool GenericVFPProgram::isSupported()
{
    return GLEW_ARB_vertex_program && GLEW_ARB_fragment_program;
}

bool GenericVFPProgram::isValid() const
{
    return _vertexProgram != 0 && _fragmentProgram != 0;
}

void GenericVFPProgram::enable()
{
    if (!isValid()) return;

    glEnable(GL_VERTEX_PROGRAM_ARB);
    glEnable(GL_FRAGMENT_PROGRAM_ARB);

    glBindProgramARB(GL_VERTEX_PROGRAM_ARB, _vertexProgram);
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, _fragmentProgram);

    debug::assertNoGlErrors();
}

void GenericVFPProgram::disable()
{
    if (!isValid()) return;

    glDisable(GL_VERTEX_PROGRAM_ARB);
    glDisable(GL_FRAGMENT_PROGRAM_ARB);

    debug::assertNoGlErrors();
}

void GenericVFPProgram::setLocalParameter(int index, const Vector4& value)
{
    if (!isValid()) return;

    float values[4] = {
        static_cast<float>(value.x()),
        static_cast<float>(value.y()),
        static_cast<float>(value.z()),
        static_cast<float>(value.w())
    };

    // The engine only feeds vertexParms to the vertex half, mirror that
    glProgramLocalParameter4fvARB(GL_VERTEX_PROGRAM_ARB, index, values);

    debug::assertNoGlErrors();
}

} // namespace
