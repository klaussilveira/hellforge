#include "WallCursorPreview.h"

#include "igrid.h"
#include "imap.h"
#include "xyview/tools/WallTool.h"

#include <fmt/format.h>

namespace ui
{

namespace
{
    const Vector4 CursorColour(1.0, 0.6, 0.1, 1.0);
    const Vector4 GhostColour(1.0, 0.85, 0.5, 1.0);
    const Vector4 ConnectColour(0.3, 1.0, 0.4, 1.0);
    const Vector4 TextColour(1.0, 1.0, 1.0, 1.0);
}

WallCursorPreview::WallCursorPreview() :
    _poleOrtho(_poleVertices),
    _crossXOrtho(_crossXVertices),
    _crossYOrtho(_crossYVertices),
    _pointsOrtho(_pointVertices),
    _ghostOrtho(_ghostVertices),
    _poleCamera(_poleVertices),
    _crossXCamera(_crossXVertices),
    _crossYCamera(_crossYVertices),
    _pointsCamera(_pointVertices),
    _ghostCamera(_ghostVertices)
{
    _poleOrtho.setColour(CursorColour);
    _crossXOrtho.setColour(CursorColour);
    _crossYOrtho.setColour(CursorColour);
    _pointsOrtho.setColour(CursorColour);
    _ghostOrtho.setColour(GhostColour);
    _poleCamera.setColour(CursorColour);
    _crossXCamera.setColour(CursorColour);
    _crossYCamera.setColour(CursorColour);
    _pointsCamera.setColour(CursorColour);
    _ghostCamera.setColour(GhostColour);
}

void WallCursorPreview::clear()
{
    _poleVertices.clear();
    _crossXVertices.clear();
    _crossYVertices.clear();
    _pointVertices.clear();
    _ghostVertices.clear();

    _poleOrtho.clear();
    _crossXOrtho.clear();
    _crossYOrtho.clear();
    _pointsOrtho.clear();
    _ghostOrtho.clear();
    _poleCamera.clear();
    _crossXCamera.clear();
    _crossYCamera.clear();
    _pointsCamera.clear();
    _ghostCamera.clear();

    if (_dimensionText)
    {
        _dimensionText->setVisible(false);
    }
}

void WallCursorPreview::onPreRender(const VolumeTest& volume)
{
    const auto& settings = WallToolSettings::Instance();

    if (!settings.active || !settings.hoverValid)
    {
        clear();
        return;
    }

    auto renderSystem = GlobalMapModule().getRoot()
        ? GlobalMapModule().getRoot()->getRenderSystem()
        : RenderSystemPtr();

    if (!renderSystem)
    {
        return;
    }

    const Vector3& base = settings.hoverPoint;
    double arm = std::max(4.0, GlobalGrid().getGridSize() * 0.5);

    const Vector4& cursorColour = settings.hoverConnected ? ConnectColour : CursorColour;
    const Vector4& ghostColour = settings.hoverConnected ? ConnectColour : GhostColour;

    _poleOrtho.setColour(cursorColour);
    _crossXOrtho.setColour(cursorColour);
    _crossYOrtho.setColour(cursorColour);
    _pointsOrtho.setColour(cursorColour);
    _ghostOrtho.setColour(ghostColour);
    _poleCamera.setColour(cursorColour);
    _crossXCamera.setColour(cursorColour);
    _crossYCamera.setColour(cursorColour);
    _pointsCamera.setColour(cursorColour);
    _ghostCamera.setColour(ghostColour);

    _poleVertices.clear();
    _crossXVertices.clear();
    _crossYVertices.clear();
    _pointVertices.clear();
    _ghostVertices.clear();

    _poleVertices.emplace_back(base.x(), base.y(), base.z());
    _poleVertices.emplace_back(base.x(), base.y(), base.z() + settings.wallHeight);

    _crossXVertices.emplace_back(base.x() - arm, base.y(), base.z());
    _crossXVertices.emplace_back(base.x() + arm, base.y(), base.z());

    _crossYVertices.emplace_back(base.x(), base.y() - arm, base.z());
    _crossYVertices.emplace_back(base.x(), base.y() + arm, base.z());

    _pointVertices.emplace_back(base.x(), base.y(), base.z());

    if (!_textRenderer)
    {
        _textRenderer = renderSystem->captureTextRenderer(IGLFont::Style::Mono, 14);
    }

    if (!_dimensionText)
    {
        _dimensionText = std::make_shared<render::StaticRenderableText>("", Vector3(0, 0, 0), TextColour);
    }

    if (settings.segmentPreviewValid)
    {
        const Vector2& start = settings.segmentPreviewStart;
        const Vector2& end = settings.segmentPreviewEnd;
        double z = settings.segmentPreviewBaseZ;

        if (settings.segmentPreviewGhost)
        {
            _ghostVertices.emplace_back(start.x(), start.y(), z);
            _ghostVertices.emplace_back(end.x(), end.y(), z);
        }

        Vector2 dir = (end - start).getNormalised();
        Vector2 perp(-dir.y(), dir.x());
        Vector2 mid = (start + end) * 0.5;
        double offset = settings.wallThickness * 0.5 + 12.0;

        double length = std::round((end - start).getLength() * 10.0) / 10.0;

        _dimensionText->setText(fmt::format("{:g}", length));
        _dimensionText->setWorldPosition(Vector3(mid.x() + perp.x() * offset, mid.y() + perp.y() * offset, z + 1));
        _dimensionText->setVisible(true);
    }
    else
    {
        _dimensionText->setVisible(false);
    }

    _dimensionText->update(_textRenderer);

    if (volume.fill())
    {
        if (!_cameraLineShader)
        {
            _cameraLineShader = renderSystem->capture(ColourShaderType::CameraOutline, CursorColour);
        }

        if (!_cameraLineShaderConnected)
        {
            _cameraLineShaderConnected = renderSystem->capture(ColourShaderType::CameraOutline, ConnectColour);
        }

        if (!_cameraPointShader)
        {
            _cameraPointShader = renderSystem->capture(BuiltInShaderType::BigPoint);
        }

        if (!_cameraGhostShader)
        {
            _cameraGhostShader = renderSystem->capture(ColourShaderType::CameraOutline, GhostColour);
        }

        const auto& lineShader = settings.hoverConnected ? _cameraLineShaderConnected : _cameraLineShader;
        const auto& ghostShader = settings.hoverConnected ? _cameraLineShaderConnected : _cameraGhostShader;

        _poleCamera.queueUpdate();
        _crossXCamera.queueUpdate();
        _crossYCamera.queueUpdate();
        _pointsCamera.queueUpdate();
        _ghostCamera.queueUpdate();

        _poleCamera.update(lineShader);
        _crossXCamera.update(lineShader);
        _crossYCamera.update(lineShader);
        _pointsCamera.update(_cameraPointShader);
        _ghostCamera.update(ghostShader);
    }
    else
    {
        if (!_orthoLineShader)
        {
            _orthoLineShader = renderSystem->capture(ColourShaderType::OrthoviewSolid, CursorColour);
        }

        if (!_orthoLineShaderConnected)
        {
            _orthoLineShaderConnected = renderSystem->capture(ColourShaderType::OrthoviewSolid, ConnectColour);
        }

        if (!_orthoPointShader)
        {
            _orthoPointShader = renderSystem->capture(BuiltInShaderType::BigPoint);
        }

        if (!_orthoGhostShader)
        {
            _orthoGhostShader = renderSystem->capture(ColourShaderType::OrthoviewSolid, GhostColour);
        }

        const auto& lineShader = settings.hoverConnected ? _orthoLineShaderConnected : _orthoLineShader;
        const auto& ghostShader = settings.hoverConnected ? _orthoLineShaderConnected : _orthoGhostShader;

        _poleOrtho.queueUpdate();
        _crossXOrtho.queueUpdate();
        _crossYOrtho.queueUpdate();
        _pointsOrtho.queueUpdate();
        _ghostOrtho.queueUpdate();

        _poleOrtho.update(lineShader);
        _crossXOrtho.update(lineShader);
        _crossYOrtho.update(lineShader);
        _pointsOrtho.update(_orthoPointShader);
        _ghostOrtho.update(ghostShader);
    }
}

}
