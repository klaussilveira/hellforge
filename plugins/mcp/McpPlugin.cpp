#include "McpPlugin.h"

#include "imap.h"
#include "iscenegraph.h"
#include "iselection.h"
#include "icameraview.h"
#include "ientity.h"
#include "ibrush.h"
#include "iclipper.h"
#include "ieclass.h"
#include "iundo.h"
#include "ishaders.h"
#include "isound.h"
#include "iparticles.h"
#include "modelskin.h"
#include "ifilesystem.h"
#include "igame.h"
#include "imapresource.h"
#include "entitylib.h"
#include "scene/PrefabBoundsAccumulator.h"
#include "ui/ieventmanager.h"
#include "scene/Entity.h"
#include "scene/EntityNode.h"
#include "scene/EntityClass.h"

#include "math/AABB.h"
#include "math/Plane3.h"
#include "math/Matrix3.h"
#include "math/Vector3.h"
#include "iselectable.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <algorithm>
#include <set>
#include <cmath>
#include <future>
#include <sstream>
#include <fstream>

#include "fmt/format.h"

namespace mcp
{

McpPlugin::McpPlugin() = default;

McpPlugin::~McpPlugin()
{
    _running = false;
    if (_serverFd >= 0)
        ::close(_serverFd);
    if (_serverThread.joinable())
        _serverThread.join();
}

std::string McpPlugin::getName() const
{
    static std::string _name("McpServer");
    return _name;
}

StringSet McpPlugin::getDependencies() const
{
    static StringSet _dependencies{
        MODULE_COMMANDSYSTEM, MODULE_MAP, MODULE_SCENEGRAPH,
        MODULE_SELECTIONSYSTEM, MODULE_CAMERA_MANAGER,
        MODULE_ENTITY, MODULE_BRUSHCREATOR, MODULE_ECLASSMANAGER,
        MODULE_SHADERSYSTEM, MODULE_SOUNDMANAGER,
        MODULE_PARTICLESMANAGER, MODULE_MODELSKINCACHE,
        MODULE_VIRTUALFILESYSTEM, MODULE_MAPRESOURCEMANAGER,
        MODULE_GAMEMANAGER, MODULE_EVENTMANAGER
    };
    return _dependencies;
}

void McpPlugin::initialiseModule(const IApplicationContext& ctx)
{
    rMessage() << "[MCP] Initialising MCP server plugin" << std::endl;

    _running = true;
    _serverThread = std::thread(&McpPlugin::serverLoop, this);

    rMessage() << "[MCP] Server starting on port " << _port << std::endl;
}

void McpPlugin::shutdownModule()
{
    rMessage() << "[MCP] Shutting down MCP server" << std::endl;
    _running = false;
    if (_serverFd >= 0)
    {
        ::shutdown(_serverFd, SHUT_RDWR);
        ::close(_serverFd);
        _serverFd = -1;
    }
    if (_serverThread.joinable())
        _serverThread.join();
}

void McpPlugin::serverLoop()
{
    _serverFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (_serverFd < 0)
    {
        rError() << "[MCP] Failed to create socket" << std::endl;
        return;
    }

    int opt = 1;
    setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(_port);

    if (::bind(_serverFd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        rError() << "[MCP] Failed to bind to port " << _port << std::endl;
        ::close(_serverFd);
        _serverFd = -1;
        return;
    }

    if (::listen(_serverFd, 4) < 0)
    {
        rError() << "[MCP] Failed to listen" << std::endl;
        ::close(_serverFd);
        _serverFd = -1;
        return;
    }

    rMessage() << "[MCP] Listening on localhost:" << _port << std::endl;

    while (_running)
    {
        struct pollfd pfd{};
        pfd.fd = _serverFd;
        pfd.events = POLLIN;

        int ret = ::poll(&pfd, 1, 500); // 500ms timeout for shutdown checks
        if (ret <= 0) continue;

        int clientFd = ::accept(_serverFd, nullptr, nullptr);
        if (clientFd < 0) continue;

        // Handle client in a detached thread
        std::thread(&McpPlugin::handleClient, this, clientFd).detach();
    }
}

void McpPlugin::handleClient(int clientFd)
{
    rMessage() << "[MCP] Client connected" << std::endl;

    std::string buffer;
    char chunk[4096];

    while (_running)
    {
        struct pollfd pfd{};
        pfd.fd = clientFd;
        pfd.events = POLLIN;

        int ret = ::poll(&pfd, 1, 500);
        if (ret < 0) break;
        if (ret == 0) continue;

        ssize_t n = ::recv(clientFd, chunk, sizeof(chunk), 0);
        if (n <= 0) break;

        buffer.append(chunk, n);

        // Process complete lines (newline-delimited JSON)
        std::size_t lineEnd;
        while ((lineEnd = buffer.find('\n')) != std::string::npos)
        {
            std::string line = buffer.substr(0, lineEnd);
            buffer.erase(0, lineEnd + 1);

            if (line.empty()) continue;

            std::string response = processLine(line);
            response += '\n';

            // Send response
            const char* data = response.c_str();
            std::size_t remaining = response.size();
            while (remaining > 0)
            {
                ssize_t sent = ::send(clientFd, data, remaining, MSG_NOSIGNAL);
                if (sent <= 0) goto done;
                data += sent;
                remaining -= sent;
            }
        }
    }

done:
    ::close(clientFd);
    rMessage() << "[MCP] Client disconnected" << std::endl;
}

std::string McpPlugin::processLine(const std::string& line)
{
    try
    {
        auto request = JsonValue::parse(line);

        auto id = request["id"];
        auto method = request["method"].getString();
        auto params = request.has("params") ? request["params"] : JsonValue(JsonValue::Object{});

        // Execute on the main (wx) thread using a promise/future
        std::promise<JsonValue> promise;
        auto future = promise.get_future();

        CallAfter([this, &promise, method, params]() {
            try
            {
                auto result = dispatch(method, params);
                promise.set_value(result);
            }
            catch (const std::exception& e)
            {
                promise.set_value(jsonRpcError(nullptr, -32000, e.what()));
            }
        });

        auto result = future.get();

        // Check if result is already an error response
        if (result.isObject() && result.has("error"))
            return jsonObject({{"jsonrpc", "2.0"}, {"id", id}, {"error", result["error"]}}).dump();

        return jsonRpcResult(id, result).dump();
    }
    catch (const std::exception& e)
    {
        return jsonRpcError(nullptr, -32700, std::string("Parse error: ") + e.what()).dump();
    }
}

JsonValue McpPlugin::dispatch(const std::string& method, const JsonValue& params)
{
    if (method == "get_map_info") return getMapInfo(params);
    if (method == "new_map") return newMap(params);
    if (method == "open_map") return openMap(params);
    if (method == "save_map") return saveMap(params);
    if (method == "create_entity") return createEntity(params);
    if (method == "set_entity_property") return setEntityProperty(params);
    if (method == "get_entity_properties") return getEntityProperties(params);
    if (method == "list_entities") return listEntities(params);
    if (method == "list_entity_classes") return listEntityClasses(params);
    if (method == "create_brush") return createBrush(params);
    if (method == "create_brushes") return createBrushes(params);
    if (method == "clone_brush") return cloneBrush(params);
    if (method == "create_entities") return createEntities(params);
    if (method == "select_all") return selectAll(params);
    if (method == "deselect_all") return deselectAll(params);
    if (method == "delete_selection") return deleteSelection(params);
    if (method == "get_camera") return getCamera(params);
    if (method == "set_camera") return setCamera(params);
    if (method == "execute_command") return executeCommand(params);
    if (method == "undo") return undo(params);
    if (method == "redo") return redo(params);
    if (method == "get_scene_info") return getSceneInfo(params);
    if (method == "list_brushes") return listBrushes(params);
    if (method == "get_brush_info") return getBrushInfo(params);
    if (method == "set_brush_material") return setBrushMaterial(params);
    if (method == "delete_brush") return deleteBrush(params);
    if (method == "delete_brushes") return deleteBrushes(params);
    if (method == "move_brush") return moveBrush(params);
    if (method == "resize_brush") return resizeBrush(params);
    if (method == "convert_brushes_to_entity") return convertBrushesToEntity(params);
    if (method == "query_region") return queryRegion(params);
    if (method == "get_scene_bounds") return getSceneBounds(params);
    if (method == "move_entity") return moveEntity(params);
    if (method == "rotate_entity") return rotateEntity(params);
    if (method == "delete_entity") return deleteEntity(params);
    if (method == "measure_distance") return measureDistance(params);
    if (method == "get_game_config") return getGameConfig(params);
    if (method == "list_special_materials") return listSpecialMaterials(params);
    if (method == "list_commands") return listCommands(params);
    if (method == "list_materials") return listMaterials(params);
    if (method == "get_material_info") return getMaterialInfo(params);
    if (method == "get_entity_class_info") return getEntityClassInfo(params);
    if (method == "list_models") return listModels(params);
    if (method == "list_sounds") return listSounds(params);
    if (method == "list_skins") return listSkins(params);
    if (method == "list_particles") return listParticles(params);
    if (method == "list_prefabs") return listPrefabs(params);
    if (method == "get_prefab_info") return getPrefabInfo(params);
    if (method == "insert_prefab") return insertPrefab(params);
    if (method == "list_shortcuts") return listShortcuts(params);
    if (method == "get_command_shortcut") return getCommandShortcut(params);
    if (method == "capture_view") return captureView(params);

    // CSG operations (all work on current selection)
    if (method == "csg_subtract") { GlobalCommandSystem().executeCommand("CSGSubtract"); return jsonObject({{"success", true}}); }
    if (method == "csg_merge") { GlobalCommandSystem().executeCommand("CSGMerge"); return jsonObject({{"success", true}}); }
    if (method == "csg_hollow") { GlobalCommandSystem().executeCommand("CSGHollow"); return jsonObject({{"success", true}}); }
    if (method == "csg_room") { GlobalCommandSystem().executeCommand("CSGRoom"); return jsonObject({{"success", true}}); }
    if (method == "csg_intersect") { GlobalCommandSystem().executeCommand("CSGIntersect"); return jsonObject({{"success", true}}); }
    if (method == "csg_seal") { GlobalCommandSystem().executeCommand("CSGSeal"); return jsonObject({{"success", true}}); }
    if (method == "csg_passable") { GlobalCommandSystem().executeCommand("CSGPassable"); return jsonObject({{"success", true}}); }

    // Brush shape conversion (works on current selection)
    if (method == "make_brush_prefab")
    {
        std::string shape = params["shape"].getString();
        int sides = params.has("sides") ? static_cast<int>(params["sides"].getNumber()) : 8;

        int prefabType;
        if (shape == "prism") prefabType = static_cast<int>(brush::PrefabType::Prism);
        else if (shape == "cone") prefabType = static_cast<int>(brush::PrefabType::Cone);
        else if (shape == "sphere") prefabType = static_cast<int>(brush::PrefabType::Sphere);
        else if (shape == "cuboid") prefabType = static_cast<int>(brush::PrefabType::Cuboid);
        else throw std::runtime_error("Invalid shape: " + shape + ". Use 'prism', 'cone', 'sphere', or 'cuboid'.");

        cmd::ArgumentList args;
        args.push_back(prefabType);
        args.push_back(sides);
        GlobalCommandSystem().executeCommand("BrushMakePrefab", args);
        return jsonObject({{"success", true}, {"shape", shape}, {"sides", sides}});
    }

    // Clip/split selected brushes by a plane defined by 2 or 3 points
    if (method == "clip_brush")
    {
        std::string mode = params.has("mode") ? params["mode"].getString() : "clip";

        auto& p1Arr = params["point1"];
        auto& p2Arr = params["point2"];
        Vector3 p1(p1Arr[0].getNumber(), p1Arr[1].getNumber(), p1Arr[2].getNumber());
        Vector3 p2(p2Arr[0].getNumber(), p2Arr[1].getNumber(), p2Arr[2].getNumber());

        // Enter clip mode
        GlobalCommandSystem().executeCommand("ToggleManipulatorMode", cmd::Argument(std::string("Clip")));

        // Set clip points
        GlobalClipper().newClipPoint(p1);
        GlobalClipper().newClipPoint(p2);
        if (params.has("point3"))
        {
            auto& p3Arr = params["point3"];
            Vector3 p3(p3Arr[0].getNumber(), p3Arr[1].getNumber(), p3Arr[2].getNumber());
            GlobalClipper().newClipPoint(p3);
        }

        // Flip if requested
        if (params.has("flip") && params["flip"].getBool())
            GlobalCommandSystem().executeCommand("FlipClip");

        // Execute
        if (mode == "split")
            GlobalCommandSystem().executeCommand("SplitSelected");
        else
            GlobalCommandSystem().executeCommand("ClipSelected");

        // Exit clip mode back to drag
        GlobalCommandSystem().executeCommand("ToggleManipulatorMode", cmd::Argument(std::string("Drag")));

        return jsonObject({{"success", true}, {"mode", mode}});
    }

    throw std::runtime_error("Unknown method: " + method);
}

// Map Operations

JsonValue McpPlugin::getMapInfo(const JsonValue& params)
{
    auto root = GlobalSceneGraph().root();
    std::string mapName = GlobalMapModule().getMapName();
    bool modified = GlobalMapModule().isModified();

    int entityCount = 0;
    int brushCount = 0;
    int patchCount = 0;

    if (root)
    {
        GlobalSceneGraph().foreachNode([&](const scene::INodePtr& node) {
            switch (node->getNodeType())
            {
                case scene::INode::Type::Entity: ++entityCount; break;
                case scene::INode::Type::Brush: ++brushCount; break;
                case scene::INode::Type::Patch: ++patchCount; break;
                default: break;
            }
            return true;
        });
    }

    return jsonObject({
        {"map_name", mapName},
        {"modified", modified},
        {"entity_count", entityCount},
        {"brush_count", brushCount},
        {"patch_count", patchCount}
    });
}

JsonValue McpPlugin::newMap(const JsonValue& params)
{
    GlobalCommandSystem().executeCommand("NewMap");
    return jsonObject({{"success", true}});
}

JsonValue McpPlugin::openMap(const JsonValue& params)
{
    std::string path = params["path"].getString();
    GlobalCommandSystem().executeCommand("OpenMap", cmd::Argument(path));
    return jsonObject({{"success", true}});
}

JsonValue McpPlugin::saveMap(const JsonValue& params)
{
    if (params.has("path"))
    {
        std::string path = params["path"].getString();
        GlobalCommandSystem().executeCommand("SaveMapAs", cmd::Argument(path));
    }
    else
    {
        GlobalCommandSystem().executeCommand("SaveMap");
    }
    return jsonObject({{"success", true}});
}

// Helper: find entity by name, falling back to classname for worldspawn-type entities
static scene::INodePtr findEntityByName(const std::string& entityName)
{
    scene::INodePtr result;
    GlobalSceneGraph().foreachNode([&](const scene::INodePtr& node) {
        if (result) return false;
        Entity* entity = node->tryGetEntity();
        if (!entity) return true;

        std::string name = entity->getKeyValue("name");
        if (name.empty())
            name = entity->getKeyValue("classname");

        if (name == entityName)
        {
            result = node;
            return false;
        }
        return true;
    });
    return result;
}

// Forward declarations (defined after createRoom)
static JsonValue createSingleBrush(const JsonValue& params, scene::INodePtr parent);
static JsonValue createSingleEntity(const JsonValue& params);

// Entity Operations

JsonValue McpPlugin::createEntity(const JsonValue& params)
{
    auto& undoSystem = GlobalMapModule().getUndoSystem();
    undoSystem.start();
    auto result = createSingleEntity(params);
    undoSystem.finish("Create entity");
    return result;
}

JsonValue McpPlugin::setEntityProperty(const JsonValue& params)
{
    std::string entityName = params["name"].getString();
    std::string key = params["key"].getString();
    std::string value = params["value"].getString();

    auto entityNode = findEntityByName(entityName);
    if (!entityNode)
        throw std::runtime_error("Entity not found: " + entityName);

    Entity* entity = entityNode->tryGetEntity();
    auto& undoSystem = GlobalMapModule().getUndoSystem();
    undoSystem.start();
    entity->setKeyValue(key, value);
    undoSystem.finish("Set entity property");

    return jsonObject({{"success", true}});
}

JsonValue McpPlugin::getEntityProperties(const JsonValue& params)
{
    std::string entityName = params["name"].getString();

    auto entityNode = findEntityByName(entityName);
    if (!entityNode)
        throw std::runtime_error("Entity not found: " + entityName);

    JsonValue::Object properties;
    Entity* entity = entityNode->tryGetEntity();
    entity->forEachKeyValue([&](const std::string& key, const std::string& value) {
        properties[key] = value;
    });

    return JsonValue(std::move(properties));
}

JsonValue McpPlugin::listEntities(const JsonValue& params)
{
    JsonValue::Array entities;

    GlobalSceneGraph().foreachNode([&](const scene::INodePtr& node) {
        Entity* entity = node->tryGetEntity();
        if (entity)
        {
            std::string classname = entity->getKeyValue("classname");
            std::string name = entity->getKeyValue("name");
            if (name.empty()) name = classname;
            std::string origin = entity->getKeyValue("origin");

            // Count child brushes
            int brushCount = 0;
            node->foreachNode([&](const scene::INodePtr& child) {
                if (child->getNodeType() == scene::INode::Type::Brush)
                    ++brushCount;
                return true;
            });

            entities.push_back(jsonObject({
                {"classname", classname},
                {"name", name},
                {"origin", origin},
                {"brush_count", brushCount}
            }));
        }
        // Don't recurse into entity children for top-level listing
        return true;
    });

    return JsonValue(std::move(entities));
}

JsonValue McpPlugin::listEntityClasses(const JsonValue& params)
{
    JsonValue::Array classes;
    std::string filter;
    int limit = 100;

    if (params.has("filter")) filter = params["filter"].getString();
    if (params.has("limit")) limit = params["limit"].getInt();

    int count = 0;
    GlobalEntityClassManager().forEachEntityClass(
        [&](const scene::EntityClass::Ptr& eclass) {
            if (count >= limit) return;

            std::string name = eclass->getDeclName();

            if (!filter.empty() && name.find(filter) == std::string::npos)
                return;

            classes.push_back(jsonObject({
                {"name", name},
                {"has_fixed_size", eclass->isFixedSize()},
                {"is_light", eclass->isLight()}
            }));
            ++count;
        }
    );

    return JsonValue(std::move(classes));
}

// Brush Operations

JsonValue McpPlugin::createBrush(const JsonValue& params)
{
    scene::INodePtr parent;
    if (params.has("entity"))
    {
        std::string entityName = params["entity"].getString();
        parent = findEntityByName(entityName);
        if (!parent)
            throw std::runtime_error("Entity not found: " + entityName);
    }
    else
    {
        parent = GlobalMapModule().findOrInsertWorldspawn();
    }

    auto& undoSystem = GlobalMapModule().getUndoSystem();
    undoSystem.start();
    auto result = createSingleBrush(params, parent);
    undoSystem.finish("Create brush");

    JsonValue::Object obj = result.getObject();
    obj["success"] = true;
    return JsonValue(std::move(obj));
}

// Helper: create a single axis-aligned brush and add to parent, returns result object
static JsonValue createSingleBrush(const JsonValue& params, scene::INodePtr parent)
{
    auto& minArr = params["min"];
    auto& maxArr = params["max"];

    double minX = minArr[0].getNumber();
    double minY = minArr[1].getNumber();
    double minZ = minArr[2].getNumber();
    double maxX = maxArr[0].getNumber();
    double maxY = maxArr[1].getNumber();
    double maxZ = maxArr[2].getNumber();

    std::string material = "textures/common/caulk";
    if (params.has("material"))
        material = params["material"].getString();

    auto faceMat = [&](const std::string& face) -> std::string {
        if (params.has("materials") && params["materials"].isObject() && params["materials"].has(face))
            return params["materials"][face].getString();
        return material;
    };

    double texScale = 0.0078125;
    Matrix3 defaultProj = Matrix3::getIdentity();
    defaultProj.xx() = texScale;
    defaultProj.yy() = texScale;

    auto brushNode = GlobalBrushCreator().createBrush();
    auto* brush = Node_getIBrush(brushNode);
    if (!brush)
        throw std::runtime_error("Failed to create brush");

    brush->addFace(Plane3( 1, 0, 0,  maxX), defaultProj, faceMat("east"));
    brush->addFace(Plane3(-1, 0, 0, -minX), defaultProj, faceMat("west"));
    brush->addFace(Plane3( 0, 1, 0,  maxY), defaultProj, faceMat("north"));
    brush->addFace(Plane3( 0,-1, 0, -minY), defaultProj, faceMat("south"));
    brush->addFace(Plane3( 0, 0, 1,  maxZ), defaultProj, faceMat("top"));
    brush->addFace(Plane3( 0, 0,-1, -minZ), defaultProj, faceMat("bottom"));

    parent->addChildNode(brushNode);

    int brushIndex = 0;
    int childIdx = 0;
    parent->foreachNode([&](const scene::INodePtr& child) {
        if (child == brushNode)
            brushIndex = childIdx;
        ++childIdx;
        return true;
    });

    JsonValue::Object faceMap;
    for (std::size_t i = 0; i < brush->getNumFaces(); ++i)
    {
        auto& normal = brush->getFace(i).getPlane3().normal();
        const char* faceName = nullptr;
        if      (normal.x() >  0.9) faceName = "east";
        else if (normal.x() < -0.9) faceName = "west";
        else if (normal.y() >  0.9) faceName = "north";
        else if (normal.y() < -0.9) faceName = "south";
        else if (normal.z() >  0.9) faceName = "top";
        else if (normal.z() < -0.9) faceName = "bottom";
        if (faceName)
            faceMap[faceName] = static_cast<int>(i);
    }

    return jsonObject({
        {"index", brushIndex},
        {"face_map", JsonValue(std::move(faceMap))},
        {"min", jsonArray({minX, minY, minZ})},
        {"max", jsonArray({maxX, maxY, maxZ})},
        {"material", material}
    });
}

JsonValue McpPlugin::createBrushes(const JsonValue& params)
{
    auto& brushArr = params["brushes"];
    if (!brushArr.isArray())
        throw std::runtime_error("'brushes' must be an array");

    // Determine parent entity
    scene::INodePtr parent;
    if (params.has("entity"))
    {
        std::string entityName = params["entity"].getString();
        parent = findEntityByName(entityName);
        if (!parent)
            throw std::runtime_error("Entity not found: " + entityName);
    }
    else
    {
        parent = GlobalMapModule().findOrInsertWorldspawn();
    }

    auto& undoSystem = GlobalMapModule().getUndoSystem();
    undoSystem.start();

    JsonValue::Array results;
    for (std::size_t i = 0; i < brushArr.size(); ++i)
    {
        auto& spec = brushArr[i];
        // Allow per-brush entity override
        scene::INodePtr brushParent = parent;
        if (spec.has("entity"))
        {
            std::string entName = spec["entity"].getString();
            brushParent = findEntityByName(entName);
            if (!brushParent)
                throw std::runtime_error("Entity not found: " + entName);
        }
        results.push_back(createSingleBrush(spec, brushParent));
    }

    undoSystem.finish("Create brushes (batch)");

    return jsonObject({
        {"success", true},
        {"count", static_cast<int>(results.size())},
        {"brushes", JsonValue(std::move(results))}
    });
}

JsonValue McpPlugin::cloneBrush(const JsonValue& params)
{
    int srcIndex = static_cast<int>(params["index"].getNumber());

    scene::INodePtr entityNode;
    if (params.has("entity"))
    {
        std::string entityName = params["entity"].getString();
        entityNode = findEntityByName(entityName);
        if (!entityNode)
            throw std::runtime_error("Entity not found: " + entityName);
    }
    else
    {
        entityNode = GlobalMapModule().findOrInsertWorldspawn();
    }

    // Find source brush
    scene::INodePtr srcBrushNode;
    int idx = 0;
    entityNode->foreachNode([&](const scene::INodePtr& child) {
        if (child->getNodeType() == scene::INode::Type::Brush)
        {
            if (idx == srcIndex)
            {
                srcBrushNode = child;
                return false;
            }
            ++idx;
        }
        return true;
    });
    if (!srcBrushNode)
        throw std::runtime_error("Brush not found at index " + std::to_string(srcIndex));

    auto* srcBrush = Node_getIBrush(srcBrushNode);
    if (!srcBrush)
        throw std::runtime_error("Failed to read source brush");

    // Get offset
    double offX = 0, offY = 0, offZ = 0;
    if (params.has("offset"))
    {
        auto& off = params["offset"];
        offX = off[0].getNumber();
        offY = off[1].getNumber();
        offZ = off[2].getNumber();
    }

    // Determine destination parent
    scene::INodePtr destParent;
    if (params.has("target_entity"))
    {
        std::string targetName = params["target_entity"].getString();
        destParent = findEntityByName(targetName);
        if (!destParent)
            throw std::runtime_error("Target entity not found: " + targetName);
    }
    else
    {
        destParent = entityNode; // same parent
    }

    auto& undoSystem = GlobalMapModule().getUndoSystem();
    undoSystem.start();

    auto newBrushNode = GlobalBrushCreator().createBrush();
    auto* newBrush = Node_getIBrush(newBrushNode);

    double texScale = 0.0078125;
    Matrix3 defaultProj = Matrix3::getIdentity();
    defaultProj.xx() = texScale;
    defaultProj.yy() = texScale;

    for (std::size_t i = 0; i < srcBrush->getNumFaces(); ++i)
    {
        auto& face = srcBrush->getFace(i);
        auto plane = face.getPlane3();
        std::string mat = face.getShader();

        // Offset the plane: new_dist = old_dist + normal . offset
        double newDist = plane.dist() + plane.normal().x() * offX
                                      + plane.normal().y() * offY
                                      + plane.normal().z() * offZ;

        newBrush->addFace(Plane3(plane.normal(), newDist), defaultProj, mat);
    }

    destParent->addChildNode(newBrushNode);

    int brushIndex = 0;
    int childIdx = 0;
    destParent->foreachNode([&](const scene::INodePtr& child) {
        if (child == newBrushNode)
            brushIndex = childIdx;
        ++childIdx;
        return true;
    });

    // Build face_map
    JsonValue::Object faceMap;
    for (std::size_t i = 0; i < newBrush->getNumFaces(); ++i)
    {
        auto& normal = newBrush->getFace(i).getPlane3().normal();
        const char* faceName = nullptr;
        if      (normal.x() >  0.9) faceName = "east";
        else if (normal.x() < -0.9) faceName = "west";
        else if (normal.y() >  0.9) faceName = "north";
        else if (normal.y() < -0.9) faceName = "south";
        else if (normal.z() >  0.9) faceName = "top";
        else if (normal.z() < -0.9) faceName = "bottom";
        if (faceName)
            faceMap[faceName] = static_cast<int>(i);
    }

    // Compute new bounds from source bounds + offset
    auto srcAABB = srcBrushNode->worldAABB();
    auto newMin = srcAABB.origin - srcAABB.extents + Vector3(offX, offY, offZ);
    auto newMax = srcAABB.origin + srcAABB.extents + Vector3(offX, offY, offZ);

    undoSystem.finish("Clone brush");

    return jsonObject({
        {"success", true},
        {"index", brushIndex},
        {"face_map", JsonValue(std::move(faceMap))},
        {"min", jsonArray({newMin.x(), newMin.y(), newMin.z()})},
        {"max", jsonArray({newMax.x(), newMax.y(), newMax.z()})}
    });
}

// Helper: create a single entity without undo wrapping
static JsonValue createSingleEntity(const JsonValue& params)
{
    std::string classname = params["classname"].getString();

    double x = 0, y = 0, z = 0;
    if (params.has("origin"))
    {
        auto& origin = params["origin"];
        x = origin[0].getNumber();
        y = origin[1].getNumber();
        z = origin[2].getNumber();
    }

    Vector3 originVec(x, y, z);

    auto eclass = GlobalEntityClassManager().findClass(classname);
    if (!eclass)
        throw std::runtime_error("Entity class not found: " + classname);

    auto entityNode = GlobalEntityModule().createEntity(eclass);
    Entity* entity = entityNode->tryGetEntity();

    if (entity)
    {
        entity->setKeyValue("origin", fmt::format("{} {} {}", x, y, z));

        if (params.has("properties") && params["properties"].isObject())
        {
            for (auto& [key, val] : params["properties"].getObject())
            {
                entity->setKeyValue(key, val.getString());
            }
        }
    }

    auto root = GlobalSceneGraph().root();
    if (root)
        root->addChildNode(entityNode);

    std::string name = entity ? entity->getKeyValue("name") : "";
    return jsonObject({
        {"success", true},
        {"name", name},
        {"classname", classname}
    });
}

JsonValue McpPlugin::createEntities(const JsonValue& params)
{
    auto& entArr = params["entities"];
    if (!entArr.isArray())
        throw std::runtime_error("'entities' must be an array");

    auto& undoSystem = GlobalMapModule().getUndoSystem();
    undoSystem.start();

    JsonValue::Array results;
    for (std::size_t i = 0; i < entArr.size(); ++i)
    {
        results.push_back(createSingleEntity(entArr[i]));
    }

    undoSystem.finish("Create entities (batch)");

    return jsonObject({
        {"success", true},
        {"count", static_cast<int>(results.size())},
        {"entities", JsonValue(std::move(results))}
    });
}

// Selection Operations

JsonValue McpPlugin::selectAll(const JsonValue& params)
{
    GlobalSelectionSystem().setSelectedAll(true);
    return jsonObject({{"success", true}});
}

JsonValue McpPlugin::deselectAll(const JsonValue& params)
{
    GlobalSelectionSystem().setSelectedAll(false);
    return jsonObject({{"success", true}});
}

JsonValue McpPlugin::deleteSelection(const JsonValue& params)
{
    GlobalCommandSystem().executeCommand("DeleteSelection");
    return jsonObject({{"success", true}});
}

// Camera Operations

JsonValue McpPlugin::getCamera(const JsonValue& params)
{
    try
    {
        auto& camera = GlobalCameraManager().getActiveView();
        auto origin = camera.getCameraOrigin();
        auto angles = camera.getCameraAngles();

        return jsonObject({
            {"origin", jsonArray({origin.x(), origin.y(), origin.z()})},
            {"angles", jsonArray({angles.x(), angles.y(), angles.z()})}
        });
    }
    catch (const std::runtime_error&)
    {
        throw std::runtime_error("No active camera view");
    }
}

JsonValue McpPlugin::setCamera(const JsonValue& params)
{
    try
    {
        auto& camera = GlobalCameraManager().getActiveView();

        if (params.has("origin"))
        {
            auto& o = params["origin"];
            Vector3 origin(o[0].getNumber(), o[1].getNumber(), o[2].getNumber());

            if (params.has("angles"))
            {
                auto& a = params["angles"];
                Vector3 angles(a[0].getNumber(), a[1].getNumber(), a[2].getNumber());
                camera.setOriginAndAngles(origin, angles);
            }
            else
            {
                camera.setCameraOrigin(origin);
            }
        }
        else if (params.has("angles"))
        {
            auto& a = params["angles"];
            Vector3 angles(a[0].getNumber(), a[1].getNumber(), a[2].getNumber());
            camera.setCameraAngles(angles);
        }

        return jsonObject({{"success", true}});
    }
    catch (const std::runtime_error&)
    {
        throw std::runtime_error("No active camera view");
    }
}

// Command Execution

JsonValue McpPlugin::executeCommand(const JsonValue& params)
{
    std::string command = params["command"].getString();
    GlobalCommandSystem().execute(command);
    return jsonObject({{"success", true}});
}

// Undo/Redo

JsonValue McpPlugin::undo(const JsonValue& params)
{
    GlobalCommandSystem().executeCommand("Undo");
    return jsonObject({{"success", true}});
}

JsonValue McpPlugin::redo(const JsonValue& params)
{
    GlobalCommandSystem().executeCommand("Redo");
    return jsonObject({{"success", true}});
}

// Scene Info

JsonValue McpPlugin::getSceneInfo(const JsonValue& params)
{
    auto& selInfo = GlobalSelectionSystem().getSelectionInfo();
    int selCount = GlobalSelectionSystem().countSelected();

    JsonValue::Array selectedEntities;
    GlobalSelectionSystem().foreachSelected([&](const scene::INodePtr& node) {
        Entity* entity = node->tryGetEntity();
        if (entity)
        {
            selectedEntities.push_back(jsonObject({
                {"name", entity->getKeyValue("name")},
                {"classname", entity->getKeyValue("classname")}
            }));
        }
    });

    return jsonObject({
        {"selection_count", selCount},
        {"selected_entities", JsonValue(std::move(selectedEntities))}
    });
}

// Brush Inspection/Manipulation

// Helper: identify brushes as "entityName:brushIndex"
// Walk the scene graph collecting brush info
struct BrushInfo
{
    scene::INodePtr node;
    std::string entityName;
    int index;          // child index within parent entity
    AABB bounds;
};

static void foreachBrushInScene(const std::function<void(const BrushInfo&)>& visitor)
{
    GlobalSceneGraph().foreachNode([&](const scene::INodePtr& node) {
        Entity* entity = node->tryGetEntity();
        if (!entity) return true;

        std::string entityName = entity->getKeyValue("name");
        if (entityName.empty())
            entityName = entity->getKeyValue("classname");

        int brushIdx = 0;
        node->foreachNode([&](const scene::INodePtr& child) {
            if (child->getNodeType() == scene::INode::Type::Brush)
            {
                BrushInfo info;
                info.node = child;
                info.entityName = entityName;
                info.index = brushIdx;
                info.bounds = child->worldAABB();
                visitor(info);
            }
            ++brushIdx;
            return true;
        });
        return true;
    });
}

static bool findBrush(const std::string& entityName, int brushIndex, BrushInfo& out)
{
    bool found = false;
    foreachBrushInScene([&](const BrushInfo& info) {
        if (!found && info.entityName == entityName && info.index == brushIndex)
        {
            out = info;
            found = true;
        }
    });
    return found;
}

JsonValue McpPlugin::listBrushes(const JsonValue& params)
{
    std::string entityFilter;
    int limit = 500;

    if (params.has("entity")) entityFilter = params["entity"].getString();
    if (params.has("limit")) limit = params["limit"].getInt();

    JsonValue::Array brushes;
    int count = 0;

    foreachBrushInScene([&](const BrushInfo& info) {
        if (count >= limit) return;
        if (!entityFilter.empty() && info.entityName != entityFilter) return;

        auto* brush = Node_getIBrush(info.node);
        if (!brush) return;

        // Collect unique materials
        JsonValue::Array mats;
        for (std::size_t i = 0; i < brush->getNumFaces(); ++i)
        {
            std::string shader = brush->getFace(i).getShader();
            bool dup = false;
            for (auto& m : mats)
                if (m.getString() == shader) { dup = true; break; }
            if (!dup) mats.push_back(shader);
        }

        auto& b = info.bounds;
        auto minPt = b.origin - b.extents;
        auto maxPt = b.origin + b.extents;

        brushes.push_back(jsonObject({
            {"entity", info.entityName},
            {"index", info.index},
            {"min", jsonArray({minPt.x(), minPt.y(), minPt.z()})},
            {"max", jsonArray({maxPt.x(), maxPt.y(), maxPt.z()})},
            {"materials", JsonValue(std::move(mats))},
        }));
        ++count;
    });

    return JsonValue(std::move(brushes));
}

JsonValue McpPlugin::getBrushInfo(const JsonValue& params)
{
    std::string entityName = params["entity"].getString();
    int brushIndex = params["index"].getInt();

    BrushInfo info;
    if (!findBrush(entityName, brushIndex, info))
        throw std::runtime_error("Brush not found: " + entityName + ":" + std::to_string(brushIndex));

    auto* brush = Node_getIBrush(info.node);
    if (!brush)
        throw std::runtime_error("Failed to access brush");

    auto& b = info.bounds;
    auto minPt = b.origin - b.extents;
    auto maxPt = b.origin + b.extents;

    // Per-face info
    JsonValue::Array faces;
    for (std::size_t i = 0; i < brush->getNumFaces(); ++i)
    {
        auto& face = brush->getFace(i);
        auto& plane = face.getPlane3();

        faces.push_back(jsonObject({
            {"material", face.getShader()},
            {"normal", jsonArray({plane.normal().x(), plane.normal().y(), plane.normal().z()})},
            {"distance", plane.dist()},
        }));
    }

    return jsonObject({
        {"entity", entityName},
        {"index", brushIndex},
        {"min", jsonArray({minPt.x(), minPt.y(), minPt.z()})},
        {"max", jsonArray({maxPt.x(), maxPt.y(), maxPt.z()})},
        {"num_faces", static_cast<int>(brush->getNumFaces())},
        {"faces", JsonValue(std::move(faces))},
    });
}

JsonValue McpPlugin::setBrushMaterial(const JsonValue& params)
{
    std::string entityName = params["entity"].getString();
    int brushIndex = params["index"].getInt();
    std::string material = params["material"].getString();

    BrushInfo info;
    if (!findBrush(entityName, brushIndex, info))
        throw std::runtime_error("Brush not found: " + entityName + ":" + std::to_string(brushIndex));

    auto* brush = Node_getIBrush(info.node);
    if (!brush)
        throw std::runtime_error("Failed to access brush");

    auto& undoSystem = GlobalMapModule().getUndoSystem();
    undoSystem.start();

    if (params.has("face"))
    {
        int faceIdx = params["face"].getInt();
        if (faceIdx < 0 || faceIdx >= static_cast<int>(brush->getNumFaces()))
            throw std::runtime_error("Face index out of range");
        brush->getFace(faceIdx).setShader(material);
    }
    else
    {
        brush->setShader(material);
    }

    undoSystem.finish("Set brush material");

    return jsonObject({{"success", true}});
}

JsonValue McpPlugin::deleteBrush(const JsonValue& params)
{
    std::string entityName = params["entity"].getString();
    int brushIndex = params["index"].getInt();

    BrushInfo info;
    if (!findBrush(entityName, brushIndex, info))
        throw std::runtime_error("Brush not found: " + entityName + ":" + std::to_string(brushIndex));

    auto& undoSystem = GlobalMapModule().getUndoSystem();
    undoSystem.start();

    // Get parent and remove the brush node
    auto parent = info.node->getParent();
    if (parent)
        parent->removeChildNode(info.node);

    undoSystem.finish("Delete brush");

    return jsonObject({{"success", true}});
}

JsonValue McpPlugin::deleteBrushes(const JsonValue& params)
{
    std::string entityName = params["entity"].getString();

    // Collect indices to delete — supports "indices" array or "from"/"to" range
    std::vector<int> indices;
    if (params.has("indices") && params["indices"].isArray())
    {
        auto& arr = params["indices"];
        for (std::size_t i = 0; i < arr.size(); ++i)
            indices.push_back(static_cast<int>(arr[i].getNumber()));
    }
    else if (params.has("from") && params.has("to"))
    {
        int from = static_cast<int>(params["from"].getNumber());
        int to = static_cast<int>(params["to"].getNumber());
        for (int i = from; i <= to; ++i)
            indices.push_back(i);
    }
    else
    {
        throw std::runtime_error("Provide 'indices' array or 'from'/'to' range");
    }

    // Find all matching brush nodes first (indices shift during deletion)
    std::vector<scene::INodePtr> toDelete;
    for (int idx : indices)
    {
        BrushInfo info;
        if (findBrush(entityName, idx, info))
            toDelete.push_back(info.node);
    }

    if (toDelete.empty())
        throw std::runtime_error("No brushes found for the given indices");

    auto& undoSystem = GlobalMapModule().getUndoSystem();
    undoSystem.start();

    for (auto& node : toDelete)
    {
        auto parent = node->getParent();
        if (parent)
            parent->removeChildNode(node);
    }

    undoSystem.finish("Delete brushes (batch)");

    return jsonObject({
        {"success", true},
        {"deleted_count", static_cast<int>(toDelete.size())}
    });
}

JsonValue McpPlugin::convertBrushesToEntity(const JsonValue& params)
{
    std::string entityName = params.has("entity") ? params["entity"].getString() : "";
    std::string targetClass = params.has("classname") ? params["classname"].getString() : "func_static";

    // Collect brush indices
    std::vector<int> indices;
    if (params.has("indices") && params["indices"].isArray())
    {
        auto& arr = params["indices"];
        for (std::size_t i = 0; i < arr.size(); ++i)
            indices.push_back(static_cast<int>(arr[i].getNumber()));
    }
    else
    {
        throw std::runtime_error("'indices' array is required");
    }

    // Resolve source entity
    scene::INodePtr srcEntity;
    if (!entityName.empty())
    {
        srcEntity = findEntityByName(entityName);
        if (!srcEntity)
            throw std::runtime_error("Entity not found: " + entityName);
    }
    else
    {
        srcEntity = GlobalMapModule().findOrInsertWorldspawn();
    }

    // Resolve the entity name as findBrush expects it
    std::string srcName;
    Entity* srcEnt = srcEntity->tryGetEntity();
    if (srcEnt)
    {
        srcName = srcEnt->getKeyValue("name");
        if (srcName.empty())
            srcName = srcEnt->getKeyValue("classname");
    }

    // Find brush nodes by index
    std::vector<scene::INodePtr> brushNodes;
    for (int idx : indices)
    {
        BrushInfo info;
        if (findBrush(srcName, idx, info))
            brushNodes.push_back(info.node);
    }

    if (brushNodes.empty())
        throw std::runtime_error("No brushes found for the given indices");

    // Create the new entity
    auto eclass = GlobalEntityClassManager().findClass(targetClass);
    if (!eclass)
        throw std::runtime_error("Entity class not found: " + targetClass);

    auto& undoSystem = GlobalMapModule().getUndoSystem();
    undoSystem.start();

    auto newEntityNode = GlobalEntityModule().createEntity(eclass);

    // Set properties if provided
    if (params.has("properties") && params["properties"].isObject())
    {
        Entity* newEnt = newEntityNode->tryGetEntity();
        if (newEnt)
        {
            for (auto& [key, val] : params["properties"].getObject())
                newEnt->setKeyValue(key, val.getString());
        }
    }

    // Move brushes from source to new entity
    for (auto& brushNode : brushNodes)
        srcEntity->removeChildNode(brushNode);

    for (auto& brushNode : brushNodes)
        newEntityNode->addChildNode(brushNode);

    // Add new entity to scene
    auto root = GlobalSceneGraph().root();
    if (root)
        root->addChildNode(newEntityNode);

    undoSystem.finish("Convert brushes to entity");

    Entity* resultEnt = newEntityNode->tryGetEntity();
    std::string resultName = resultEnt ? resultEnt->getKeyValue("name") : "";

    return jsonObject({
        {"success", true},
        {"name", resultName},
        {"classname", targetClass},
        {"brush_count", static_cast<int>(brushNodes.size())}
    });
}

JsonValue McpPlugin::moveBrush(const JsonValue& params)
{
    std::string entityName = params["entity"].getString();
    int brushIndex = params["index"].getInt();

    auto& offsetArr = params["offset"];
    double ox = offsetArr[0].getNumber();
    double oy = offsetArr[1].getNumber();
    double oz = offsetArr[2].getNumber();

    BrushInfo info;
    if (!findBrush(entityName, brushIndex, info))
        throw std::runtime_error("Brush not found: " + entityName + ":" + std::to_string(brushIndex));

    auto* brush = Node_getIBrush(info.node);
    if (!brush)
        throw std::runtime_error("Failed to access brush");

    auto& undoSystem = GlobalMapModule().getUndoSystem();
    undoSystem.start();

    Matrix4 translation = Matrix4::getTranslation(Vector3(ox, oy, oz));
    for (std::size_t i = 0; i < brush->getNumFaces(); ++i)
    {
        brush->getFace(i).transform(translation);
        brush->getFace(i).freezeTransform();
    }

    undoSystem.finish("Move brush");

    auto newBounds = info.node->worldAABB();
    auto newMin = newBounds.origin - newBounds.extents;
    auto newMax = newBounds.origin + newBounds.extents;

    return jsonObject({
        {"success", true},
        {"min", jsonArray({newMin.x(), newMin.y(), newMin.z()})},
        {"max", jsonArray({newMax.x(), newMax.y(), newMax.z()})}
    });
}

JsonValue McpPlugin::resizeBrush(const JsonValue& params)
{
    std::string entityName = params["entity"].getString();
    int brushIndex = params["index"].getInt();

    BrushInfo info;
    if (!findBrush(entityName, brushIndex, info))
        throw std::runtime_error("Brush not found: " + entityName + ":" + std::to_string(brushIndex));

    auto& newMinArr = params["min"];
    auto& newMaxArr = params["max"];
    double newMinX = newMinArr[0].getNumber(), newMinY = newMinArr[1].getNumber(), newMinZ = newMinArr[2].getNumber();
    double newMaxX = newMaxArr[0].getNumber(), newMaxY = newMaxArr[1].getNumber(), newMaxZ = newMaxArr[2].getNumber();

    // Get current materials per face direction
    auto* oldBrush = Node_getIBrush(info.node);
    if (!oldBrush)
        throw std::runtime_error("Failed to access brush");

    std::map<std::string, std::string> faceMaterials;
    for (std::size_t i = 0; i < oldBrush->getNumFaces(); ++i)
    {
        auto& normal = oldBrush->getFace(i).getPlane3().normal();
        const char* faceName = nullptr;
        if      (normal.x() >  0.9) faceName = "east";
        else if (normal.x() < -0.9) faceName = "west";
        else if (normal.y() >  0.9) faceName = "north";
        else if (normal.y() < -0.9) faceName = "south";
        else if (normal.z() >  0.9) faceName = "top";
        else if (normal.z() < -0.9) faceName = "bottom";
        if (faceName)
            faceMaterials[faceName] = oldBrush->getFace(i).getShader();
    }

    auto& undoSystem = GlobalMapModule().getUndoSystem();
    undoSystem.start();

    // Remove old brush
    auto parent = info.node->getParent();
    if (parent)
        parent->removeChildNode(info.node);

    // Create replacement with preserved materials
    JsonValue::Object materials;
    for (auto& [face, mat] : faceMaterials)
        materials[face] = mat;

    auto spec = jsonObject({
        {"min", jsonArray({newMinX, newMinY, newMinZ})},
        {"max", jsonArray({newMaxX, newMaxY, newMaxZ})},
        {"materials", JsonValue(std::move(materials))}
    });
    auto result = createSingleBrush(spec, parent);

    undoSystem.finish("Resize brush");

    JsonValue::Object obj = result.getObject();
    obj["success"] = true;
    return JsonValue(std::move(obj));
}

// Spatial Queries

JsonValue McpPlugin::queryRegion(const JsonValue& params)
{
    auto& minArr = params["min"];
    auto& maxArr = params["max"];

    Vector3 qMin(minArr[0].getNumber(), minArr[1].getNumber(), minArr[2].getNumber());
    Vector3 qMax(maxArr[0].getNumber(), maxArr[1].getNumber(), maxArr[2].getNumber());

    AABB queryBox = AABB::createFromMinMax(qMin, qMax);

    JsonValue::Array entities;
    JsonValue::Array brushes;

    GlobalSceneGraph().foreachNode([&](const scene::INodePtr& node) {
        Entity* entity = node->tryGetEntity();
        if (!entity) return true;

        std::string entityName = entity->getKeyValue("name");
        if (entityName.empty())
            entityName = entity->getKeyValue("classname");

        // Check if the entity itself (point entity) is in the region
        auto entityAABB = node->worldAABB();
        if (entityAABB.isValid() && queryBox.intersects(entityAABB))
        {
            std::string origin = entity->getKeyValue("origin");
            if (!origin.empty())
            {
                entities.push_back(jsonObject({
                    {"name", entityName},
                    {"classname", entity->getKeyValue("classname")},
                    {"origin", origin},
                }));
            }
        }

        // Check child brushes
        int brushIdx = 0;
        node->foreachNode([&](const scene::INodePtr& child) {
            if (child->getNodeType() == scene::INode::Type::Brush)
            {
                auto brushAABB = child->worldAABB();
                if (brushAABB.isValid() && queryBox.intersects(brushAABB))
                {
                    auto bMin = brushAABB.origin - brushAABB.extents;
                    auto bMax = brushAABB.origin + brushAABB.extents;
                    brushes.push_back(jsonObject({
                        {"entity", entityName},
                        {"index", brushIdx},
                        {"min", jsonArray({bMin.x(), bMin.y(), bMin.z()})},
                        {"max", jsonArray({bMax.x(), bMax.y(), bMax.z()})},
                    }));
                }
            }
            ++brushIdx;
            return true;
        });

        return true;
    });

    return jsonObject({
        {"entities", JsonValue(std::move(entities))},
        {"brushes", JsonValue(std::move(brushes))},
    });
}

JsonValue McpPlugin::getSceneBounds(const JsonValue& params)
{
    AABB sceneBounds;

    GlobalSceneGraph().foreachNode([&](const scene::INodePtr& node) {
        if (node->getNodeType() == scene::INode::Type::Brush ||
            node->getNodeType() == scene::INode::Type::Patch)
        {
            auto aabb = node->worldAABB();
            if (aabb.isValid())
                sceneBounds.includeAABB(aabb);
        }
        return true;
    });

    if (!sceneBounds.isValid())
        return jsonObject({{"empty", true}});

    auto minPt = sceneBounds.origin - sceneBounds.extents;
    auto maxPt = sceneBounds.origin + sceneBounds.extents;

    return jsonObject({
        {"min", jsonArray({minPt.x(), minPt.y(), minPt.z()})},
        {"max", jsonArray({maxPt.x(), maxPt.y(), maxPt.z()})},
        {"origin", jsonArray({sceneBounds.origin.x(), sceneBounds.origin.y(), sceneBounds.origin.z()})},
    });
}

// Entity Manipulation

JsonValue McpPlugin::moveEntity(const JsonValue& params)
{
    std::string entityName = params["name"].getString();
    auto& originArr = params["origin"];
    std::string newOrigin = fmt::format("{} {} {}",
        originArr[0].getNumber(), originArr[1].getNumber(), originArr[2].getNumber());

    auto entityNode = findEntityByName(entityName);
    if (!entityNode)
        throw std::runtime_error("Entity not found: " + entityName);

    Entity* entity = entityNode->tryGetEntity();
    auto& undoSystem = GlobalMapModule().getUndoSystem();
    undoSystem.start();
    entity->setKeyValue("origin", newOrigin);
    undoSystem.finish("Move entity");

    return jsonObject({{"success", true}});
}

JsonValue McpPlugin::rotateEntity(const JsonValue& params)
{
    std::string entityName = params["name"].getString();

    auto entityNode = findEntityByName(entityName);
    if (!entityNode)
        throw std::runtime_error("Entity not found: " + entityName);

    Entity* entity = entityNode->tryGetEntity();
    auto& undoSystem = GlobalMapModule().getUndoSystem();
    undoSystem.start();

    // "angle" sets yaw in degrees (most common: doors, furniture facing direction)
    // Doom 3 convention: 0=east, 90=north, 180=west, 270=south
    if (params.has("angle"))
    {
        double angle = params["angle"].getNumber();
        entity->setKeyValue("angle", fmt::format("{}", angle));
    }
    // "rotation" sets the full 3x3 rotation matrix (9 values, row-major)
    if (params.has("rotation"))
    {
        auto& arr = params["rotation"];
        std::string rot = fmt::format("{} {} {} {} {} {} {} {} {}",
            arr[0].getNumber(), arr[1].getNumber(), arr[2].getNumber(),
            arr[3].getNumber(), arr[4].getNumber(), arr[5].getNumber(),
            arr[6].getNumber(), arr[7].getNumber(), arr[8].getNumber());
        entity->setKeyValue("rotation", rot);
    }

    undoSystem.finish("Rotate entity");
    return jsonObject({{"success", true}});
}

JsonValue McpPlugin::deleteEntity(const JsonValue& params)
{
    std::string entityName = params["name"].getString();

    auto target = findEntityByName(entityName);
    if (!target)
        throw std::runtime_error("Entity not found: " + entityName);

    auto& undoSystem = GlobalMapModule().getUndoSystem();
    undoSystem.start();

    auto parent = target->getParent();
    if (parent)
        parent->removeChildNode(target);

    undoSystem.finish("Delete entity");

    return jsonObject({{"success", true}});
}

// Measurement

// Helper: resolve a target to its AABB. Target can be:
//   {"entity": "name"} - entity bounds
//   {"entity": "name", "brush_index": N} - specific brush bounds
//   {"point": [x,y,z]} - a point (zero-size AABB)
static bool resolveTarget(const JsonValue& target, AABB& out)
{
    if (target.has("point"))
    {
        auto& p = target["point"];
        out = AABB(Vector3(p[0].getNumber(), p[1].getNumber(), p[2].getNumber()), Vector3(0, 0, 0));
        return true;
    }

    if (!target.has("entity"))
        return false;

    std::string entityName = target["entity"].getString();
    bool hasBrushIndex = target.has("brush_index");
    int brushIndex = hasBrushIndex ? target["brush_index"].getInt() : -1;

    bool found = false;
    GlobalSceneGraph().foreachNode([&](const scene::INodePtr& node) {
        if (found) return false;
        Entity* entity = node->tryGetEntity();
        if (!entity) return true;

        std::string name = entity->getKeyValue("name");
        if (name.empty()) name = entity->getKeyValue("classname");
        if (name != entityName) return true;

        if (hasBrushIndex)
        {
            int idx = 0;
            node->foreachNode([&](const scene::INodePtr& child) {
                if (found) return false;
                if (child->getNodeType() == scene::INode::Type::Brush && idx == brushIndex)
                {
                    out = child->worldAABB();
                    found = true;
                    return false;
                }
                ++idx;
                return true;
            });
        }
        else
        {
            out = node->worldAABB();
            found = true;
        }
        return !found;
    });

    return found;
}

JsonValue McpPlugin::measureDistance(const JsonValue& params)
{
    AABB aabbA, aabbB;

    if (!resolveTarget(params["from"], aabbA))
        throw std::runtime_error("Could not resolve 'from' target");
    if (!resolveTarget(params["to"], aabbB))
        throw std::runtime_error("Could not resolve 'to' target");

    Vector3 centerA = aabbA.origin;
    Vector3 centerB = aabbB.origin;
    Vector3 delta = centerB - centerA;

    double dist = delta.getLength();

    // Nearest-surface distance (gap between bounding boxes)
    // For each axis: gap = max(0, |delta_i| - extA_i - extB_i)
    double gapX = std::max(0.0, std::abs(delta.x()) - aabbA.extents.x() - aabbB.extents.x());
    double gapY = std::max(0.0, std::abs(delta.y()) - aabbA.extents.y() - aabbB.extents.y());
    double gapZ = std::max(0.0, std::abs(delta.z()) - aabbA.extents.z() - aabbB.extents.z());
    double surfaceDist = Vector3(gapX, gapY, gapZ).getLength();

    bool overlaps = (gapX == 0 && gapY == 0 && gapZ == 0) &&
        (std::abs(delta.x()) < aabbA.extents.x() + aabbB.extents.x()) &&
        (std::abs(delta.y()) < aabbA.extents.y() + aabbB.extents.y()) &&
        (std::abs(delta.z()) < aabbA.extents.z() + aabbB.extents.z());

    return jsonObject({
        {"center_distance", dist},
        {"surface_distance", surfaceDist},
        {"delta", jsonArray({delta.x(), delta.y(), delta.z()})},
        {"overlaps", overlaps},
    });
}

JsonValue McpPlugin::getGameConfig(const JsonValue& params)
{
    return jsonObject({
        {"player", jsonObject({
            {"width", 32},
            {"height_stand", 74},
            {"height_crouch", 38},
            {"eye_height_stand", 68},
            {"eye_height_crouch", 32},
            {"max_step_up", 16},
            {"jump_height", 48},
        })},
        {"movement", jsonObject({
            {"run_speed", 220},
            {"walk_speed", 140},
            {"crouch_speed", 80},
        })},
        {"reference_sizes", jsonObject({
            {"standard_door_width", 64},
            {"standard_door_height", 96},
            {"standard_hallway_width_min", 96},
            {"standard_hallway_width_max", 128},
            {"standard_room_height_min", 96},
            {"standard_room_height_max", 128},
            {"min_grid_size", 8},
        })},
        {"units", "1 unit = 1 inch = 2.54cm. 1 foot = 12 units."},
    });
}

// Special Materials Reference

JsonValue McpPlugin::listSpecialMaterials(const JsonValue& params)
{
    // Static reference list of special/utility textures for Doom 3 / The Dark Mod
    JsonValue::Array materials;

    auto add = [&](const std::string& name, const std::string& path,
                    const std::string& category, const std::string& description)
    {
        materials.push_back(JsonValue(JsonValue::Object{
            {"name", name},
            {"path", path},
            {"category", category},
            {"description", description},
        }));
    };

    // Optimization textures
    add("Caulk", "textures/common/caulk", "optimization",
        "Invisible texture that blocks nothing. Use on faces the player will never see (backs of brushes, hidden surfaces) to reduce draw calls.");
    add("Nodraw", "textures/common/nodraw", "optimization",
        "Non-rendered, non-solid surface. Use on faces that should not be drawn and have no collision.");
    add("Visportal", "textures/editor/visportal", "optimization",
        "Visportal surface. Place on a brush that exactly fills a doorway/opening between areas. The brush must be a func_static and seal the opening completely. Splits the map into vis-areas for rendering optimization.");

    // Clip textures
    add("Clip", "textures/common/clip", "collision",
        "Invisible wall that blocks player movement but not AI or projectiles. Use to smooth out player collision around complex geometry.");
    add("Monster Clip", "textures/common/monster_clip", "collision",
        "Invisible wall that blocks AI movement. Use to prevent monsters from going into areas they shouldn't reach.");
    add("Weapon Clip", "textures/common/weapon_clip", "collision",
        "Invisible surface that blocks projectiles but not movement. Useful for windows/grates where you can see but not shoot through.");

    // Trigger textures
    add("Trigger", "textures/common/trigger", "trigger",
        "Texture for trigger brushes. Apply to all faces of a brush entity (trigger_once, trigger_multiple, etc.).");

    // Lighting textures
    add("Shadow", "textures/common/shadow", "lighting",
        "Casts shadows but is otherwise invisible. Use for shadow-only geometry.");
    add("Noshadow", "textures/common/noshadow", "lighting",
        "Surface that does not cast shadows. Apply to faces where shadow artifacts occur.");

    std::string filter;
    if (params.has("filter")) filter = params["filter"].getString();
    std::string category;
    if (params.has("category")) category = params["category"].getString();

    // Filter results
    JsonValue::Array filtered;
    for (auto& mat : materials)
    {
        if (!category.empty() && mat["category"].getString() != category)
            continue;
        if (!filter.empty())
        {
            auto name = mat["name"].getString();
            auto path = mat["path"].getString();
            auto desc = mat["description"].getString();
            // Case-insensitive search
            std::string filterLower = filter;
            std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
            std::string nameLower = name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            std::string pathLower = path;
            std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);
            std::string descLower = desc;
            std::transform(descLower.begin(), descLower.end(), descLower.begin(), ::tolower);

            if (nameLower.find(filterLower) == std::string::npos &&
                pathLower.find(filterLower) == std::string::npos &&
                descLower.find(filterLower) == std::string::npos)
                continue;
        }
        filtered.push_back(mat);
    }

    return JsonValue(JsonValue::Object{
        {"materials", JsonValue(std::move(filtered))},
        {"categories", JsonValue(JsonValue::Array{
            "optimization", "collision", "trigger", "lighting"
        })},
    });
}

// Command Discovery

JsonValue McpPlugin::listCommands(const JsonValue& params)
{
    std::string filter;
    if (params.has("filter")) filter = params["filter"].getString();

    JsonValue::Array commands;

    GlobalCommandSystem().foreachCommand([&](const std::string& name) {
        if (!filter.empty())
        {
            // Case-insensitive search
            std::string nameLower = name;
            std::string filterLower = filter;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
            if (nameLower.find(filterLower) == std::string::npos) return;
        }
        commands.push_back(name);
    });

    return JsonValue(std::move(commands));
}

// Asset Queries

static const char* surfaceTypeName(Material::SurfaceType type)
{
    switch (type)
    {
        case Material::SURFTYPE_METAL:    return "metal";
        case Material::SURFTYPE_STONE:    return "stone";
        case Material::SURFTYPE_FLESH:    return "flesh";
        case Material::SURFTYPE_WOOD:     return "wood";
        case Material::SURFTYPE_CARDBOARD:return "cardboard";
        case Material::SURFTYPE_LIQUID:   return "liquid";
        case Material::SURFTYPE_GLASS:    return "glass";
        case Material::SURFTYPE_PLASTIC:  return "plastic";
        case Material::SURFTYPE_RICOCHET: return "ricochet";
        default:                          return "default";
    }
}

static const char* coverageName(Material::Coverage cov)
{
    switch (cov)
    {
        case Material::MC_OPAQUE:      return "opaque";
        case Material::MC_PERFORATED:  return "perforated";
        case Material::MC_TRANSLUCENT: return "translucent";
        default:                       return "undetermined";
    }
}

JsonValue McpPlugin::listMaterials(const JsonValue& params)
{
    std::string filter;
    std::string prefix;
    std::string surfaceTypeFilter;
    bool details = false;
    int limit = 100;

    if (params.has("filter")) filter = params["filter"].getString();
    if (params.has("prefix")) prefix = params["prefix"].getString();
    if (params.has("surface_type")) surfaceTypeFilter = params["surface_type"].getString();
    if (params.has("details")) details = params["details"].getBool();
    if (params.has("limit")) limit = params["limit"].getInt();

    // Default to textures/ prefix when no filter or prefix is specified,
    // since that's the primary use case for level designers
    if (filter.empty() && prefix.empty() && surfaceTypeFilter.empty())
        prefix = "textures/";

    // surface_type filter implies details
    if (!surfaceTypeFilter.empty()) details = true;

    JsonValue::Array materials;
    int count = 0;

    // Use foreachShaderName which iterates ALL declared materials from .mtr files,
    // not just the instantiated cache (foreachMaterial only returns used/active ones)
    GlobalMaterialManager().foreachShaderName([&](const std::string& name) {
        if (count >= limit) return;

        // Prefix filter (case-insensitive starts-with)
        if (!prefix.empty())
        {
            if (name.size() < prefix.size()) return;
            bool match = std::equal(prefix.begin(), prefix.end(), name.begin(),
                [](char a, char b) { return ::tolower(a) == ::tolower(b); });
            if (!match) return;
        }

        // Substring filter
        if (!filter.empty() && name.find(filter) == std::string::npos) return;

        if (details)
        {
            auto mat = GlobalMaterialManager().getMaterial(name);
            std::string surfType = surfaceTypeName(mat->getSurfaceType());

            // Surface type filter
            if (!surfaceTypeFilter.empty() && surfType != surfaceTypeFilter) return;

            materials.push_back(jsonObject({
                {"name", name},
                {"surface_type", surfType},
                {"coverage", coverageName(mat->getCoverage())},
                {"description", mat->getDescription()},
            }));
        }
        else
        {
            // Name-only listing (fast path, no material instantiation)
            materials.push_back(jsonObject({
                {"name", name},
            }));
        }
        ++count;
    });

    return JsonValue(std::move(materials));
}

JsonValue McpPlugin::getMaterialInfo(const JsonValue& params)
{
    std::string name = params["name"].getString();

    if (!GlobalMaterialManager().materialExists(name))
        throw std::runtime_error("Material not found: " + name);

    auto mat = GlobalMaterialManager().getMaterial(name);

    int flags = mat->getMaterialFlags();
    int surfFlags = mat->getSurfaceFlags();

    // Collect layer info
    JsonValue::Array layers;
    mat->foreachLayer([&](const IShaderLayer::Ptr& layer) {
        const char* typeName = "blend";
        switch (layer->getType())
        {
            case IShaderLayer::BUMP: typeName = "bump"; break;
            case IShaderLayer::DIFFUSE: typeName = "diffuse"; break;
            case IShaderLayer::SPECULAR: typeName = "specular"; break;
            default: break;
        }

        std::string mapImage = layer->getMapImageFilename();

        layers.push_back(jsonObject({
            {"type", typeName},
            {"map_image", mapImage},
        }));
        return true;
    });

    return jsonObject({
        {"name", name},
        {"description", mat->getDescription()},
        {"shader_file", std::string(mat->getShaderFileName() ? mat->getShaderFileName() : "")},
        {"surface_type", surfaceTypeName(mat->getSurfaceType())},
        {"coverage", coverageName(mat->getCoverage())},
        {"is_drawn", mat->isDrawn()},
        {"casts_shadow", mat->surfaceCastsShadow()},
        {"is_ambient_light", mat->isAmbientLight()},
        {"is_blend_light", mat->isBlendLight()},
        {"is_fog_light", mat->isFogLight()},
        {"noshadows", (flags & Material::FLAG_NOSHADOWS) != 0},
        {"translucent", (flags & Material::FLAG_TRANSLUCENT) != 0},
        {"nonsolid", (surfFlags & Material::SURF_NONSOLID) != 0},
        {"playerclip", (surfFlags & Material::SURF_PLAYERCLIP) != 0},
        {"areaportal", (surfFlags & Material::SURF_AREAPORTAL) != 0},
        {"layers", JsonValue(std::move(layers))},
    });
}

JsonValue McpPlugin::getEntityClassInfo(const JsonValue& params)
{
    std::string className = params["name"].getString();

    auto eclass = GlobalEntityClassManager().findClass(className);
    if (!eclass)
        throw std::runtime_error("Entity class not found: " + className);

    // Class type
    const char* classType = "generic";
    switch (eclass->getClassType())
    {
        case scene::EntityClass::Type::StaticGeometry:   classType = "static_geometry"; break;
        case scene::EntityClass::Type::EntityClassModel:  classType = "entity_class_model"; break;
        case scene::EntityClass::Type::Light:             classType = "light"; break;
        case scene::EntityClass::Type::Speaker:           classType = "speaker"; break;
        default: break;
    }

    // Parent
    std::string parentName;
    auto parent = eclass->getParent();
    if (parent)
        parentName = parent->getDeclName();

    // Colour
    auto& colour = eclass->getColour();

    // Attributes (spawnargs)
    JsonValue::Array attrs;
    eclass->forEachAttribute([&](const EntityClassAttribute& attr, bool inherited) {
        attrs.push_back(jsonObject({
            {"name", attr.getName()},
            {"type", attr.getType()},
            {"value", attr.getValue()},
            {"description", attr.getDescription()},
            {"inherited", inherited},
        }));
    }, false); // don't include editor_ keys in the spawnarg list

    // Also grab editor keys that define model, usage, etc.
    std::string model = eclass->getAttributeValue("model");
    std::string editorUsage = eclass->getAttributeValue("editor_usage");

    return jsonObject({
        {"name", className},
        {"class_type", classType},
        {"parent", parentName},
        {"is_light", eclass->isLight()},
        {"is_fixed_size", eclass->isFixedSize()},
        {"color", jsonArray({colour.x(), colour.y(), colour.z()})},
        {"model", model},
        {"usage", editorUsage},
        {"attributes", JsonValue(std::move(attrs))},
    });
}

JsonValue McpPlugin::listModels(const JsonValue& params)
{
    std::string filter;
    int limit = 100;

    if (params.has("filter")) filter = params["filter"].getString();
    if (params.has("limit")) limit = params["limit"].getInt();

    JsonValue::Array models;
    int count = 0;

    // Search for common model formats in the VFS
    auto visitor = [&](const std::string& extension) {
        GlobalFileSystem().forEachFile("models/", extension, [&](const vfs::FileInfo& info) {
            if (count >= limit) return;

            std::string path = info.fullPath();

            if (!filter.empty() && path.find(filter) == std::string::npos) return;

            models.push_back(jsonObject({
                {"path", path},
            }));
            ++count;
        }, 99);
    };

    visitor("lwo");
    visitor("ase");
    visitor("md5mesh");

    return JsonValue(std::move(models));
}

JsonValue McpPlugin::listSounds(const JsonValue& params)
{
    std::string filter;
    int limit = 100;

    if (params.has("filter")) filter = params["filter"].getString();
    if (params.has("limit")) limit = params["limit"].getInt();

    JsonValue::Array sounds;
    int count = 0;

    GlobalSoundManager().forEachShader([&](const ISoundShader::Ptr& shader) {
        if (count >= limit) return;

        std::string name = shader->getDeclName();

        if (!filter.empty() && name.find(filter) == std::string::npos) return;

        auto radii = shader->getRadii();

        sounds.push_back(jsonObject({
            {"name", name},
            {"folder", shader->getDisplayFolder()},
            {"min_radius", radii.getMin()},
            {"max_radius", radii.getMax()},
        }));
        ++count;
    });

    return JsonValue(std::move(sounds));
}

JsonValue McpPlugin::listSkins(const JsonValue& params)
{
    std::string modelFilter;
    int limit = 100;

    if (params.has("model")) modelFilter = params["model"].getString();
    if (params.has("limit")) limit = params["limit"].getInt();

    JsonValue::Array skins;
    int count = 0;

    if (!modelFilter.empty())
    {
        // Return skins for a specific model
        auto& skinList = GlobalModelSkinCache().getSkinsForModel(modelFilter);
        for (auto& skinName : skinList)
        {
            if (count >= limit) break;
            skins.push_back(jsonObject({{"name", skinName}}));
            ++count;
        }
    }
    else
    {
        auto& allSkins = GlobalModelSkinCache().getAllSkins();
        for (auto& skinName : allSkins)
        {
            if (count >= limit) break;
            skins.push_back(jsonObject({{"name", skinName}}));
            ++count;
        }
    }

    return JsonValue(std::move(skins));
}

JsonValue McpPlugin::listParticles(const JsonValue& params)
{
    std::string filter;
    int limit = 100;

    if (params.has("filter")) filter = params["filter"].getString();
    if (params.has("limit")) limit = params["limit"].getInt();

    JsonValue::Array particles;
    int count = 0;

    GlobalParticlesManager().forEachParticleDef([&](particles::IParticleDef& def) {
        if (count >= limit) return;

        std::string name = def.getDeclName();

        if (!filter.empty() && name.find(filter) == std::string::npos) return;

        particles.push_back(jsonObject({
            {"name", name},
            {"num_stages", static_cast<int>(def.getNumStages())},
        }));
        ++count;
    });

    return JsonValue(std::move(particles));
}

// Prefab Operations

JsonValue McpPlugin::listPrefabs(const JsonValue& params)
{
    std::string filter;
    int limit = 100;

    if (params.has("filter")) filter = params["filter"].getString();
    if (params.has("limit")) limit = static_cast<int>(params["limit"].getNumber());

    JsonValue::Array prefabs;
    int count = 0;

    // Search for .pfb and .pfbx files in the prefabs/ directory
    auto searchExtension = [&](const std::string& ext) {
        GlobalFileSystem().forEachFile("prefabs/", ext, [&](const vfs::FileInfo& info) {
            if (count >= limit) return;

            std::string name = info.name;
            std::string fullPath = info.fullPath();

            if (!filter.empty() && name.find(filter) == std::string::npos
                && fullPath.find(filter) == std::string::npos)
                return;

            prefabs.push_back(jsonObject({
                {"name", name},
                {"path", fullPath}
            }));
            ++count;
        }, 99); // deep recursion to find nested prefabs
    };

    searchExtension("pfb");
    searchExtension("pfbx");

    // Also search for .map files in prefabs/ since some mods use those
    searchExtension("map");

    return jsonObject({
        {"count", static_cast<int>(prefabs.size())},
        {"prefabs", JsonValue(std::move(prefabs))}
    });
}

JsonValue McpPlugin::getPrefabInfo(const JsonValue& params)
{
    std::string path = params["path"].getString();

    auto resource = GlobalMapResourceManager().createFromPath(path);
    if (!resource)
        throw std::runtime_error("Could not load prefab: " + path);

    resource->load();

    auto rootNode = resource->getRootNode();
    if (!rootNode)
        throw std::runtime_error("Failed to parse prefab: " + path);

    // Get description from worldspawn
    WorldspawnArgFinder descFinder("editor_description");
    rootNode->traverse(descFinder);
    std::string description = descFinder.getFoundValue();

    // Count entities and brushes
    int entityCount = 0, brushCount = 0, patchCount = 0;
    rootNode->foreachNode([&](const scene::INodePtr& node) {
        switch (node->getNodeType())
        {
            case scene::INode::Type::Entity: ++entityCount; break;
            case scene::INode::Type::Brush: ++brushCount; break;
            case scene::INode::Type::Patch: ++patchCount; break;
            default: break;
        }
        return true;
    });

    // Calculate bounds
    scene::PrefabBoundsAccumulator boundsCalc;
    rootNode->traverse(boundsCalc);
    auto& bounds = boundsCalc.getBounds();

    auto minPt = bounds.origin - bounds.extents;
    auto maxPt = bounds.origin + bounds.extents;

    // Get worldspawn origin if set
    WorldspawnArgFinder originFinder("origin");
    rootNode->traverse(originFinder);
    std::string originStr = originFinder.getFoundValue();

    // Collect entity classes and models/materials used
    JsonValue::Array entityClasses;
    std::set<std::string> modelSet, materialSet;
    rootNode->foreachNode([&](const scene::INodePtr& node) {
        auto* ent = node->tryGetEntity();
        if (ent)
        {
            if (!ent->isWorldspawn())
            {
                std::string cls = ent->getKeyValue("classname");
                std::string name = ent->getKeyValue("name");
                std::string model = ent->getKeyValue("model");
                entityClasses.push_back(jsonObject({
                    {"classname", cls},
                    {"name", name}
                }));
                if (!model.empty() && model != name)
                    modelSet.insert(model);
            }
        }
        // Collect materials from brushes
        if (node->getNodeType() == scene::INode::Type::Brush)
        {
            auto* brush = Node_getIBrush(node);
            if (brush)
            {
                for (std::size_t i = 0; i < brush->getNumFaces(); ++i)
                {
                    std::string shader = brush->getFace(i).getShader();
                    if (!shader.empty() && shader.find("common/") == std::string::npos)
                        materialSet.insert(shader);
                }
            }
        }
        return true;
    });

    // Auto-generate description if empty
    if (description.empty())
    {
        std::string autoDesc;
        if (brushCount > 0)
            autoDesc += fmt::format("{} brushes", brushCount);
        if (patchCount > 0)
            autoDesc += fmt::format("{}{}patches", autoDesc.empty() ? "" : ", ", patchCount);
        if (!entityClasses.empty())
            autoDesc += fmt::format("{}{} entities", autoDesc.empty() ? "" : ", ", entityClasses.size());
        if (!modelSet.empty())
        {
            autoDesc += ". Models: ";
            int n = 0;
            for (auto& m : modelSet)
            {
                if (n++ > 0) autoDesc += ", ";
                if (n > 5) { autoDesc += "..."; break; }
                autoDesc += m;
            }
        }
        description = autoDesc;
    }

    // Collect materials array
    JsonValue::Array materials;
    for (auto& m : materialSet)
        materials.push_back(m);

    // Collect models array
    JsonValue::Array models;
    for (auto& m : modelSet)
        models.push_back(m);

    return jsonObject({
        {"path", path},
        {"description", description},
        {"entity_count", entityCount},
        {"brush_count", brushCount},
        {"patch_count", patchCount},
        {"bounds_min", jsonArray({minPt.x(), minPt.y(), minPt.z()})},
        {"bounds_max", jsonArray({maxPt.x(), maxPt.y(), maxPt.z()})},
        {"bounds_size", jsonArray({maxPt.x() - minPt.x(), maxPt.y() - minPt.y(), maxPt.z() - minPt.z()})},
        {"origin", originStr},
        {"entities", JsonValue(std::move(entityClasses))},
        {"models", JsonValue(std::move(models))},
        {"materials", JsonValue(std::move(materials))}
    });
}

JsonValue McpPlugin::insertPrefab(const JsonValue& params)
{
    std::string path = params["path"].getString();

    double x = 0, y = 0, z = 0;
    if (params.has("origin"))
    {
        auto& origin = params["origin"];
        x = origin[0].getNumber();
        y = origin[1].getNumber();
        z = origin[2].getNumber();
    }

    bool insertAsGroup = params.has("as_group") ? params["as_group"].getBool() : true;
    bool recalcOrigin = params.has("recalculate_origin") ? params["recalculate_origin"].getBool() : true;

    // Build typed argument list for LoadPrefabAt command
    cmd::ArgumentList args;
    args.push_back(path);
    args.push_back(Vector3(x, y, z));
    args.push_back(insertAsGroup ? 1 : 0);
    args.push_back(recalcOrigin ? 1 : 0);

    GlobalCommandSystem().executeCommand(LOAD_PREFAB_AT_CMD, args);

    // Post-insertion rotation: the inserted prefab items are now selected
    if (params.has("angle"))
    {
        double yaw = params["angle"].getNumber();
        cmd::ArgumentList rotArgs;
        rotArgs.push_back(Vector3(0, 0, yaw));
        GlobalCommandSystem().executeCommand("RotateSelectedEulerXYZ", rotArgs);
    }
    else if (params.has("angles"))
    {
        auto& arr = params["angles"];
        cmd::ArgumentList rotArgs;
        rotArgs.push_back(Vector3(arr[0].getNumber(), arr[1].getNumber(), arr[2].getNumber()));
        GlobalCommandSystem().executeCommand("RotateSelectedEulerXYZ", rotArgs);
    }

    return jsonObject({
        {"success", true},
        {"path", path},
        {"origin", jsonArray({x, y, z})},
        {"as_group", insertAsGroup},
        {"recalculate_origin", recalcOrigin}
    });
}

// Shortcut/Help Operations

JsonValue McpPlugin::listShortcuts(const JsonValue& params)
{
    std::string filter;
    int limit = 500;

    if (params.has("filter")) filter = params["filter"].getString();
    if (params.has("limit")) limit = static_cast<int>(params["limit"].getNumber());

    std::string filterLower = filter;
    std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);

    JsonValue::Array shortcuts;
    int count = 0;

    class ShortcutVisitor : public IEventVisitor
    {
    public:
        const std::string& filterLower;
        int limit;
        int& count;
        JsonValue::Array& shortcuts;

        ShortcutVisitor(const std::string& f, int l, int& c, JsonValue::Array& s)
            : filterLower(f), limit(l), count(c), shortcuts(s) {}

        void visit(const std::string& eventName, const IAccelerator& accel) override
        {
            if (count >= limit) return;

            std::string shortcutStr = accel.getString(true);
            if (shortcutStr.empty()) return; // skip unbound commands

            if (!filterLower.empty())
            {
                std::string nameLower = eventName;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                std::string keyLower = shortcutStr;
                std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(), ::tolower);

                if (nameLower.find(filterLower) == std::string::npos &&
                    keyLower.find(filterLower) == std::string::npos)
                    return;
            }

            shortcuts.push_back(jsonObject({
                {"command", eventName},
                {"shortcut", shortcutStr}
            }));
            ++count;
        }
    };

    ShortcutVisitor visitor(filterLower, limit, count, shortcuts);
    GlobalEventManager().foreachEvent(visitor);

    return jsonObject({
        {"count", static_cast<int>(shortcuts.size())},
        {"shortcuts", JsonValue(std::move(shortcuts))}
    });
}

JsonValue McpPlugin::getCommandShortcut(const JsonValue& params)
{
    std::string command = params["command"].getString();

    auto accel = GlobalEventManager().findAcceleratorForEvent(command);

    JsonValue::Object result;
    result["command"] = command;

    if (accel)
    {
        result["shortcut"] = accel->getString(true);
        result["bound"] = true;
    }
    else
    {
        result["shortcut"] = std::string("");
        result["bound"] = false;
    }

    // Also check if the event exists at all
    auto event = GlobalEventManager().findEvent(command);
    result["exists"] = event != nullptr;

    return JsonValue(std::move(result));
}

namespace
{

std::string base64Encode(const std::vector<unsigned char>& data)
{
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);

    for (std::size_t i = 0; i < data.size(); i += 3)
    {
        unsigned int n = static_cast<unsigned int>(data[i]) << 16;
        if (i + 1 < data.size()) n |= static_cast<unsigned int>(data[i + 1]) << 8;
        if (i + 2 < data.size()) n |= static_cast<unsigned int>(data[i + 2]);

        out.push_back(table[(n >> 18) & 0x3F]);
        out.push_back(table[(n >> 12) & 0x3F]);
        out.push_back(i + 1 < data.size() ? table[(n >> 6) & 0x3F] : '=');
        out.push_back(i + 2 < data.size() ? table[n & 0x3F] : '=');
    }

    return out;
}

} // anonymous namespace

JsonValue McpPlugin::captureView(const JsonValue& params)
{
    std::string viewType = params.has("view") ? params["view"].getString() : "camera";
    int maxWidth = params.has("max_width") ? static_cast<int>(params["max_width"].getNumber()) : 800;

    std::string tmpFile = "/tmp/hellforge_capture_" + viewType + ".png";

    if (viewType == "camera")
    {
        cmd::ArgumentList args;
        args.push_back(tmpFile);
        args.push_back(maxWidth);
        GlobalCommandSystem().executeCommand("CaptureCamera", args);
    }
    else if (viewType == "xy" || viewType == "xz" || viewType == "yz")
    {
        cmd::ArgumentList args;
        args.push_back(viewType);
        args.push_back(tmpFile);
        args.push_back(maxWidth);
        GlobalCommandSystem().executeCommand("CaptureOrthoView", args);
    }
    else
    {
        throw std::runtime_error("Invalid view type: " + viewType + ". Use 'camera', 'xy', 'xz', or 'yz'.");
    }

    // Read the PNG file
    std::ifstream file(tmpFile, std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Failed to capture view");

    std::vector<unsigned char> fileData(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    file.close();
    std::remove(tmpFile.c_str());

    if (fileData.empty())
        throw std::runtime_error("Captured image is empty");

    return jsonObject({
        {"image", base64Encode(fileData)},
        {"format", "png"},
        {"view", viewType}
    });
}

} // namespace mcp

extern "C" void DARKRADIANT_DLLEXPORT RegisterModule(IModuleRegistry& registry)
{
    module::performDefaultInitialisation(registry);
    registry.registerModule(std::make_shared<mcp::McpPlugin>());
}
