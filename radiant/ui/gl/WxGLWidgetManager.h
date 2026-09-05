#pragma once

#include <set>
#include "ui/iwxgl.h"

namespace ui
{

class WxGLWidgetManager :
	public IWxGLWidgetManager
{
private:
	std::set<wxutil::GLWidget*> _wxGLWidgets;

public:
	void registerGLWidget(wxutil::GLWidget* widget) override;
	void unregisterGLWidget(wxutil::GLWidget* widget) override;
	bool makeContextCurrent() override;

	// RegisterableModule implementation
	std::string getName() const override;
	void shutdownModule() override;
};

}
