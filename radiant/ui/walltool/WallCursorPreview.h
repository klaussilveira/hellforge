#pragma once

#include "irenderable.h"
#include "irender.h"
#include "render/RenderableVertexArray.h"
#include "render/StaticRenderableText.h"

namespace ui
{

class WallCursorPreview :
    public Renderable
{
private:
    std::vector<Vertex3> _poleVertices;
    std::vector<Vertex3> _crossXVertices;
    std::vector<Vertex3> _crossYVertices;
    std::vector<Vertex3> _pointVertices;
    std::vector<Vertex3> _ghostVertices;

    render::RenderableLine _poleOrtho;
    render::RenderableLine _crossXOrtho;
    render::RenderableLine _crossYOrtho;
    render::RenderablePoints _pointsOrtho;
    render::RenderableLine _ghostOrtho;

    render::RenderableLine _poleCamera;
    render::RenderableLine _crossXCamera;
    render::RenderableLine _crossYCamera;
    render::RenderablePoints _pointsCamera;
    render::RenderableLine _ghostCamera;

    ShaderPtr _orthoLineShader;
    ShaderPtr _orthoLineShaderConnected;
    ShaderPtr _orthoPointShader;
    ShaderPtr _orthoGhostShader;
    ShaderPtr _cameraLineShader;
    ShaderPtr _cameraLineShaderConnected;
    ShaderPtr _cameraPointShader;
    ShaderPtr _cameraGhostShader;

    ITextRenderer::Ptr _textRenderer;
    std::shared_ptr<render::StaticRenderableText> _dimensionText;

public:
    WallCursorPreview();

    void clear();

    void setRenderSystem(const RenderSystemPtr&) override {}
    void onPreRender(const VolumeTest& volume) override;
    void renderHighlights(IRenderableCollector&, const VolumeTest&) override {}
    std::size_t getHighlightFlags() override { return Highlight::NoHighlight; }
};

}
