#pragma once

#include <GL/glew.h>
#include <memory>
#include <string>
#include <wx/glcanvas.h>
#include <functional>

// greebo: Undo the min max macro definitions coming from a windows header
#undef min
#undef max

namespace wxutil
{

class GLWidget :
	public wxGLCanvas
{
	// TRUE, if this GL widget has been registered
	bool _registered;

	// The attached client method to invoke to render this view
	std::function<bool()> _renderCallback;

	// Some widgets have their own openGL context, 
	// If it  is non-NULL _privateContext will be used. 
	wxGLContext* _privateContext;

public:
    GLWidget(wxWindow *parent, const std::function<bool()>& renderCallback, const std::string& name);

	// Call this to enable/disable the private GL context of this widget
	void SetHasPrivateContext(bool hasPrivateContext);

	// wx reports window sizes in logical pixels on HiDPI displays, while
	// OpenGL viewport/readback dimensions must use physical framebuffer pixels.
	wxSize GetGLViewportSize() const;
	wxSize GetGLViewportSize(const wxSize& logicalSize) const;

	bool MakeCurrent();

	// Render the scene to the back buffer and save as PNG.
	// Must be called on the main thread. Returns true on success.
	bool captureToFile(const std::string& filename, int maxWidth = 0);

	// Render the scene to the back buffer and read it into the given image.
	// Must be called on the main thread. Returns true on success.
	bool captureImage(wxImage& image, int maxWidth = 0);

	virtual ~GLWidget();

private:
	void DestroyPrivateContext();
	void OnPaint(wxPaintEvent& event);
};
typedef std::shared_ptr<GLWidget> GLWidgetPtr;

} // namespace
