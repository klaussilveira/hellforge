#include "CameraCubeMapDecl.h"
#include "textures/CubeMapTexture.h"
#include "debugging/gl.h"
#include "itextstream.h"
#include <stdexcept>

namespace shaders
{

// Constructor
CameraCubeMapDecl::CameraCubeMapDecl(const std::string& prefix, bool objectSuffixes)
: _prefix(prefix),
  _objectSuffixes(objectSuffixes)
{ }

// Public construction method
NamedBindablePtr CameraCubeMapDecl::createForPrefix(const std::string& prefix)
{
    return NamedBindablePtr(new CameraCubeMapDecl(prefix, false));
}

NamedBindablePtr CameraCubeMapDecl::createForObjectPrefix(const std::string& prefix)
{
    return NamedBindablePtr(new CameraCubeMapDecl(prefix, true));
}

// Bind directional image
void CameraCubeMapDecl::bindDirection(const std::string& dir,
                                      GLuint glDir) const
{
    // Load the image
    ImagePtr img = GlobalImageLoader().imageFromVFS(_prefix + dir);
    if (!img)
    {
        throw std::runtime_error(
            "Camera cube map directional image not found: "
            + _prefix + dir
        );
    }

    // Bind the image to OpenGL
    glTexImage2D(
        glDir,
        0,                    //level
        GL_RGBA,              //internal format
        static_cast<GLsizei>(img->getWidth()),   //width
        static_cast<GLsizei>(img->getHeight()),  //height
        0,                    //border
        GL_RGBA,               //format
        GL_UNSIGNED_BYTE,     //type
        img->getPixels()
    );
    debug::assertNoGlErrors();
}

/* NamedBindable implementation */

std::string CameraCubeMapDecl::getIdentifier() const
{
    return (_objectSuffixes ? "_cubeMap_" : "_cameraCubeMap_") + _prefix;
}

std::string CameraCubeMapDecl::getExpressionString()
{
    return _prefix;
}

TexturePtr CameraCubeMapDecl::bindTexture(const std::string& name,
                                          Role /* role */) const
{
    // Load the six images from the prefix
    try
    {
        // Allocate the GL texture
        GLuint texnum;
        glGenTextures(1, &texnum);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texnum);

        // Set filtering and mipmapping
		glTexParameteri(
            GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR
        );
		glTexParameteri(
            GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR
        );
		glTexParameteri(
            GL_TEXTURE_CUBE_MAP, GL_GENERATE_MIPMAP, GL_TRUE
        );

        // Bind the images
        bindDirection(_objectSuffixes ? "_px" : "_right", GL_TEXTURE_CUBE_MAP_POSITIVE_X);
        bindDirection(_objectSuffixes ? "_nx" : "_left", GL_TEXTURE_CUBE_MAP_NEGATIVE_X);
        bindDirection(_objectSuffixes ? "_py" : "_up", GL_TEXTURE_CUBE_MAP_POSITIVE_Y);
        bindDirection(_objectSuffixes ? "_ny" : "_down", GL_TEXTURE_CUBE_MAP_NEGATIVE_Y);
        bindDirection(_objectSuffixes ? "_pz" : "_forward", GL_TEXTURE_CUBE_MAP_POSITIVE_Z);
        bindDirection(_objectSuffixes ? "_nz" : "_back", GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);

        rMessage() << "[shaders] bound cubemap texture " << texnum << std::endl;

        // Unbind and create texture object
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        return TexturePtr(new CubeMapTexture(texnum, name));
    }
    catch (const std::runtime_error& e)
    {
        rError() << "[shaders] Unable to bind camera cubemap '"
                  << name << "': " << e.what() << std::endl;

        return TexturePtr();
    }
}

}
