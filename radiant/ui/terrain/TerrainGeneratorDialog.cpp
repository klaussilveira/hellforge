#include "TerrainGeneratorDialog.h"

#include "i18n.h"
#include "ui/imainframe.h"
#include "ui/common/GeneratorSpawn.h"
#include "noise/TerrainGenerator.h"
#include "icommandsystem.h"
#include "iscenegraph.h"
#include "ishaderclipboard.h"

#include "string/convert.h"
#include "selectionlib.h"
#include "shaderlib.h"

#include <random>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/spinctrl.h>
#include <wx/choice.h>
#include <wx/button.h>
#include <wx/statbox.h>

#include "ui/materials/MaterialChooser.h"
#include "ui/materials/MaterialSelector.h"
#include "wxutil/PickerButton.h"

namespace
{
const char* const WINDOW_TITLE = N_("Terrain Generator");

// Gets the active/selected shader or the default fallback value
inline std::string getSelectedShader()
{
    auto selectedShader = GlobalShaderClipboard().getShaderName();

    if (selectedShader.empty())
    {
        selectedShader = texdef_name_default();
    }

    return selectedShader;
}

} // namespace

namespace ui
{

TerrainGeneratorDialog::TerrainGeneratorDialog(const scene::INodePtr& parent)
    : Dialog(_(WINDOW_TITLE), GlobalMainFrame().getWxTopLevelWindow()), _fractalSizer(nullptr),
      _parent(parent)
{
    _dialog->GetSizer()->Add(
        loadNamedPanel(_dialog, "TerrainGeneratorMainPanel"), 1, wxEXPAND | wxALL, 12);

    wxStaticText* topLabel = findNamedObject<wxStaticText>(_dialog, "TerrainGeneratorTopLabel");
    topLabel->SetFont(topLabel->GetFont().Bold());

    // Bind events
    wxChoice* algorithmChoice = findNamedObject<wxChoice>(_dialog, "TerrainGeneratorAlgorithm");

    for (int i = 0; i < noise::AlgorithmCount; ++i)
    {
        algorithmChoice->Append(_(noise::getAlgorithmName(static_cast<noise::Algorithm>(i))));
    }

    algorithmChoice->SetSelection(static_cast<int>(noise::Algorithm::FBm));
    algorithmChoice->Bind(wxEVT_CHOICE, &TerrainGeneratorDialog::onAlgorithmChanged, this);

    wxButton* randomizeBtn = findNamedObject<wxButton>(_dialog, "TerrainGeneratorRandomizeSeed");
    randomizeBtn->Bind(wxEVT_BUTTON, &TerrainGeneratorDialog::onRandomizeSeed, this);

    auto* browseBtn = wxutil::ReplaceWithPickerButton(
        findNamedObject<wxButton>(_dialog, "TerrainGeneratorBrowseMaterial"));
    browseBtn->Bind(wxEVT_BUTTON, &TerrainGeneratorDialog::onBrowseMaterial, this);

    // Set a random seed on init
    std::random_device rd;
    findNamedObject<wxSpinCtrl>(_dialog, "TerrainGeneratorSeed")->SetValue(rd() % 1000000000);

    findNamedObject<wxTextCtrl>(_dialog, "TerrainGeneratorMaterial")->SetValue(getSelectedShader());

    updateControlVisibility();

    bindParameterEvents(_dialog, this, &TerrainGeneratorDialog::onParameterChanged);

    regenerate();
}

GeneratorPreview& TerrainGeneratorDialog::getPreview()
{
    return _preview;
}

void TerrainGeneratorDialog::onParameterChanged(wxCommandEvent& ev)
{
    regenerate();
}

void TerrainGeneratorDialog::generateInto()
{
    std::size_t columns = getColumns();
    std::size_t rows = getRows();

    if (columns < 2 || rows < 2 || getPhysicalWidth() <= 0 || getPhysicalHeight() <= 0)
    {
        return;
    }

    float physicalWidth = getPhysicalWidth();
    float physicalHeight = getPhysicalHeight();

    Vector3 spawnPos = getGeneratorSpawnPosition(
        std::max(256.0, static_cast<double>(std::max(physicalWidth, physicalHeight))));

    noise::NoiseParameters params;
    params.algorithm = getAlgorithm();
    params.seed = getSeed();
    params.frequency = getFrequency();
    params.amplitude = getAmplitude();
    params.octaves = getOctaves();
    params.persistence = getPersistence();
    params.lacunarity = getLacunarity();

    noise::generateTerrainPatch(params, columns, rows, physicalWidth, physicalHeight,
        spawnPos, getMaterial(), _parent);
}

void TerrainGeneratorDialog::regenerate()
{
    _preview.update(_parent, [this]() { generateInto(); });
}

void TerrainGeneratorDialog::commitToMap()
{
    _preview.commit(_parent, "terrainGeneratorCreate", [this]() { generateInto(); });
}

void TerrainGeneratorDialog::onAlgorithmChanged(wxCommandEvent& ev)
{
    updateControlVisibility();
    _dialog->Layout();
    _dialog->Fit();
    regenerate();
}

void TerrainGeneratorDialog::onRandomizeSeed(wxCommandEvent& ev)
{
    std::random_device rd;
    findNamedObject<wxSpinCtrl>(_dialog, "TerrainGeneratorSeed")->SetValue(rd() % 1000000000);
    regenerate();
}

void TerrainGeneratorDialog::onBrowseMaterial(wxCommandEvent& ev)
{
    wxTextCtrl* materialEntry = findNamedObject<wxTextCtrl>(_dialog, "TerrainGeneratorMaterial");
    MaterialChooser chooser(_dialog, MaterialSelector::TextureFilter::Regular, materialEntry);
    chooser.ShowModal();
}

void TerrainGeneratorDialog::updateControlVisibility()
{
    bool showFractal = noise::algorithmUsesFractalParameters(getAlgorithm());

    // Show/hide fractal parameter controls
    findNamedObject<wxSpinCtrl>(_dialog, "TerrainGeneratorOctaves")->GetParent()->Show(showFractal);
}

noise::Algorithm TerrainGeneratorDialog::getAlgorithm()
{
    wxChoice* choice = findNamedObject<wxChoice>(_dialog, "TerrainGeneratorAlgorithm");
    return static_cast<noise::Algorithm>(choice->GetSelection());
}

std::size_t TerrainGeneratorDialog::getColumns()
{
    wxChoice* choice = findNamedObject<wxChoice>(_dialog, "TerrainGeneratorColumns");
    return string::convert<std::size_t>(choice->GetStringSelection().ToStdString(), 11);
}

std::size_t TerrainGeneratorDialog::getRows()
{
    wxChoice* choice = findNamedObject<wxChoice>(_dialog, "TerrainGeneratorRows");
    return string::convert<std::size_t>(choice->GetStringSelection().ToStdString(), 11);
}

float TerrainGeneratorDialog::getPhysicalWidth()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "TerrainGeneratorWidth")->GetValue().ToStdString(),
        512.0f);
}

float TerrainGeneratorDialog::getPhysicalHeight()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "TerrainGeneratorHeight")->GetValue().ToStdString(),
        512.0f);
}

unsigned int TerrainGeneratorDialog::getSeed()
{
    return static_cast<unsigned int>(
        findNamedObject<wxSpinCtrl>(_dialog, "TerrainGeneratorSeed")->GetValue());
}

float TerrainGeneratorDialog::getFrequency()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "TerrainGeneratorFrequency")->GetValue().ToStdString(),
        0.01f);
}

float TerrainGeneratorDialog::getAmplitude()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "TerrainGeneratorAmplitude")->GetValue().ToStdString(),
        64.0f);
}

int TerrainGeneratorDialog::getOctaves()
{
    return findNamedObject<wxSpinCtrl>(_dialog, "TerrainGeneratorOctaves")->GetValue();
}

float TerrainGeneratorDialog::getPersistence()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "TerrainGeneratorPersistence")
            ->GetValue()
            .ToStdString(),
        0.5f);
}

float TerrainGeneratorDialog::getLacunarity()
{
    return string::convert<float>(findNamedObject<wxTextCtrl>(_dialog, "TerrainGeneratorLacunarity")
                                      ->GetValue()
                                      .ToStdString(),
                                  2.0f);
}

std::string TerrainGeneratorDialog::getMaterial()
{
    return findNamedObject<wxTextCtrl>(_dialog, "TerrainGeneratorMaterial")
        ->GetValue()
        .ToStdString();
}

void TerrainGeneratorDialog::Show(const cmd::ArgumentList& args)
{
    scene::INodePtr worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    TerrainGeneratorDialog dialog(worldspawn);

    if (dialog.run() == IDialog::RESULT_OK)
    {
        dialog.commitToMap();
    }
    else
    {
        dialog.getPreview().clear();
    }
}

} // namespace ui
