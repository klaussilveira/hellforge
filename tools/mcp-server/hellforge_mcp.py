#!/usr/bin/env python3
"""
HellForge MCP Server

Communicates with the HellForge MCP plugin via TCP on localhost:13646.
"""

import json
import socket
from typing import Any

import anyio
from mcp.server import Server
from mcp.server.stdio import stdio_server
from mcp.types import Tool, TextContent, ImageContent

HELLFORGE_HOST = "127.0.0.1"
HELLFORGE_PORT = 13646


class HellForgeConnection:
    """Manages TCP connection to the HellForge MCP plugin."""

    def __init__(self, host: str = HELLFORGE_HOST, port: int = HELLFORGE_PORT):
        self.host = host
        self.port = port
        self._sock: socket.socket | None = None
        self._request_id = 0

    def connect(self) -> bool:
        try:
            self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self._sock.settimeout(10.0)
            self._sock.connect((self.host, self.port))
            return True
        except (ConnectionRefusedError, OSError):
            self._sock = None
            return False

    def disconnect(self):
        if self._sock:
            try:
                self._sock.close()
            except OSError:
                pass
            self._sock = None

    @property
    def connected(self) -> bool:
        return self._sock is not None

    def call(self, method: str, params: dict | None = None) -> Any:
        """Send a JSON-RPC request and return the result."""
        if not self._sock:
            if not self.connect():
                raise ConnectionError(
                    "Cannot connect to HellForge. Is the editor running with the MCP plugin?"
                )

        self._request_id += 1
        request = {
            "jsonrpc": "2.0",
            "method": method,
            "params": params or {},
            "id": self._request_id,
        }

        try:
            data = json.dumps(request) + "\n"
            self._sock.sendall(data.encode("utf-8"))

            response_data = b""
            while b"\n" not in response_data:
                chunk = self._sock.recv(65536)
                if not chunk:
                    raise ConnectionError("Connection closed by HellForge")
                response_data += chunk

            line = response_data.split(b"\n")[0]
            response = json.loads(line.decode("utf-8"))

            if "error" in response:
                err = response["error"]
                raise RuntimeError(f"HellForge error: {err.get('message', str(err))}")

            return response.get("result")

        except (BrokenPipeError, ConnectionResetError, OSError) as e:
            self.disconnect()
            raise ConnectionError(f"Lost connection to HellForge: {e}")


conn = HellForgeConnection()
app = Server("hellforge")

TOOLS = [
    Tool(name="get_map_info", description="Get information about the currently loaded map, including name, modification status, and counts of entities/brushes/patches.", inputSchema={"type": "object", "properties": {}}),
    Tool(name="new_map", description="Create a new empty map in the editor.", inputSchema={"type": "object", "properties": {}}),
    Tool(name="open_map", description="Open a .map file in the editor.", inputSchema={"type": "object", "properties": {"path": {"type": "string", "description": "Absolute path to the .map file"}}, "required": ["path"]}),
    Tool(name="save_map", description="Save the current map. Optionally save to a new path.", inputSchema={"type": "object", "properties": {"path": {"type": "string", "description": "Optional: path to save as"}}}),
    Tool(name="create_entity", description="Create a new entity in the map. Use list_entity_classes to discover available types.", inputSchema={"type": "object", "properties": {"classname": {"type": "string", "description": "Entity class (e.g. light, info_player_start, func_static)"}, "origin": {"type": "array", "items": {"type": "number"}, "description": "Position [x, y, z]"}, "properties": {"type": "object", "description": "Additional key-value properties"}}, "required": ["classname"]}),
    Tool(name="set_entity_property", description="Set a key-value property on an existing entity.", inputSchema={"type": "object", "properties": {"name": {"type": "string"}, "key": {"type": "string"}, "value": {"type": "string"}}, "required": ["name", "key", "value"]}),
    Tool(name="get_entity_properties", description="Get all key-value properties of a named entity.", inputSchema={"type": "object", "properties": {"name": {"type": "string"}}, "required": ["name"]}),
    Tool(name="list_entities", description="List all entities in the current map with their classname, name, origin, and brush count.", inputSchema={"type": "object", "properties": {}}),
    Tool(name="list_entity_classes", description="List available entity classes (from .def files). Use filter to search.", inputSchema={"type": "object", "properties": {"filter": {"type": "string", "description": "Text filter"}, "limit": {"type": "integer", "description": "Max results (default 100)"}}}),
    Tool(name="create_brush", description="Create an axis-aligned brush (rectangular solid). Returns {success, index, min, max, material}. Coordinates in Doom 3 units (1 unit ~ 1 inch, 64 ~ player width). Added to worldspawn by default. Supports per-face materials via 'materials' object with keys: top, bottom, north (+Y), south (-Y), east (+X), west (-X).", inputSchema={"type": "object", "properties": {"min": {"type": "array", "items": {"type": "number"}, "description": "Minimum corner [x,y,z]"}, "max": {"type": "array", "items": {"type": "number"}, "description": "Maximum corner [x,y,z]"}, "material": {"type": "string", "description": "Default material for all faces (default: textures/common/caulk)"}, "materials": {"type": "object", "description": "Per-face materials. Keys: top, bottom, north, south, east, west. Unspecified faces use 'material'.", "properties": {"top": {"type": "string"}, "bottom": {"type": "string"}, "north": {"type": "string"}, "south": {"type": "string"}, "east": {"type": "string"}, "west": {"type": "string"}}}, "entity": {"type": "string", "description": "Parent entity name (default: worldspawn)"}}, "required": ["min", "max"]}),
    Tool(name="create_brushes", description="Create multiple brushes in a single batch operation (one undo step). Each brush spec has the same format as create_brush: min, max, material, materials. All brushes default to the same parent entity, but individual brushes can override with their own 'entity' key.", inputSchema={"type": "object", "properties": {"brushes": {"type": "array", "items": {"type": "object", "properties": {"min": {"type": "array", "items": {"type": "number"}, "description": "Minimum corner [x,y,z]"}, "max": {"type": "array", "items": {"type": "number"}, "description": "Maximum corner [x,y,z]"}, "material": {"type": "string", "description": "Default material for all faces"}, "materials": {"type": "object", "description": "Per-face materials: top, bottom, north, south, east, west"}, "entity": {"type": "string", "description": "Override parent entity for this brush"}}, "required": ["min", "max"]}, "description": "Array of brush specifications"}, "entity": {"type": "string", "description": "Default parent entity for all brushes (default: worldspawn)"}}, "required": ["brushes"]}),
    Tool(name="clone_brush", description="Clone an existing brush with an offset. Preserves all face planes and materials. Useful for symmetric geometry (pillars, repeated features).", inputSchema={"type": "object", "properties": {"entity": {"type": "string", "description": "Entity owning the source brush (default: worldspawn)"}, "index": {"type": "integer", "description": "Source brush index"}, "offset": {"type": "array", "items": {"type": "number"}, "description": "Translation offset [x,y,z] (default: [0,0,0])"}, "target_entity": {"type": "string", "description": "Entity to add clone to (default: same as source)"}}, "required": ["index"]}),
    Tool(name="create_entities", description="Create multiple entities in a single batch operation (one undo step). Each entity spec has the same format as create_entity: classname, origin, properties.", inputSchema={"type": "object", "properties": {"entities": {"type": "array", "items": {"type": "object", "properties": {"classname": {"type": "string", "description": "Entity class"}, "origin": {"type": "array", "items": {"type": "number"}, "description": "Position [x,y,z]"}, "properties": {"type": "object", "description": "Additional key-value properties"}}, "required": ["classname"]}, "description": "Array of entity specifications"}}, "required": ["entities"]}),
    Tool(name="select_all", description="Select all objects in the map.", inputSchema={"type": "object", "properties": {}}),
    Tool(name="deselect_all", description="Deselect all currently selected objects.", inputSchema={"type": "object", "properties": {}}),
    Tool(name="delete_selection", description="Delete all currently selected objects.", inputSchema={"type": "object", "properties": {}}),
    Tool(name="get_camera", description="Get the current camera position and angles in the 3D viewport.", inputSchema={"type": "object", "properties": {}}),
    Tool(name="set_camera", description="Set the camera position and/or angles in the 3D viewport.", inputSchema={"type": "object", "properties": {"origin": {"type": "array", "items": {"type": "number"}, "description": "Position [x,y,z]"}, "angles": {"type": "array", "items": {"type": "number"}, "description": "Angles [pitch,yaw,roll] degrees"}}}),
    Tool(name="execute_command", description="Execute a named editor command with optional arguments. Chain multiple with semicolons. Arguments are space-separated after the command name. Use list_commands to discover available commands. Examples: 'TextureNatural', 'SetShaderOnSelection textures/base_wall/lfwall27', 'FitTexture 1 1', 'TexScale 0.5 0.5', 'MoveSelection 0 0 64'.", inputSchema={"type": "object", "properties": {"command": {"type": "string", "description": "Command string with optional arguments, e.g. 'SetShaderOnSelection textures/common/caulk'"}}, "required": ["command"]}),
    Tool(name="undo", description="Undo the last editor operation.", inputSchema={"type": "object", "properties": {}}),
    Tool(name="redo", description="Redo the last undone operation.", inputSchema={"type": "object", "properties": {}}),
    Tool(name="get_scene_info", description="Get current selection info and selected entity details.", inputSchema={"type": "object", "properties": {}}),
    Tool(name="list_brushes", description="List all brushes with their bounds, materials, and parent entity. Brushes are identified by entity name + index.", inputSchema={"type": "object", "properties": {"entity": {"type": "string", "description": "Filter by entity name (e.g. 'worldspawn')"}, "limit": {"type": "integer", "description": "Max results (default 500)"}}}),
    Tool(name="get_brush_info", description="Get detailed info about a specific brush: bounds, face count, and per-face material and plane info.", inputSchema={"type": "object", "properties": {"entity": {"type": "string", "description": "Entity name owning the brush"}, "index": {"type": "integer", "description": "Brush index within the entity"}}, "required": ["entity", "index"]}),
    Tool(name="set_brush_material", description="Change the material on a brush. Can target all faces or a specific face by index.", inputSchema={"type": "object", "properties": {"entity": {"type": "string", "description": "Entity name owning the brush"}, "index": {"type": "integer", "description": "Brush index within the entity"}, "material": {"type": "string", "description": "New material path"}, "face": {"type": "integer", "description": "Optional: specific face index (omit to set all faces)"}}, "required": ["entity", "index", "material"]}),
    Tool(name="delete_brush", description="Delete a specific brush by entity name and index.", inputSchema={"type": "object", "properties": {"entity": {"type": "string", "description": "Entity name owning the brush"}, "index": {"type": "integer", "description": "Brush index within the entity"}}, "required": ["entity", "index"]}),
    Tool(name="delete_brushes", description="Delete multiple brushes in one operation. Specify indices as an array or a from/to range (inclusive). All indices are resolved before any deletion, so index shifting is not an issue.", inputSchema={"type": "object", "properties": {"entity": {"type": "string", "description": "Entity name owning the brushes"}, "indices": {"type": "array", "items": {"type": "integer"}, "description": "Array of brush indices to delete"}, "from": {"type": "integer", "description": "Start of index range (inclusive)"}, "to": {"type": "integer", "description": "End of index range (inclusive)"}}, "required": ["entity"]}),
    Tool(name="move_brush", description="Move a brush by an offset vector. Translates all face planes.", inputSchema={"type": "object", "properties": {"entity": {"type": "string", "description": "Entity name owning the brush"}, "index": {"type": "integer", "description": "Brush index"}, "offset": {"type": "array", "items": {"type": "number"}, "description": "Translation [dx, dy, dz]"}}, "required": ["entity", "index", "offset"]}),
    Tool(name="resize_brush", description="Resize a brush by specifying new min/max bounds. Preserves per-face materials. Replaces the brush with a new axis-aligned brush at the given bounds.", inputSchema={"type": "object", "properties": {"entity": {"type": "string", "description": "Entity name owning the brush"}, "index": {"type": "integer", "description": "Brush index"}, "min": {"type": "array", "items": {"type": "number"}, "description": "New minimum corner [x,y,z]"}, "max": {"type": "array", "items": {"type": "number"}, "description": "New maximum corner [x,y,z]"}}, "required": ["entity", "index", "min", "max"]}),
    Tool(name="convert_brushes_to_entity", description="Move worldspawn brushes into a new entity (e.g. func_static). Useful for grouping furniture, pillars, or detail geometry. The brushes are removed from the source entity and added to a new entity of the specified class.", inputSchema={"type": "object", "properties": {"entity": {"type": "string", "description": "Source entity name (default: worldspawn)"}, "indices": {"type": "array", "items": {"type": "integer"}, "description": "Brush indices to convert"}, "classname": {"type": "string", "description": "Target entity class (default: func_static)"}, "properties": {"type": "object", "description": "Key-value properties for the new entity"}}, "required": ["indices"]}),
    Tool(name="query_region", description="Find all entities and brushes within a bounding box. Use this to check what exists at a location before placing new geometry.", inputSchema={"type": "object", "properties": {"min": {"type": "array", "items": {"type": "number"}, "description": "Minimum corner [x,y,z]"}, "max": {"type": "array", "items": {"type": "number"}, "description": "Maximum corner [x,y,z]"}}, "required": ["min", "max"]}),
    Tool(name="get_scene_bounds", description="Get the overall bounding box of all geometry in the scene. Useful for orienting new additions relative to existing content.", inputSchema={"type": "object", "properties": {}}),
    Tool(name="move_entity", description="Move an entity to a new position by setting its origin.", inputSchema={"type": "object", "properties": {"name": {"type": "string", "description": "Entity name"}, "origin": {"type": "array", "items": {"type": "number"}, "description": "New position [x,y,z]"}}, "required": ["name", "origin"]}),
    Tool(name="rotate_entity", description="Set the rotation of an entity. Use 'angle' for yaw-only rotation (most common for doors, furniture). Doom 3 convention: 0=east(+X), 90=north(+Y), 180=west(-X), 270=south(-Y). Use 'rotation' for full 3x3 rotation matrix (9 values, row-major).", inputSchema={"type": "object", "properties": {"name": {"type": "string", "description": "Entity name"}, "angle": {"type": "number", "description": "Yaw angle in degrees (0=east, 90=north, 180=west, 270=south)"}, "rotation": {"type": "array", "items": {"type": "number"}, "description": "Full 3x3 rotation matrix as 9 values [r0,r1,r2,r3,r4,r5,r6,r7,r8]"}}, "required": ["name"]}),
    Tool(name="delete_entity", description="Delete a named entity from the map.", inputSchema={"type": "object", "properties": {"name": {"type": "string", "description": "Entity name to delete"}}, "required": ["name"]}),
    Tool(name="measure_distance", description="Measure distances between two targets. Returns center-to-center distance, surface-to-surface gap distance, axis deltas, and whether they overlap. Targets can be entities, specific brushes, or arbitrary points.", inputSchema={"type": "object", "properties": {"from": {"type": "object", "description": "Source target: {entity: 'name'} or {entity: 'name', brush_index: N} or {point: [x,y,z]}"}, "to": {"type": "object", "description": "Destination target: same format as 'from'"}}, "required": ["from", "to"]}),
    Tool(name="get_game_config", description="Get Doom 3 standard sizes and reference dimensions: player width/height/crouch/eye heights, max step-up, jump height, run/walk/crouch speeds, standard door/hallway/room sizes, and unit scale.", inputSchema={"type": "object", "properties": {}}),
    Tool(name="list_commands", description="List all available editor commands. Commands can be executed via execute_command. Common commands: TextureNatural, NormaliseTexture, FitTexture <width> <height>, SetShaderOnSelection <material>, TexAlign <top|bottom|left|right>, TexRotate <degrees>, TexScale <sx sy>, TexShift <dx dy>, FlipTextureX, FlipTextureY, SnapToGrid, CloneSelection, MirrorSelectionX/Y/Z, RotateSelectionX/Y/Z, ConvertSelectedToFuncStatic, SelectAllOfType, SelectItemsByShader, NudgeSelected <direction>, MoveSelection <x y z>, ScaleSelected <x y z>, RotateSelectedEulerXYZ <pitch yaw roll>, CSGSubtract, CSGMerge.", inputSchema={"type": "object", "properties": {"filter": {"type": "string", "description": "Case-insensitive filter on command name"}}}),
    Tool(name="list_materials", description="List materials (textures/shaders). Defaults to prefix 'textures/' when no filters given. Use prefix to browse by path (e.g. 'textures/darkmod/stone/'), filter for substring match, surface_type for physical type, or details=true to include surface_type/coverage/description in results.", inputSchema={"type": "object", "properties": {"prefix": {"type": "string", "description": "Path prefix filter (e.g. 'textures/darkmod/stone/'). Defaults to 'textures/' when no filters given. Use empty string to list all materials."}, "filter": {"type": "string", "description": "Substring filter on material name (combined with prefix if both given)"}, "surface_type": {"type": "string", "description": "Filter by surface type: metal, stone, wood, glass, flesh, cardboard, liquid, plastic, default. Implies details=true."}, "details": {"type": "boolean", "description": "Include surface_type, coverage, and description in results (slower, instantiates each material). Default false."}, "limit": {"type": "integer", "description": "Max results (default 100)"}}}),
    Tool(name="get_material_info", description="Get detailed info about a specific material: surface type, coverage, flags (noshadows, translucent, nonsolid, etc.), shader layers with their types and texture maps.", inputSchema={"type": "object", "properties": {"name": {"type": "string", "description": "Full material name (e.g. textures/base_wall/lfwall27)"}}, "required": ["name"]}),
    Tool(name="get_entity_class_info", description="Get full info about an entity class: class type, parent, color, model, usage description, and all spawnargs with types, defaults, and descriptions.", inputSchema={"type": "object", "properties": {"name": {"type": "string", "description": "Entity class name (e.g. light, info_player_start, func_static)"}}, "required": ["name"]}),
    Tool(name="list_models", description="List available 3D models (.lwo, .ase, .md5mesh) from the game assets. Filter by name substring.", inputSchema={"type": "object", "properties": {"filter": {"type": "string", "description": "Text filter on model path"}, "limit": {"type": "integer", "description": "Max results (default 100)"}}}),
    Tool(name="list_sounds", description="List sound shaders with name, display folder, and min/max radius. Filter by name substring.", inputSchema={"type": "object", "properties": {"filter": {"type": "string", "description": "Text filter on sound name"}, "limit": {"type": "integer", "description": "Max results (default 100)"}}}),
    Tool(name="list_skins", description="List available skins (material remappings for models). Optionally filter by model path to get only applicable skins.", inputSchema={"type": "object", "properties": {"model": {"type": "string", "description": "Model path to filter skins for"}, "limit": {"type": "integer", "description": "Max results (default 100)"}}}),
    Tool(name="list_particles", description="List available particle effect definitions. Filter by name substring.", inputSchema={"type": "object", "properties": {"filter": {"type": "string", "description": "Text filter on particle name"}, "limit": {"type": "integer", "description": "Max results (default 100)"}}}),
    Tool(name="list_prefabs", description="List available prefab files (.pfb, .pfbx, .map) from the prefabs/ directory. Prefabs are reusable map fragments that can be inserted into the current map.", inputSchema={"type": "object", "properties": {"filter": {"type": "string", "description": "Text filter on prefab name/path"}, "limit": {"type": "integer", "description": "Max results (default 100)"}}}),
    Tool(name="get_prefab_info", description="Get detailed info about a prefab: description (auto-generated if empty, listing models/materials), entity/brush/patch counts, bounding box with dimensions, designed origin, contained entities, models used, and visible materials used.", inputSchema={"type": "object", "properties": {"path": {"type": "string", "description": "VFS path to the prefab (e.g. 'prefabs/rooms/hallway.pfb')"}}, "required": ["path"]}),
    Tool(name="insert_prefab", description="Insert a prefab into the current map at a given position with optional rotation. The prefab geometry is merged into the map. Supports 'angle' (yaw degrees) or 'angles' (pitch/yaw/roll) for rotation on insertion.", inputSchema={"type": "object", "properties": {"path": {"type": "string", "description": "VFS path to the prefab"}, "origin": {"type": "array", "items": {"type": "number"}, "description": "Insertion position [x,y,z] (default: [0,0,0])"}, "angle": {"type": "number", "description": "Yaw rotation in degrees (0=east, 90=north, 180=west, 270=south)"}, "angles": {"type": "array", "items": {"type": "number"}, "description": "Full rotation [pitch, yaw, roll] in degrees"}, "as_group": {"type": "boolean", "description": "Group prefab parts together (default: true)"}, "recalculate_origin": {"type": "boolean", "description": "Recalculate prefab origin to center at insertion point (default: true)"}}, "required": ["path"]}),
    Tool(name="list_shortcuts", description="List keyboard shortcuts currently configured in the editor. Returns command names with their bound key combinations. Use this to teach users the shortcuts for operations they ask about. Filter by command name or key combo substring.", inputSchema={"type": "object", "properties": {"filter": {"type": "string", "description": "Case-insensitive filter on command name or shortcut key (e.g. 'clone', 'ctrl+c', 'texture', 'select')"}, "limit": {"type": "integer", "description": "Max results (default 500)"}}}),
    Tool(name="get_command_shortcut", description="Get the keyboard shortcut bound to a specific editor command. Returns the key combination string, whether it's bound, and whether the command exists. Use this when a user asks 'how do I do X' to tell them the exact shortcut.", inputSchema={"type": "object", "properties": {"command": {"type": "string", "description": "Command name (e.g. 'CloneSelection', 'DeleteSelection', 'ToggleSurfaceInspector')"}}, "required": ["command"]}),
    Tool(name="list_special_materials", description="List special/utility textures (caulk, clip, visportal, trigger, etc.) with their material paths, categories, and descriptions. Use this to find the correct texture path for non-visible utility surfaces. Filter by category or text search.", inputSchema={"type": "object", "properties": {"filter": {"type": "string", "description": "Case-insensitive text filter on name, path, or description"}, "category": {"type": "string", "description": "Filter by category: optimization, collision, trigger, lighting"}}}),
    Tool(name="capture_view", description="Capture a screenshot of an editor viewport. Returns a PNG image. Use this to see the current state of the level from the 3D camera or any orthographic (top/side/front) view. Call this after making changes to verify the result visually.", inputSchema={"type": "object", "properties": {"view": {"type": "string", "enum": ["camera", "xy", "xz", "yz"], "description": "Which view to capture: 'camera' (3D perspective), 'xy' (top-down), 'xz' (side), 'yz' (front). Default: 'camera'"}, "max_width": {"type": "integer", "description": "Maximum image width in pixels for downscaling (default: 800). Use smaller values to reduce response size."}}}),
    Tool(name="csg_subtract", description="CSG Subtract: use selected brushes to cut holes in unselected brushes they overlap. The selected brushes are removed after the operation. Select the 'cutter' brushes first, then run this. Useful for carving doorways, windows, or complex shapes out of existing geometry.", inputSchema={"type": "object", "properties": {}}),
    Tool(name="csg_merge", description="CSG Merge: combine multiple selected brushes into a single convex brush. Only works if the result is convex. Useful for simplifying geometry after construction.", inputSchema={"type": "object", "properties": {}}),
    Tool(name="csg_hollow", description="CSG Hollow: hollow out selected brushes, turning each solid brush into a set of thin-walled brushes (like a box). The wall thickness equals the current grid size. Useful for quickly creating rooms from solid blocks.", inputSchema={"type": "object", "properties": {}}),
    Tool(name="csg_room", description="CSG Room: similar to CSG Hollow but creates a sealed room with caulk on exterior faces. The interior faces keep the original material. Wall thickness equals the current grid size.", inputSchema={"type": "object", "properties": {}}),
    Tool(name="csg_intersect", description="CSG Intersect: keep only the volume where all selected brushes overlap, discarding everything else. Useful for creating complex shapes from overlapping primitives.", inputSchema={"type": "object", "properties": {}}),
    Tool(name="csg_seal", description="CSG Seal: attempt to seal the selected entity by adding brushes where leaks exist. Works on the selected entity's brushes.", inputSchema={"type": "object", "properties": {}}),
    Tool(name="csg_passable", description="CSG Passable: set the 'nonsolid' flag on selected brushes, making them non-blocking to the player. The brushes remain visible but don't create collision.", inputSchema={"type": "object", "properties": {}}),
    Tool(name="make_brush_prefab", description="Convert selected brush(es) into a geometric shape: prism (cylinder-like polygon), cone, or sphere. The brush's current bounding box defines the shape dimensions. Select a brush first, then call this to reshape it.", inputSchema={"type": "object", "properties": {"shape": {"type": "string", "enum": ["prism", "cone", "sphere", "cuboid"], "description": "Target shape"}, "sides": {"type": "integer", "description": "Number of sides for prism/cone/sphere (default: 8, min: 3)"}}, "required": ["shape"]}),
    Tool(name="clip_brush", description="Clip or split selected brushes along a plane defined by 2 or 3 points. In 'clip' mode, the part on one side of the plane is kept and the other discarded. In 'split' mode, both halves are kept as separate brushes. Use 'flip' to reverse which side is kept when clipping. The plane is defined by 2 points (auto-projected) or 3 points (explicit plane).", inputSchema={"type": "object", "properties": {"point1": {"type": "array", "items": {"type": "number"}, "description": "First clip point [x,y,z]"}, "point2": {"type": "array", "items": {"type": "number"}, "description": "Second clip point [x,y,z]"}, "point3": {"type": "array", "items": {"type": "number"}, "description": "Optional third clip point [x,y,z] for explicit plane"}, "mode": {"type": "string", "enum": ["clip", "split"], "description": "clip = discard one side (default), split = keep both halves"}, "flip": {"type": "boolean", "description": "Reverse which side is kept when clipping (default: false)"}}, "required": ["point1", "point2"]}),

]


@app.list_tools()
async def list_tools():
    return TOOLS


@app.call_tool()
async def call_tool(name: str, arguments: dict) -> list[TextContent | ImageContent]:
    try:
        result = conn.call(name, arguments)

        # capture_view returns base64 image data
        if name == "capture_view" and isinstance(result, dict) and "image" in result:
            return [
                ImageContent(
                    type="image",
                    data=result["image"],
                    mimeType="image/png",
                ),
                TextContent(
                    type="text",
                    text=f"Captured {result.get('view', 'unknown')} view",
                ),
            ]

        return [TextContent(type="text", text=json.dumps(result, indent=2))]
    except ConnectionError as e:
        return [TextContent(type="text", text=f"Connection error: {e}")]
    except RuntimeError as e:
        return [TextContent(type="text", text=f"Error: {e}")]
    except Exception as e:
        return [TextContent(type="text", text=f"Unexpected error: {type(e).__name__}: {e}")]


async def main():
    async with stdio_server() as (read_stream, write_stream):
        await app.run(read_stream, write_stream, app.create_initialization_options())


def run():
    anyio.run(main)


if __name__ == "__main__":
    run()
