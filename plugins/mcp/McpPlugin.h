#pragma once

#include "icommandsystem.h"
#include "imodule.h"

#include "Json.h"

#include <thread>
#include <mutex>
#include <atomic>
#include <wx/event.h>

namespace mcp
{

class McpPlugin :
    public wxEvtHandler,
    public RegisterableModule
{
public:
    McpPlugin();
    ~McpPlugin();

    std::string getName() const override;
    StringSet getDependencies() const override;
    void initialiseModule(const IApplicationContext& ctx) override;
    void shutdownModule() override;

private:
    // TCP server
    void serverLoop();
    void handleClient(int clientFd);
    std::string processLine(const std::string& line);

    // JSON-RPC dispatch
    JsonValue dispatch(const std::string& method, const JsonValue& params);

    // Map tools
    JsonValue getMapInfo(const JsonValue& params);
    JsonValue newMap(const JsonValue& params);
    JsonValue openMap(const JsonValue& params);
    JsonValue saveMap(const JsonValue& params);

    // Entities tools
    JsonValue createEntity(const JsonValue& params);
    JsonValue setEntityProperty(const JsonValue& params);
    JsonValue getEntityProperties(const JsonValue& params);
    JsonValue listEntities(const JsonValue& params);
    JsonValue listEntityClasses(const JsonValue& params);

    // Brushes tools
    JsonValue createBrush(const JsonValue& params);
    JsonValue createBrushes(const JsonValue& params);
    JsonValue cloneBrush(const JsonValue& params);

    // Batch entity tools
    JsonValue createEntities(const JsonValue& params);

    // Selection tools
    JsonValue selectAll(const JsonValue& params);
    JsonValue deselectAll(const JsonValue& params);
    JsonValue deleteSelection(const JsonValue& params);

    // Camera tools
    JsonValue getCamera(const JsonValue& params);
    JsonValue setCamera(const JsonValue& params);

    // Commands tools
    JsonValue executeCommand(const JsonValue& params);

    // Undo tools
    JsonValue undo(const JsonValue& params);
    JsonValue redo(const JsonValue& params);

    // Scene tools
    JsonValue getSceneInfo(const JsonValue& params);

    // Brush inspection/manipulation tools
    JsonValue listBrushes(const JsonValue& params);
    JsonValue getBrushInfo(const JsonValue& params);
    JsonValue setBrushMaterial(const JsonValue& params);
    JsonValue deleteBrush(const JsonValue& params);
    JsonValue deleteBrushes(const JsonValue& params);
    JsonValue moveBrush(const JsonValue& params);
    JsonValue resizeBrush(const JsonValue& params);
    JsonValue convertBrushesToEntity(const JsonValue& params);

    // Spatial queries tools
    JsonValue queryRegion(const JsonValue& params);
    JsonValue getSceneBounds(const JsonValue& params);

    // Entity manipulation tools
    JsonValue moveEntity(const JsonValue& params);
    JsonValue rotateEntity(const JsonValue& params);
    JsonValue deleteEntity(const JsonValue& params);

    // Measurement/reference tools
    JsonValue measureDistance(const JsonValue& params);
    JsonValue getGameConfig(const JsonValue& params);
    JsonValue listSpecialMaterials(const JsonValue& params);

    // Command discovery tools
    JsonValue listCommands(const JsonValue& params);

    // Asset queries tools
    JsonValue listMaterials(const JsonValue& params);
    JsonValue getMaterialInfo(const JsonValue& params);
    JsonValue getEntityClassInfo(const JsonValue& params);
    JsonValue listModels(const JsonValue& params);
    JsonValue listSounds(const JsonValue& params);
    JsonValue listSkins(const JsonValue& params);
    JsonValue listParticles(const JsonValue& params);

    // Shortcut/help tools
    JsonValue listShortcuts(const JsonValue& params);
    JsonValue getCommandShortcut(const JsonValue& params);

    // Prefab tools
    JsonValue listPrefabs(const JsonValue& params);
    JsonValue getPrefabInfo(const JsonValue& params);
    JsonValue insertPrefab(const JsonValue& params);

    // Game map browsing tools
    JsonValue listMaps(const JsonValue& params);

    // View capture tools
    JsonValue captureView(const JsonValue& params);

    std::thread _serverThread;
    std::atomic<bool> _running{false};
    int _serverFd = -1;
    int _port = 13646;
};

} // namespace mcp
