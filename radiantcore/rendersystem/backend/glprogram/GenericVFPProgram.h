#pragma once

#include "iglprogram.h"
#include "igl.h"
#include "math/Vector4.h"

#include <iosfwd>
#include <string>

namespace render
{

/**
 * An ARB vertex/fragment program as referenced by a idTech4 material
 * declaration. The programs are loaded from the game's glprogs/ folder.
 */
class GenericVFPProgram: public GLProgram
{
    // openGL Vertex and fragment program identifiers
    GLuint _vertexProgram;
    GLuint _fragmentProgram;

public:
    GenericVFPProgram(const std::string& vertexProgramFilename,
                      const std::string& fragmentProgramFilename);

    ~GenericVFPProgram();

    // True if both halves compiled
    bool isValid() const;

    // Uploads the value to program.local[index]
    void setLocalParameter(int index, const Vector4& value);

    // True if the driver can run ARB programs
    static bool isSupported();

    // Returns the half of the .vfp file starting at the given !!ARB marker
    static std::string extractProgramSection(std::istream& stream, const std::string& marker);

    static const char* const VERTEX_MARKER;
    static const char* const FRAGMENT_MARKER;

    // GLProgram implementation
    void enable() override;
    void disable() override;
};

} // namespace render
