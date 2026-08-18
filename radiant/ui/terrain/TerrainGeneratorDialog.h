#pragma once

#include "icommandsystem.h"
#include "inode.h"
#include "math/Vector3.h"
#include "ui/common/GeneratorPreview.h"
#include "wxutil/dialog/Dialog.h"
#include "wxutil/XmlResourceBasedWidget.h"
#include "noise/TerrainGenerator.h"

namespace ui
{

/**
 * Dialog for generating terrain patch meshes using procedural noise
 */
class TerrainGeneratorDialog : public wxutil::Dialog, private wxutil::XmlResourceBasedWidget
{
private:
    wxWindow* _fractalSizer;
    wxWindow* _offsetLabel;
    wxWindow* _offsetCtrl;
    scene::INodePtr _parent;
    GeneratorPreview _preview;

public:
    TerrainGeneratorDialog(const scene::INodePtr& parent);

    GeneratorPreview& getPreview();
    void commitToMap();

    // Get selected noise algorithm
    noise::Algorithm getAlgorithm();

    // Patch dimensions
    std::size_t getColumns();
    std::size_t getRows();

    // Physical dimensions
    float getPhysicalWidth();
    float getPhysicalHeight();

    // Noise params
    unsigned int getSeed();
    float getFrequency();
    float getAmplitude();

    // Fractal params
    int getOctaves();
    float getPersistence();
    float getLacunarity();
    float getOffset();

    std::string getMaterial();

    static void Show(const cmd::ArgumentList& args);

private:
    void onAlgorithmChanged(wxCommandEvent& ev);
    void onRandomizeSeed(wxCommandEvent& ev);
    void onBrowseMaterial(wxCommandEvent& ev);
    void onParameterChanged(wxCommandEvent& ev);
    void generateInto();
    void regenerate();
    void updateControlVisibility();
};

} // namespace ui
