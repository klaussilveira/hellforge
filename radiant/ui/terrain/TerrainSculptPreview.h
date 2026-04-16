#pragma once

#include "irenderable.h"
#include "irender.h"
#include "render/RenderableVertexArray.h"

namespace ui
{

class TerrainSculptPreview : public Renderable
{
private:
    std::vector<Vertex3> _circleVertices;
    std::vector<Vertex3> _innerCircleVertices;
    std::vector<Vertex3> _pointVertices;

    render::RenderableLine _circle;
    render::RenderableLine _innerCircle;
    render::RenderablePoints _points;

    ShaderPtr _lineShader;
    ShaderPtr _innerLineShader;
    ShaderPtr _pointShader;

public:
    TerrainSculptPreview();

    void clear();

    void setRenderSystem(const RenderSystemPtr&) override {}
    void onPreRender(const VolumeTest& volume) override;
    void renderHighlights(IRenderableCollector&, const VolumeTest&) override {}
    std::size_t getHighlightFlags() override { return Highlight::NoHighlight; }
};

}
