#pragma once

#include <GL/glew.h>

namespace wxutil
{

class GLFrameBuffer
{
private:
    GLuint _fbo = 0;
    GLuint _colourBuffer = 0;
    GLuint _depthBuffer = 0;
    int _size = 0;

public:
    ~GLFrameBuffer()
    {
        release();
    }

    bool bind(int size)
    {
        if (size != _size)
        {
            release();
            create(size);
        }

        if (_fbo == 0) return false;

        glBindFramebuffer(GL_FRAMEBUFFER, _fbo);

        return true;
    }

    void unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

private:
    void create(int size)
    {
        glGenFramebuffers(1, &_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, _fbo);

        glGenRenderbuffers(1, &_colourBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, _colourBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, size, size);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _colourBuffer);

        glGenRenderbuffers(1, &_depthBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, _depthBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depthBuffer);

        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        if (!complete)
        {
            release();
            return;
        }

        _size = size;
    }

    void release()
    {
        if (_colourBuffer != 0)
        {
            glDeleteRenderbuffers(1, &_colourBuffer);
            _colourBuffer = 0;
        }

        if (_depthBuffer != 0)
        {
            glDeleteRenderbuffers(1, &_depthBuffer);
            _depthBuffer = 0;
        }

        if (_fbo != 0)
        {
            glDeleteFramebuffers(1, &_fbo);
            _fbo = 0;
        }

        _size = 0;
    }
};

}
