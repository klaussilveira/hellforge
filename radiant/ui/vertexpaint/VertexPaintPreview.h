#pragma once

#include "irenderable.h"
#include "irender.h"
#include "render/RenderableVertexArray.h"

namespace ui
{

class VertexPaintPreview : public Renderable
{
private:
    std::vector<Vertex3> _circleVertices;
    std::vector<Vertex3> _innerCircleVertices;

    render::RenderableLine _circle;
    render::RenderableLine _innerCircle;

    ShaderPtr _lineShader;
    ShaderPtr _innerLineShader;

public:
    VertexPaintPreview();

    void clear();

    void setRenderSystem(const RenderSystemPtr&) override {}
    void onPreRender(const VolumeTest& volume) override;
    void renderHighlights(IRenderableCollector&, const VolumeTest&) override {}
    std::size_t getHighlightFlags() override { return Highlight::NoHighlight; }
};

}
