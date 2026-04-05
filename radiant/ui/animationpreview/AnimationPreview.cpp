#include "AnimationPreview.h"

#include "i18n.h"
#include "imodel.h"
#include "icameraview.h"
#include "scene/EntityNode.h"
#include "ieclass.h"
#include "imd5anim.h"
#include "itextstream.h"
#include "math/AABB.h"
#include "math/pi.h"
#include "wxutil/GLWidget.h"
#include "scene/BasicRootNode.h"
#include <fmt/format.h>
#include <cmath>

namespace ui
{

namespace
{
	const char* const FUNC_STATIC_CLASS = "func_static";

	constexpr unsigned int MOVE_FORWARD = 1 << 0;
	constexpr unsigned int MOVE_BACK = 1 << 1;
	constexpr unsigned int MOVE_STRAFERIGHT = 1 << 2;
	constexpr unsigned int MOVE_STRAFELEFT = 1 << 3;
}

AnimationPreview::AnimationPreview(wxWindow* parent) :
	wxutil::RenderPreview(parent, true),
	_freeMoveTimer(this)
{
	Bind(wxEVT_TIMER, &AnimationPreview::onFreeMoveTimer, this, _freeMoveTimer.GetId());
}

void AnimationPreview::clearModel()
{
	if (_model)
	{
		if (_entity)
		{
			_entity->removeChildNode(_model);
		}

		_model.reset();
	}
}

// Set the model, this also resets the camera
void AnimationPreview::setModelNode(const scene::INodePtr& node)
{
	// Remove the old model from the scene, if any
	clearModel();

	// Ensure that this is an MD5 model node
	model::ModelNodePtr model = Node_getModel(node);

	if (!model)
	{
		rError() << "AnimationPreview::setModelNode: node is not a model." << std::endl;
		stopPlayback();
		return;
	}

	// greebo: Call getScene() to trigger a scene setup if necessary
	getScene();

	try
	{
		dynamic_cast<const md5::IMD5Model&>(model->getIModel()).getAnim();
	}
	catch (std::bad_cast&)
	{
		rError() << "AnimationPreview::setModelNode: modelnode doesn't contain an MD5 model." << std::endl;
		stopPlayback();
		return;
	}

	_model = node;

	// Set the animation to play
	dynamic_cast<md5::IMD5Model&>(model->getIModel()).setAnim(_anim);

	// AddChildNode also tells the model which renderentity it is attached to
	_entity->addChildNode(_model);

	// Reset preview time
	stopPlayback();

	if (_model != nullptr)
	{
        // Reset the model rotation
        resetModelRotation();

		// Use AABB to adjust camera distance
		const AABB& bounds = _model->localAABB();

		if (bounds.isValid())
		{
            // Reset the default view, facing down to the model from diagonally above the bounding box
            double distance = bounds.getRadius() * 3.0f;
            setViewOrigin(Vector3(1, 1, 1) * distance);
		}
		else
		{
			// Bounds not valid, fall back to default
            setViewOrigin(Vector3(1, 1, 1) * 40.0f);
		}

        setViewAngles(Vector3(23, 135, 0));

		// Start playback when switching particles
		startPlayback();
	}

	// Redraw
	queueDraw();
}

AABB AnimationPreview::getSceneBounds()
{
	if (!_model) return RenderPreview::getSceneBounds();

	return _model->localAABB();
}

bool AnimationPreview::onPreRender()
{
	if (!_model) return false;

	// Set the animation to play
	model::ModelNodePtr model = Node_getModel(_model);
	dynamic_cast<md5::IMD5Model&>(model->getIModel()).updateAnim(_renderSystem->getTime());

	return true;
}

std::string AnimationPreview::getInfoText()
{
    auto text = RenderPreview::getInfoText();

    if (_model)
    {
        // Set the animation to play
        auto model = Node_getModel(_model);
        auto anim = dynamic_cast<md5::IMD5Model&>(model->getIModel()).getAnim();

        if (anim)
        {
            auto numFrames = anim->getNumFrames();
            auto currentFrame = (_renderSystem->getTime() / MSEC_PER_FRAME) % numFrames;
            return fmt::format(_("{0} | Frame {1} of {2}."), text, currentFrame, numFrames);
        }
    }

    return text;
}

RenderStateFlags AnimationPreview::getRenderFlagsFill()
{
	return RenderPreview::getRenderFlagsFill() | RENDER_DEPTHWRITE | RENDER_DEPTHTEST;
}

void AnimationPreview::setAnim(const md5::IMD5AnimPtr& anim)
{
	_anim = anim;

	if (!_model)
	{
		return;
	}

	// Set the animation to play
	model::ModelNodePtr model = Node_getModel(_model);
	dynamic_cast<md5::IMD5Model&>(model->getIModel()).setAnim(_anim);

	queueDraw();
}

void AnimationPreview::setupSceneGraph()
{
    _root = std::make_shared<scene::BasicRootNode>();

	_entity = GlobalEntityModule().createEntity(
		GlobalEntityClassManager().findClass(FUNC_STATIC_CLASS)
    );

    _root->addChildNode(_entity);

	// This entity is acting as our root node in the scene
	getScene()->setRoot(_root);

	auto lightClass = GlobalEntityClassManager().findClass("light");
	if (lightClass)
	{
		_light = GlobalEntityModule().createEntity(lightClass);
		_light->tryGetEntity()->setKeyValue("light_radius", "750 750 750");
		_light->tryGetEntity()->setKeyValue("origin", "150 150 150");
		_root->addChildNode(_light);
	}
}

void AnimationPreview::onModelRotationChanged()
{
    if (_entity)
    {
        // Update the model rotation on the entity
        std::ostringstream value;
        value << _modelRotation.xx() << ' '
            << _modelRotation.xy() << ' '
            << _modelRotation.xz() << ' '
            << _modelRotation.yx() << ' '
            << _modelRotation.yy() << ' '
            << _modelRotation.yz() << ' '
            << _modelRotation.zx() << ' '
            << _modelRotation.zy() << ' '
            << _modelRotation.zz();

        _entity->tryGetEntity()->setKeyValue("rotation", value.str());
    }
}

void AnimationPreview::onGLMouseClick(wxMouseEvent& ev)
{
    if (ev.LeftDown())
    {
        getGLWidget()->SetFocus();
    }

    RenderPreview::onGLMouseClick(ev);
}

void AnimationPreview::onGLMotion(wxMouseEvent& ev)
{
    if (ev.LeftIsDown())
    {
        Vector3 deltaPos(ev.GetX() - _lastX, _lastY - ev.GetY(), 0);

        _lastX = ev.GetX();
        _lastY = ev.GetY();

        Vector3 orbitCenter = getSceneBounds().getOrigin();
        double orbitDistance = (_viewOrigin - orbitCenter).getLength();

        _viewAngles[camera::CAMERA_YAW] += deltaPos.x() * 0.3;
        _viewAngles[camera::CAMERA_PITCH] -= deltaPos.y() * 0.3;

        if (_viewAngles[camera::CAMERA_PITCH] > 90)
            _viewAngles[camera::CAMERA_PITCH] = 90;
        else if (_viewAngles[camera::CAMERA_PITCH] < -90)
            _viewAngles[camera::CAMERA_PITCH] = -90;

        if (_viewAngles[camera::CAMERA_YAW] >= 360)
            _viewAngles[camera::CAMERA_YAW] -= 360;
        else if (_viewAngles[camera::CAMERA_YAW] <= 0)
            _viewAngles[camera::CAMERA_YAW] += 360;

        updateModelViewMatrix();

        Vector3 forward(_modelView[2], _modelView[6], _modelView[10]);
        _viewOrigin = orbitCenter + forward * orbitDistance;

        updateModelViewMatrix();
        queueDraw();
    }
}

void AnimationPreview::onGLKeyPress(wxKeyEvent& ev)
{
    switch (ev.GetKeyCode())
    {
    case 'W': case 'w':
        setFreeMoveFlags(MOVE_FORWARD);
        break;
    case 'S': case 's':
        setFreeMoveFlags(MOVE_BACK);
        break;
    case 'A': case 'a':
        setFreeMoveFlags(MOVE_STRAFELEFT);
        break;
    case 'D': case 'd':
        setFreeMoveFlags(MOVE_STRAFERIGHT);
        break;
    default:
        ev.Skip();
        return;
    }
}

void AnimationPreview::onGLKeyRelease(wxKeyEvent& ev)
{
    switch (ev.GetKeyCode())
    {
    case 'W': case 'w':
        clearFreeMoveFlags(MOVE_FORWARD);
        break;
    case 'S': case 's':
        clearFreeMoveFlags(MOVE_BACK);
        break;
    case 'A': case 'a':
        clearFreeMoveFlags(MOVE_STRAFELEFT);
        break;
    case 'D': case 'd':
        clearFreeMoveFlags(MOVE_STRAFERIGHT);
        break;
    default:
        ev.Skip();
        return;
    }
}

void AnimationPreview::setFreeMoveFlags(unsigned int mask)
{
    if (_freeMoveFlags == 0 && (~_freeMoveFlags & mask) != 0)
    {
        _keyControlTimer.Start();
        _freeMoveTimer.Start(10);
    }
    _freeMoveFlags |= mask;
}

void AnimationPreview::clearFreeMoveFlags(unsigned int mask)
{
    _freeMoveFlags &= ~mask;
    if (_freeMoveFlags == 0)
    {
        _freeMoveTimer.Stop();
    }
}

void AnimationPreview::onFreeMoveTimer(wxTimerEvent& ev)
{
    float timePassed = _keyControlTimer.Time() / 1000.0f;
    _keyControlTimer.Start();

    if (timePassed > 0.05f)
        timePassed = 0.05f;

    handleFreeMovement(timePassed);
    queueDraw();
}

void AnimationPreview::handleFreeMovement(float timePassed)
{
    float speed = static_cast<float>(getSceneBounds().getRadius()) * 2.0f;

    Vector3 forward(_modelView[2], _modelView[6], _modelView[10]);
    Vector3 right(_modelView[0], _modelView[4], _modelView[8]);

    if (_freeMoveFlags & MOVE_FORWARD)
        _viewOrigin -= forward * (timePassed * speed);

    if (_freeMoveFlags & MOVE_BACK)
        _viewOrigin += forward * (timePassed * speed);

    if (_freeMoveFlags & MOVE_STRAFELEFT)
        _viewOrigin -= right * (timePassed * speed);

    if (_freeMoveFlags & MOVE_STRAFERIGHT)
        _viewOrigin += right * (timePassed * speed);

    updateModelViewMatrix();
}

} // namespace
