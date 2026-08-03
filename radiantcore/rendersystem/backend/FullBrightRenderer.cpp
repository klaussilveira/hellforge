#include "FullBrightRenderer.h"

#include "OpenGLShaderPass.h"
#include "OpenGLShader.h"
#include "glprogram/GenericVFPProgram.h"

namespace render
{

namespace
{

class FullBrightRenderResult : 
    public IRenderResult
{
private:
    std::string _statistics;

public:
    FullBrightRenderResult(const std::string& statistics) :
        _statistics(statistics)
    {}

    std::string toString() override
    {
        return _statistics;
    }
};

}

IRenderResult::Ptr FullBrightRenderer::render(RenderStateFlags globalstate, const IRenderView& view, std::size_t time)
{
    // Make sure all the data is uploaded
    _geometryStore.syncToBufferObjects();

    // Construct default OpenGL state
    OpenGLState current;
    setupState(current);

    setupViewMatrices(view);

    // Bind the vertex and index buffer object before drawing geometry
    auto [vertexBuffer, indexBuffer] = _geometryStore.getBufferObjects();

    vertexBuffer->bind();
    indexBuffer->bind();

    // Set the attribute pointers
    _objectRenderer.initAttributePointers();

    // Iterate over the sorted mapping between OpenGLStates and their
    // OpenGLShaderPasses (containing the renderable geometry), and render the
    // contents of each bucket. Each pass is passed a reference to the "current"
    // state, which it can change.
    for (const auto& [_, pass] : _sortedStates)
    {
        // Render the OpenGLShaderPass
        if (pass->empty()) continue;

        if (pass->getShader().isVisible() && pass->isApplicableTo(_renderViewType))
        {
            // Apply our state to the current state object
            pass->evaluateStagesAndApplyState(current, globalstate, time, nullptr);

            // vertexParms are defined per stage, uploading once per pass is enough
            if (auto vfp = dynamic_cast<GenericVFPProgram*>(current.glProgram); vfp != nullptr)
            {
                if (const auto& stage = pass->state().stage0; stage)
                {
                    for (int parm = 0; parm < stage->getNumVertexParms(); ++parm)
                    {
                        if (stage->getVertexParm(parm).index < 0) continue;

                        vfp->setLocalParameter(parm, stage->getVertexParmValue(parm));
                    }
                }
            }

            if (!pass->hasRenderables())
            {
                // All regular geometry like patches, brushes, meshes, single vertices
                pass->submitSurfaces(view);
            }
            else
            {
                // Selection overlays are processed by OpenGLRenderable
                pass->submitRenderables(current);
            }
        }

        pass->clearRenderables();
    }

    // Unbind the geometry buffer and draw the rest of the renderables
    vertexBuffer->unbind();
    indexBuffer->unbind();

    cleanupState();

    return std::make_shared<FullBrightRenderResult>(view.getCullStats());
}

}
