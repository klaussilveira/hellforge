#pragma once

#include "i18n.h"
#include "icommandsystem.h"
#include "itextstream.h"
#include "iundo.h"
#include "iselection.h"
#include "isound.h"
#include "scene/EntityNode.h"
#include "fmt/format.h"
#include "command/ExecutionFailure.h"
#include "string/convert.h"
#include "gamelib.h"

namespace entity
{

namespace algorithm
{

inline void CreateSpeaker(const cmd::ArgumentList& args)
{
    constexpr const char* SPEAKER_CLASSNAME = "speaker";

    if (args.size() != 2)
    {
        rWarning() << "Usage: CreateSpeaker <soundShader:string> <position:Vector3>" << std::endl;
        return;
    }

    UndoableCommand command("addSpeaker");

    // Cancel all selection
    GlobalSelectionSystem().setSelectedAll(false);

    // Create the speaker entity (exceptions are allowed to leak)
    auto spkNode = GlobalEntityModule().createEntityFromSelection(
        SPEAKER_CLASSNAME, args[1].getVector3()
    );

    auto shader = args[0].getString();

    if (shader.empty() || !module::GlobalModuleRegistry().moduleExists(MODULE_SOUNDMANAGER))
    {
        return; // done here
    }

    auto soundShader = GlobalSoundManager().getSoundShader(shader);

    if (!soundShader)
    {
        throw cmd::ExecutionFailure(fmt::format(_("Cannot find sound shader: {0}"), shader));
    }

    // Set the shader keyvalue
    auto& entity = spkNode->getEntity();

    entity.setKeyValue("s_shader", soundShader->getDeclName());

    // Initialise the speaker with suitable distance values
    auto radii = soundShader->getRadii();
    auto inMetres = game::current::soundRadiiInMetres();

    if (radii.getMax() <= 0)
    {
        radii.setMax(10, true);
    }

    entity.setKeyValue("s_mindistance", string::to_string(radii.getMin(inMetres)));
    entity.setKeyValue("s_maxdistance", string::to_string(radii.getMax(inMetres)));
}

}

}
