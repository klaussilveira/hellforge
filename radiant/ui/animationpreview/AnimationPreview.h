#pragma once

#include "wxutil/preview/RenderPreview.h"

#include <memory>
#include <wx/timer.h>
#include <wx/stopwatch.h>
#include "imd5model.h"
#include "inode.h"
#include "ieclass.h"
#include "scene/Entity.h"
#include "imap.h"

namespace ui
{

class AnimationPreview :
	public wxutil::RenderPreview
{
private:
    // The scene root
    scene::IMapRootNodePtr _root;

	// Current MD5 model node to display
	scene::INodePtr _model;

	// Each model node needs a parent entity to be properly renderable
	EntityNodePtr _entity;

	// Light to illuminate the model in lighting mode
	scene::INodePtr _light;

	// The animation to play on this model
	md5::IMD5AnimPtr _anim;

	unsigned int _freeMoveFlags = 0;
	wxTimer _freeMoveTimer;
	wxStopWatch _keyControlTimer;

public:
	/** Construct a AnimationPreview widget.
	 */
	AnimationPreview(wxWindow* parent);

	void setModelNode(const scene::INodePtr& model);
	void setAnim(const md5::IMD5AnimPtr& anim);

	const scene::INodePtr& getModelNode() const
	{
		return _model;
	}

	const md5::IMD5AnimPtr& getAnim() const
	{
		return _anim;
	}

    AABB getSceneBounds() override;

protected:
	// Creates parent entity etc.
    void setupSceneGraph() override;

    bool onPreRender() override;
    std::string getInfoText() override;

	void clearModel();

    RenderStateFlags getRenderFlagsFill() override;

    void onModelRotationChanged() override;

    void onGLMouseClick(wxMouseEvent& ev) override;
    void onGLMotion(wxMouseEvent& ev) override;
    void onGLKeyPress(wxKeyEvent& ev) override;
    void onGLKeyRelease(wxKeyEvent& ev) override;

private:
	void setFreeMoveFlags(unsigned int mask);
	void clearFreeMoveFlags(unsigned int mask);
	void onFreeMoveTimer(wxTimerEvent& ev);
	void handleFreeMovement(float timePassed);
};
typedef std::shared_ptr<AnimationPreview> AnimationPreviewPtr;

}
