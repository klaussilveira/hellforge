#include "GLWidget.h"

#include "igl.h"
#include "itextstream.h"
#include "ui/iwxgl.h"

#include "GLContext.h"

#include <vector>
#include <wx/dcclient.h>
#include <wx/image.h>

namespace wxutil
{

const int ATTRIBS [] = {
	WX_GL_RGBA,
	WX_GL_DOUBLEBUFFER,
	WX_GL_DEPTH_SIZE, 24,
	0
};

GLWidget::GLWidget(wxWindow *parent, const std::function<bool()>& renderCallback, const std::string& name) :
    wxGLCanvas(parent, wxID_ANY, ATTRIBS, wxDefaultPosition, wxDefaultSize,
               wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS,
               wxString(name.c_str(), *wxConvCurrent)),
    _registered(false),
    _renderCallback(renderCallback),
    _privateContext(nullptr)
{
    Bind(wxEVT_PAINT, &GLWidget::OnPaint, this);
    Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent&) {
        // needs to be empty
        // see: https://forums.wxwidgets.org/viewtopic.php?t=24461
    });
}

void GLWidget::SetHasPrivateContext(bool hasPrivateContext)
{
	if (hasPrivateContext)
	{
		_privateContext = new wxGLContext(this);
	}
	else
	{
		DestroyPrivateContext();
	}
}

void GLWidget::DestroyPrivateContext()
{
	if (_privateContext != nullptr)
	{
		_privateContext->UnRef();
		_privateContext = nullptr;
	}
}

GLWidget::~GLWidget()
{
	DestroyPrivateContext();

	if (_registered)
	{
		GlobalWxGlWidgetManager().unregisterGLWidget(this);
	}
}

void GLWidget::OnPaint(wxPaintEvent& WXUNUSED(event))
{
	// Got this check from the wxWidgets sources, they assert the widget to be shown
	// "although on MSW it works even if the window is still hidden, it doesn't
    // work in other ports (notably X11-based ones) and documentation mentions
    // that SetCurrent() can only be called for a shown window, so check for it"
	if (!IsShownOnScreen()) return;

	// Make sure this widget is registered
	if (!_registered)
	{
		_registered = true;

		GlobalWxGlWidgetManager().registerGLWidget(this);
	}

    // This is required even though dc is not used otherwise.
    wxPaintDC dc(this);

	// Grab the context for this widget
	if (_privateContext != nullptr)
	{
		// Use the private context for this widget
		SetCurrent(*_privateContext);
	}
	else
	{
		// Use the globally shared context, we rely on this being of type GLContext
		const auto& context = GlobalOpenGLContext().getSharedContext();
		assert(std::dynamic_pointer_cast<GLContext>(context));

		auto wxContext = std::static_pointer_cast<GLContext>(context);
		SetCurrent(wxContext->get());
	}

	if (_renderCallback())
	{
		// Render callback returned true, so drawing took place
		// and we can swap the buffers
		SwapBuffers();
	}
}

bool GLWidget::captureToFile(const std::string& filename, int maxWidth)
{
	if (!IsShownOnScreen()) return false;

	// Make the GL context current (same logic as OnPaint)
	if (_privateContext != nullptr)
	{
		SetCurrent(*_privateContext);
	}
	else
	{
		const auto& context = GlobalOpenGLContext().getSharedContext();
		auto wxContext = std::static_pointer_cast<GLContext>(context);
		SetCurrent(wxContext->get());
	}

	// Render the scene to the back buffer
	if (!_renderCallback())
		return false;

	int width = GetClientSize().GetWidth();
	int height = GetClientSize().GetHeight();
	if (width <= 0 || height <= 0)
		return false;

	// Now read from the back buffer
	glReadBuffer(GL_BACK);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	std::vector<unsigned char> pixels(width * height * 3);
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

	// GL reads bottom-up, wxImage expects top-down
	wxImage image(width, height, false);
	auto* dst = image.GetData();
	int rowBytes = width * 3;

	for (int y = 0; y < height; y++)
	{
		memcpy(dst + y * rowBytes,
		       pixels.data() + (height - 1 - y) * rowBytes,
		       rowBytes);
	}

	// Scale down if requested
	if (maxWidth > 0 && width > maxWidth)
	{
		int newHeight = height * maxWidth / width;
		if (newHeight > 0)
			image.Rescale(maxWidth, newHeight, wxIMAGE_QUALITY_HIGH);
	}

	return image.SaveFile(filename, wxBITMAP_TYPE_PNG);
}

} // namespace
