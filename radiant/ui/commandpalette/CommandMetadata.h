#pragma once

#include <string>
#include <unordered_map>

namespace ui
{

enum class CmdCategory
{
	General,
	Brush,
	Patch,
	Entity,
	Texture,
	Selection,
	Transform,
	Camera,
	View,
	Map,
};

struct CommandInfo
{
	const char* displayName;
	const char* description;
	CmdCategory category = CmdCategory::General;
};

inline const std::unordered_map<std::string, CommandInfo>& getCommandMetadata()
{
	static const std::unordered_map<std::string, CommandInfo> metadata = {
		// Map operations
		{"NewMap",                      {"New Map",                         "Start a new empty map", CmdCategory::Map}},
		{"OpenMap",                     {"Open Map",                        "Open an existing map file from disk", CmdCategory::Map}},
		{"OpenMapFromProject",          {"Open Map from Project",           "Browse and open a map from the current project", CmdCategory::Map}},
		{"OpenMapFromArchive",          {"Open Map from Archive",           "Open a map from a PK4 archive", CmdCategory::Map}},
		{"ImportMap",                   {"Import Map",                      "Merge another map file into the current map", CmdCategory::Map}},
		{"SaveMap",                     {"Save Map",                        "Save the current map to disk", CmdCategory::Map}},
		{"SaveMapAs",                   {"Save Map As",                     "Save the current map under a new file name", CmdCategory::Map}},
		{"SaveMapCopyAs",              {"Save Map Copy As",                "Save a copy without changing the current file name", CmdCategory::Map}},
		{"SaveSelected",                {"Save Selected as Map",            "Save the current selection into a new map file", CmdCategory::Map}},
		{"SaveSelectedAsPrefab",        {"Save Selected as Prefab",         "Save the current selection as a reusable prefab", CmdCategory::Map}},
		{"ExportMap",                   {"Export Map",                      "Export the map in a different format", CmdCategory::Map}},
		{"LoadPrefab",                  {"Import Prefab",                   "Browse and insert a prefab into the map", CmdCategory::Map}},
		{"SaveRegion",                  {"Save Region",                     "Save the current region to a file", CmdCategory::Map}},
		{"RegionSetSelection",          {"Region: Set to Selection",        "Set the export region to the current selection bounds", CmdCategory::Map}},
		{"RegionSetBrush",              {"Region: Set to Brush",            "Set the export region to the selected brush bounds", CmdCategory::Map}},
		{"RegionSetXY",                 {"Region: Set to XY View",          "Set the export region to the visible XY area", CmdCategory::Map}},
		{"RegionOff",                   {"Region: Clear",                   "Remove any active region", CmdCategory::Map}},
		{"Exit",                        {"Exit",                            "Quit HellForge"}},

		// Edit operations
		{"Undo",                        {"Undo",                            "Undo the last operation"}},
		{"Redo",                        {"Redo",                            "Redo the last undone operation"}},
		{"Copy",                        {"Copy",                            "Copy the selection to the clipboard", CmdCategory::Selection}},
		{"Cut",                         {"Cut",                             "Cut the selection to the clipboard", CmdCategory::Selection}},
		{"Paste",                       {"Paste",                           "Paste from the clipboard at the original location", CmdCategory::Selection}},
		{"PasteToCamera",               {"Paste to Camera",                 "Paste from the clipboard at the camera position", CmdCategory::Selection}},
		{"CloneSelection",              {"Duplicate",                       "Duplicate the selected objects with a small offset", CmdCategory::Selection}},
		{"DeleteSelection",             {"Delete",                          "Delete the selected objects", CmdCategory::Selection}},
		{"UnSelectSelection",           {"Deselect All",                    "Clear the current selection", CmdCategory::Selection}},
		{"InvertSelection",             {"Invert Selection",                "Select everything that is not currently selected", CmdCategory::Selection}},
		{"SelectAllOfType",             {"Select All of Type",              "Select all objects matching the type of the selection", CmdCategory::Selection}},
		{"SelectChildren",              {"Select Children",                 "Select child primitives of the selected entity", CmdCategory::Selection}},
		{"SelectParentEntities",        {"Select Parent Entities",          "Select the parent entity of the selected primitives", CmdCategory::Selection}},
		{"ExpandSelectionToSiblings",   {"Select Siblings",                 "Expand selection to all siblings of the selected primitive", CmdCategory::Selection}},
		{"SelectCompleteTall",          {"Select Complete Tall",            "Select all objects within the selected brush outline", CmdCategory::Selection}},
		{"SelectInside",                {"Select Inside",                   "Select all objects fully inside the selected brush", CmdCategory::Selection}},
		{"SelectFullyInside",           {"Select Fully Inside",             "Select objects fully inside without touching boundary", CmdCategory::Selection}},
		{"SelectTouching",              {"Select Touching",                 "Select all objects touching the selected brush", CmdCategory::Selection}},
		{"SelectItemsByShader",         {"Select by Shader",                "Select all objects using a particular material", CmdCategory::Selection}},
		{"DeselectItemsByShader",       {"Deselect by Shader",              "Deselect all objects using a particular material", CmdCategory::Selection}},
		{"SelectItemsByModel",          {"Select by Model",                 "Select all objects using a particular model", CmdCategory::Selection}},
		{"DeselectItemsByModel",        {"Deselect by Model",               "Deselect all objects using a particular model", CmdCategory::Selection}},

		// Grouping
		{"GroupSelected",               {"Group Selection",                 "Combine selected objects into a group", CmdCategory::Selection}},
		{"UngroupSelected",             {"Ungroup Selection",               "Break the selected group into individual objects", CmdCategory::Selection}},
		{"GroupCycleForward",           {"Cycle Group Forward",             "Cycle forward through grouped objects", CmdCategory::Selection}},
		{"GroupCycleBackward",          {"Cycle Group Backward",            "Cycle backward through grouped objects", CmdCategory::Selection}},
		{"DeleteAllSelectionGroups",    {"Delete All Groups",               "Remove all selection groups in the map", CmdCategory::Selection}},

		// Selection sets
		{"DeleteAllSelectionSets",      {"Delete All Selection Sets",       "Remove all saved selection sets", CmdCategory::Selection}},

		// Entity operations
		{"ConnectSelection",            {"Connect Entities",                "Set a target from the first entity to the second", CmdCategory::Entity}},
		{"BindSelection",               {"Bind Entities",                   "Bind the first selected entity to the second", CmdCategory::Entity}},
		{"ParentSelection",             {"Reparent to Entity",              "Make selected primitives children of the selected entity", CmdCategory::Entity}},
		{"ParentSelectionToWorldspawn",  {"Reparent to Worldspawn",         "Move selected primitives back to worldspawn", CmdCategory::Entity}},
		{"MergeSelectedEntities",       {"Merge Entities",                  "Merge multiple entities into one", CmdCategory::Entity}},
		{"ConvertSelectedToFuncStatic",  {"Convert to func_static",         "Convert selected primitives into a func_static entity", CmdCategory::Entity}},
		{"RevertToWorldspawn",          {"Revert to Worldspawn",            "Dissolve entity and return primitives to worldspawn", CmdCategory::Entity}},
		{"CreateSpeaker",               {"Create Speaker",                  "Create a sound-emitting speaker entity", CmdCategory::Entity}},
		{"PlacePlayerStart",            {"Place Player Start",              "Place or move the player start position", CmdCategory::Entity}},
		{"SetEntityKeyValue",           {"Set Entity Key/Value",            "Set a spawnarg on the selected entity", CmdCategory::Entity}},

		// Shader / material operations
		{"CopyShader",                  {"Copy Shader",                     "Copy the material from the selected face", CmdCategory::Texture}},
		{"PasteShader",                 {"Paste Shader",                    "Apply the copied material to the selection", CmdCategory::Texture}},
		{"PasteShaderNatural",          {"Paste Shader (Natural)",          "Apply the copied material with natural alignment", CmdCategory::Texture}},
		{"SetShaderOnSelection",        {"Apply Material to Selection",     "Set a specific material on all selected faces", CmdCategory::Texture}},
		{"FitTexture",                  {"Fit Texture",                     "Fit the texture to the selected face(s)", CmdCategory::Texture}},
		{"NormaliseTexture",            {"Normalize Texture",               "Reset texture to default alignment", CmdCategory::Texture}},
		{"TextureNatural",              {"Natural Texture Alignment",       "Apply natural texture scaling to the selection", CmdCategory::Texture}},
		{"FlipTextureX",                {"Flip Texture X",                  "Mirror the texture horizontally", CmdCategory::Texture}},
		{"FlipTextureY",                {"Flip Texture Y",                  "Mirror the texture vertically", CmdCategory::Texture}},

		// Texture shifting/scaling/rotating
		{"TexShiftUp",                  {"Shift Texture Up",                "Shift the texture up by the grid amount", CmdCategory::Texture}},
		{"TexShiftDown",                {"Shift Texture Down",              "Shift the texture down by the grid amount", CmdCategory::Texture}},
		{"TexShiftLeft",                {"Shift Texture Left",              "Shift the texture left by the grid amount", CmdCategory::Texture}},
		{"TexShiftRight",               {"Shift Texture Right",             "Shift the texture right by the grid amount", CmdCategory::Texture}},
		{"TexScaleUp",                  {"Scale Texture Up",                "Scale the texture up", CmdCategory::Texture}},
		{"TexScaleDown",                {"Scale Texture Down",              "Scale the texture down", CmdCategory::Texture}},
		{"TexScaleLeft",                {"Scale Texture Left",              "Scale the texture left", CmdCategory::Texture}},
		{"TexScaleRight",               {"Scale Texture Right",             "Scale the texture right", CmdCategory::Texture}},
		{"TexRotateClock",              {"Rotate Texture Clockwise",        "Rotate the texture clockwise", CmdCategory::Texture}},
		{"TexRotateCounter",            {"Rotate Texture Counterclockwise", "Rotate the texture counterclockwise", CmdCategory::Texture}},

		// Brush operations
		{"BrushMakeSided",              {"Make Sided Brush",                "Create a brush with a specific number of sides", CmdCategory::Brush}},
		{"BrushMakePrefab",             {"Make Brush Prefab",               "Create a brush shape: prism, cone, sphere, or cuboid", CmdCategory::Brush}},
		{"BrushSetDetailFlag",          {"Set Brush Detail Flag",           "Toggle detail/structural flag on selected brushes", CmdCategory::Brush}},
		{"QueryBrushPrefabSidesDialog", {"Brush Prefab Sides",             "Choose the number of sides for a brush prefab", CmdCategory::Brush}},
		{"MakeVisportal",               {"Make Visportal",                  "Apply the visportal material to the selected brush", CmdCategory::Brush}},
		{"SurroundWithMonsterclip",     {"Surround with Monsterclip",       "Create monsterclip brushes around the selection", CmdCategory::Brush}},
		{"FindBrush",                   {"Find Brush",                      "Search for a brush by number", CmdCategory::Brush}},
		{"ResizeSelectedBrushesToBounds", {"Resize Brushes to Bounds",      "Resize selected brushes to specific dimensions", CmdCategory::Brush}},

		// CSG operations
		{"CSGSubtract",                 {"CSG: Subtract",                   "Subtract the selected brush from surrounding brushes", CmdCategory::Brush}},
		{"CSGMerge",                    {"CSG: Merge",                      "Merge overlapping brushes into one", CmdCategory::Brush}},
		{"CSGHollow",                   {"CSG: Make Hollow",                "Hollow out the selected brush", CmdCategory::Brush}},
		{"CSGRoom",                     {"CSG: Make Room",                  "Create a room from the selected brush without overlaps", CmdCategory::Brush}},
		{"CSGIntersect",                {"CSG: Intersect",                  "Keep only the overlapping volume of selected brushes", CmdCategory::Brush}},
		{"CSGSeal",                     {"CSG: Seal",                       "Seal gap between brushes", CmdCategory::Brush}},
		{"CSGPassable",                 {"CSG: Make Passable",              "Mark selected brush as passable (non-solid)", CmdCategory::Brush}},
		{"CSGShell",                    {"CSG: Shell",                      "Create a shell from the selected brush", CmdCategory::Brush}},

		// Clipper
		{"ToggleClipper",               {"Toggle Clipper",                  "Activate or deactivate the clip tool", CmdCategory::Brush}},

		// Patch operations
		{"CreateSimplePatchDialog",     {"Create Patch Mesh",               "Create a new flat patch mesh", CmdCategory::Patch}},
		{"CreateSimplePatchMesh",       {"Create Simple Patch",             "Create a simple patch with default settings", CmdCategory::Patch}},
		{"CreatePatchPrefab",           {"Create Patch Prefab",             "Create a patch shape: cylinder, sphere, cone, etc.", CmdCategory::Patch}},
		{"PatchCapDialog",              {"Cap Patch",                       "Close the open end of a patch cylinder", CmdCategory::Patch}},
		{"ThickenPatchDialog",          {"Thicken Patch",                   "Give a patch visible thickness", CmdCategory::Patch}},
		{"BulgePatchDialog",            {"Bulge Patch",                     "Deform a flat patch into a bulge", CmdCategory::Patch}},
		{"StitchPatchTexture",          {"Stitch Patch Textures",           "Align textures between adjacent patches", CmdCategory::Patch}},
		{"MatrixTranspose",             {"Transpose Patch Matrix",          "Swap rows and columns of the patch control grid", CmdCategory::Patch}},
		{"InvertCurve",                 {"Invert Patch Normals",            "Flip the visible side of the patch", CmdCategory::Patch}},
		{"RedisperseRows",              {"Redisperse Rows",                 "Evenly space control points along rows", CmdCategory::Patch}},
		{"RedisperseCols",              {"Redisperse Columns",              "Evenly space control points along columns", CmdCategory::Patch}},
		{"CapSelectedPatches",          {"Cap Selected Patches",            "Cap the selected patches", CmdCategory::Patch}},
		{"PatchAppendRowEnd",           {"Append Row at End",               "Add a row of control points at the end", CmdCategory::Patch}},
		{"PatchAppendRowBeginning",     {"Append Row at Beginning",         "Add a row of control points at the beginning", CmdCategory::Patch}},
		{"PatchAppendColumnEnd",        {"Append Column at End",            "Add a column of control points at the end", CmdCategory::Patch}},
		{"PatchAppendColumnBeginning",  {"Append Column at Beginning",      "Add a column of control points at the beginning", CmdCategory::Patch}},
		{"PatchDeleteRowEnd",           {"Delete Last Row",                 "Remove the last row of control points", CmdCategory::Patch}},
		{"PatchDeleteRowBeginning",     {"Delete First Row",                "Remove the first row of control points", CmdCategory::Patch}},
		{"PatchDeleteColumnEnd",        {"Delete Last Column",              "Remove the last column of control points", CmdCategory::Patch}},
		{"PatchDeleteColumnBeginning",  {"Delete First Column",             "Remove the first column of control points", CmdCategory::Patch}},

		// Transformation
		{"RotateSelectionX",            {"Rotate X",                        "Rotate selection around the X axis", CmdCategory::Transform}},
		{"RotateSelectionY",            {"Rotate Y",                        "Rotate selection around the Y axis", CmdCategory::Transform}},
		{"RotateSelectionZ",            {"Rotate Z",                        "Rotate selection around the Z axis", CmdCategory::Transform}},
		{"MirrorSelectionX",            {"Mirror X",                        "Mirror the selection along the X axis", CmdCategory::Transform}},
		{"MirrorSelectionY",            {"Mirror Y",                        "Mirror the selection along the Y axis", CmdCategory::Transform}},
		{"MirrorSelectionZ",            {"Mirror Z",                        "Mirror the selection along the Z axis", CmdCategory::Transform}},
		{"FloorSelection",              {"Floor Selection",                 "Move the selection down to touch the floor", CmdCategory::Transform}},
		{"SnapToGrid",                  {"Snap to Grid",                    "Snap the selection to the nearest grid point", CmdCategory::Transform}},
		{"NudgeSelected",               {"Nudge Selection",                 "Move the selection by one grid unit", CmdCategory::Transform}},
		{"MoveSelection",               {"Move Selection",                  "Move the selection by a specified amount", CmdCategory::Transform}},
		{"MoveSelectionVertically",     {"Move Selection Vertically",       "Move the selection up or down", CmdCategory::Transform}},
		{"RotateSelectedEulerXYZ",      {"Rotate Selection (Euler)",        "Rotate the selection by Euler angles", CmdCategory::Transform}},
		{"ScaleSelected",               {"Scale Selection",                 "Scale the selection by a factor", CmdCategory::Transform}},
		{"ArrayDialog",                 {"Array",                           "Create an array of clones from the selection", CmdCategory::Entity}},
		{"ScatterDialog",               {"Scatter Objects",                 "Scatter copies of the selection across a surface", CmdCategory::Entity}},

		// Camera
		{"CameraMoveForward",           {"Camera: Move Forward",            "Move the camera forward", CmdCategory::Camera}},
		{"CameraMoveBack",              {"Camera: Move Back",               "Move the camera backward", CmdCategory::Camera}},
		{"CameraMoveLeft",              {"Camera: Strafe Left",             "Strafe the camera left", CmdCategory::Camera}},
		{"CameraMoveRight",             {"Camera: Strafe Right",            "Strafe the camera right", CmdCategory::Camera}},
		{"CameraMoveUp",                {"Camera: Move Up",                 "Move the camera up", CmdCategory::Camera}},
		{"CameraMoveDown",              {"Camera: Move Down",               "Move the camera down", CmdCategory::Camera}},
		{"CameraAngleUp",               {"Camera: Look Up",                 "Tilt the camera upward", CmdCategory::Camera}},
		{"CameraAngleDown",             {"Camera: Look Down",               "Tilt the camera downward", CmdCategory::Camera}},
		{"CameraStrafeLeft",            {"Camera: Turn Left",               "Turn the camera left", CmdCategory::Camera}},
		{"CameraStrafeRight",           {"Camera: Turn Right",              "Turn the camera right", CmdCategory::Camera}},
		{"CamIncreaseMoveSpeed",        {"Camera: Increase Speed",          "Increase camera movement speed", CmdCategory::Camera}},
		{"CamDecreaseMoveSpeed",        {"Camera: Decrease Speed",          "Decrease camera movement speed", CmdCategory::Camera}},
		{"CenterView",                  {"Center Camera View",              "Center the camera on the selection", CmdCategory::Camera}},
		{"CubicClipZoomIn",             {"Camera: Zoom In (Cubic)",         "Zoom the camera in (cubic clip)", CmdCategory::Camera}},
		{"CubicClipZoomOut",            {"Camera: Zoom Out (Cubic)",        "Zoom the camera out (cubic clip)", CmdCategory::Camera}},
		{"FocusCameraViewOnSelection",  {"Focus Camera on Selection",       "Point the camera at the selected objects", CmdCategory::Camera}},
		{"FocusViews",                  {"Focus All Views on Selection",    "Center all views on the selected objects", CmdCategory::Camera}},
		{"MoveCamera",                  {"Move Camera to Position",         "Move the camera to a specific location", CmdCategory::Camera}},
		{"SetActiveCameraPosition",     {"Set Camera Position",             "Set the camera to an exact position", CmdCategory::Camera}},
		{"SetActiveCameraAngles",       {"Set Camera Angles",               "Set the camera to exact view angles", CmdCategory::Camera}},
		{"CaptureCamera",               {"Capture Camera View",             "Take a screenshot of the 3D camera view", CmdCategory::Camera}},

		// 2D views
		{"CenterXYView",                {"Center 2D View",                  "Center the active 2D view on the selection", CmdCategory::View}},
		{"CenterXYViews",               {"Center All 2D Views",             "Center all 2D views on the selection", CmdCategory::View}},
		{"NextView",                    {"Cycle 2D View Axis",              "Switch between XY, XZ, and YZ views", CmdCategory::View}},
		{"ViewTop",                     {"2D View: Top (XY)",               "Switch to top-down view", CmdCategory::View}},
		{"ViewFront",                   {"2D View: Front (XZ)",             "Switch to front view", CmdCategory::View}},
		{"ViewSide",                    {"2D View: Side (YZ)",              "Switch to side view", CmdCategory::View}},
		{"ZoomIn",                      {"Zoom In",                         "Zoom into the active view", CmdCategory::View}},
		{"ZoomOut",                     {"Zoom Out",                        "Zoom out of the active view", CmdCategory::View}},
		{"Zoom100",                     {"Zoom to 100%",                    "Reset the zoom level to 100%", CmdCategory::View}},
		{"CaptureOrthoView",            {"Capture 2D View",                 "Take a screenshot of the active 2D view", CmdCategory::View}},

		// Grid
		{"GridUp",                      {"Increase Grid Size",              "Double the current grid spacing", CmdCategory::View}},
		{"GridDown",                    {"Decrease Grid Size",              "Halve the current grid spacing", CmdCategory::View}},
		{"SetGrid",                     {"Set Grid Size",                   "Set the grid to a specific size", CmdCategory::View}},
		{"ToggleGrid",                  {"Toggle Grid Display",             "Show or hide grid lines", CmdCategory::View}},

		// Manipulator modes
		{"MouseTranslate",              {"Translate Mode",                  "Switch to the translate manipulator", CmdCategory::Transform}},
		{"MouseRotate",                 {"Rotate Mode",                     "Switch to the rotate manipulator", CmdCategory::Transform}},
		{"MouseDrag",                   {"Drag Mode",                       "Switch to the drag/resize manipulator", CmdCategory::Transform}},

		// Selection modes
		{"ToggleEntitySelectionMode",       {"Toggle Entity Selection Mode",       "Switch to entity selection mode", CmdCategory::Selection}},
		{"ToggleGroupPartSelectionMode",    {"Toggle Group Part Selection Mode",   "Switch to group part selection mode", CmdCategory::Selection}},
		{"ToggleComponentSelectionMode",    {"Toggle Component Selection Mode",    "Switch to component (vertex/edge/face) mode", CmdCategory::Selection}},
		{"ToggleMergeActionSelectionMode",  {"Toggle Merge Action Selection Mode", "Switch to merge action selection mode", CmdCategory::Selection}},
		{"DragVertices",                    {"Select Vertices",                    "Enter vertex editing mode", CmdCategory::Selection}},
		{"DragEdges",                       {"Select Edges",                       "Enter edge editing mode", CmdCategory::Selection}},
		{"DragFaces",                       {"Select Faces",                       "Enter face editing mode", CmdCategory::Selection}},
		{"DragEntities",                    {"Select Entities",                    "Enter entity drag mode", CmdCategory::Selection}},

		// Visibility
		{"ShowHidden",                  {"Show Hidden Objects",             "Make all hidden objects visible again", CmdCategory::View}},
		{"HideSelected",                {"Hide Selected",                   "Hide the selected objects from view", CmdCategory::View}},
		{"HideDeselected",              {"Hide Unselected",                 "Hide everything except the selection", CmdCategory::View}},

		// Layers
		{"CreateNewLayerDialog",        {"Create Layer",                    "Create a new layer"}},

		// Filters
		{"EditFiltersDialog",           {"Edit Filters",                    "Create and manage object visibility filters", CmdCategory::View}},

		// Floors
		{"UpFloor",                     {"Up Floor",                        "Move the camera up one floor level", CmdCategory::Camera}},
		{"DownFloor",                   {"Down Floor",                      "Move the camera down one floor level", CmdCategory::Camera}},

		// Curves
		{"CreateCurveNURBS",            {"Create NURBS Curve",              "Create a NURBS curve on the selected entity", CmdCategory::Entity}},
		{"CreateCurveCatmullRom",       {"Create Catmull-Rom Curve",        "Create a Catmull-Rom spline on the selected entity", CmdCategory::Entity}},
		{"CurveAppendControlPoint",     {"Append Curve Point",              "Add a control point to the curve", CmdCategory::Entity}},
		{"CurveInsertControlPoint",     {"Insert Curve Point",              "Insert a control point into the curve", CmdCategory::Entity}},
		{"CurveRemoveControlPoint",     {"Remove Curve Point",              "Remove a control point from the curve", CmdCategory::Entity}},
		{"CurveConvertType",            {"Convert Curve Type",              "Switch between NURBS and Catmull-Rom", CmdCategory::Entity}},

		// UI panels and inspectors
		{"ToggleSurfaceInspector",      {"Surface Inspector",               "Open the texture/surface alignment inspector", CmdCategory::Texture}},
		{"TogglePatchInspector",        {"Patch Inspector",                 "Open the patch control point inspector", CmdCategory::Patch}},
		{"ToggleTransformPanel",        {"Transform Panel",                 "Open the transform panel for precise positioning", CmdCategory::Transform}},
		{"ToggleEntityList",            {"Entity List",                     "Open the list of all entities in the map", CmdCategory::Entity}},
		{"LightInspector",              {"Light Inspector",                 "Open the light properties inspector", CmdCategory::Entity}},
		{"ToggleTextureBrowser",        {"Texture Browser",                 "Open the texture/material browser", CmdCategory::Texture}},
		{"ToggleTextureTool",           {"Texture Tool",                    "Open the UV texture editing tool", CmdCategory::Texture}},
		{"ToggleLayerControlPanel",     {"Layer Panel",                     "Open the layer management panel"}},
		{"ToggleMapMergePanel",         {"Map Merge Panel",                 "Open the map merge/diff panel", CmdCategory::Map}},
		{"ToggleAasVisualisationPanel", {"AAS Visualisation",               "Open the AAS (AI navigation) visualisation panel"}},
		{"ToggleOrthoBackgroundPanel",  {"Ortho Background",                "Configure background images in 2D views", CmdCategory::View}},

		// Dialogs and editors
		{"Preferences",                 {"Preferences",                     "Open the application preferences"}},
		{"ProjectSettings",             {"Game/Project Setup",              "Configure game paths and project settings"}},
		{"EditColourScheme",            {"Edit Colour Scheme",              "Customize the editor colour scheme", CmdCategory::View}},
		{"MapInfo",                     {"Map Information",                 "Show statistics about the current map", CmdCategory::Map}},
		{"ShowCommandList",             {"Keyboard Shortcuts",              "View and edit keyboard shortcut bindings"}},
		{"CommandPalette",              {"Command Palette",                 "Search and execute commands"}},
		{"MouseToolMappingDialog",      {"Mouse Bindings",                  "View and edit mouse tool bindings"}},
		{"EntityClassTree",             {"Entity Class Tree",               "Browse all available entity classes", CmdCategory::Entity}},
		{"About",                       {"About HellForge",                 "Show version and credits"}},
		{"ShowUserGuide",               {"User Guide",                      "Open the online user documentation"}},
		{"ShowOfflineUserGuide",        {"User Guide (Offline)",            "Open the offline user documentation"}},
		{"OpenForumUrl",                {"Community Forum",                 "Open the community forum in a browser"}},
		{"OpenScriptReference",         {"Script Reference",                "Open the scripting API reference"}},
		{"AnimationPreview",            {"Animation Viewer",                "Preview MD5 model animations"}},
		{"ParticlesEditor",             {"Particle Editor",                 "Create and edit particle effects"}},
		{"SkinEditor",                  {"Skin Editor",                     "Create and edit model skins"}},
		{"MaterialEditor",              {"Material Editor",                 "Create and edit materials", CmdCategory::Texture}},

		// Merge operations
		{"StartMergeOperation",         {"Start Map Merge",                 "Begin a map merge/diff operation", CmdCategory::Map}},
		{"FinishMergeOperation",        {"Finish Map Merge",                "Complete the map merge operation", CmdCategory::Map}},
		{"AbortMergeOperation",         {"Abort Map Merge",                 "Cancel the current map merge", CmdCategory::Map}},

		// Asset reloading
		{"RefreshModels",               {"Reload All Models",               "Re-read all models from disk"}},
		{"RefreshSelectedModels",       {"Reload Selected Models",          "Re-read only selected models from disk"}},
		{"ReloadDefs",                  {"Reload Entity Definitions",       "Re-read entity definitions from disk"}},
		{"ReloadDecls",                 {"Reload Declarations",             "Re-read all declarations from disk"}},
		{"ReloadImages",                {"Reload Textures",                 "Re-read all textures from disk"}},
		{"ReloadReadables",             {"Reload Readables",                "Re-read readable GUI definitions from disk"}},

		// Model export
		{"ExportSelectedAsModelDialog", {"Export Selection as Model",       "Export selected geometry to ASE, LWO, or OBJ", CmdCategory::Entity}},
		{"ExportSelectedAsCollisionModel", {"Export as Collision Model",    "Save selection as a collision model for a mesh", CmdCategory::Entity}},
		{"ExportCollisionModelDialog",  {"Export Collision Model",          "Choose options for collision model export", CmdCategory::Entity}},
		{"ConvertModelDialog",          {"Convert Model Format",            "Convert a model between different formats"}},

		// Toggles
		{"TogTexLock",                  {"Toggle Texture Lock",             "Keep textures in place when moving brushes", CmdCategory::Texture}},
		{"ToggleRotationPivot",         {"Toggle Rotation Pivot",           "Switch rotation pivot between center and origin", CmdCategory::Transform}},
		{"ToggleSnapRotationPivot",     {"Snap Rotation Pivot to Grid",     "Snap the rotation pivot point to the grid", CmdCategory::Transform}},
		{"ToggleOffsetClones",          {"Offset Clones",                   "Offset duplicated objects from the original", CmdCategory::Selection}},
		{"ToggleFreeObjectRotation",    {"Free Object Rotation",            "Allow free rotation instead of axis-locked", CmdCategory::Transform}},
		{"ToggleShowAllLightRadii",     {"Show All Light Radii",            "Display the radius of every light in the map", CmdCategory::View}},
		{"ToggleShowAllSpeakerRadii",   {"Show All Speaker Radii",          "Display the radius of every speaker in the map", CmdCategory::View}},
		{"ToggleShowSizeInfo",          {"Show Size Info",                  "Display dimensions of selected objects", CmdCategory::View}},
		{"ToggleDragResizeEntitiesSymmetrically", {"Symmetric Entity Resize", "Resize entities symmetrically when dragging", CmdCategory::Entity}},
		{"ToggleShadowMapping",         {"Toggle Shadow Mapping",           "Enable or disable shadow mapping in the 3D view", CmdCategory::View}},
		{"ToggleCrosshairs",            {"Toggle Crosshairs",               "Show or hide the crosshair in the 2D view", CmdCategory::View}},
		{"TogglePreview",               {"Toggle Render Preview",           "Toggle the realtime lighting preview", CmdCategory::View}},

		// Display toggles
		{"ShowAxes",                    {"Show Axes",                       "Show the coordinate axes indicator", CmdCategory::View}},
		{"ShowNames",                   {"Show Entity Names",               "Display entity names in the 2D view", CmdCategory::View}},
		{"ShowAngles",                  {"Show Entity Angles",              "Display entity angle indicators", CmdCategory::View}},
		{"ShowBlocks",                  {"Show Region Blocks",              "Display region block boundaries", CmdCategory::View}},
		{"ShowCoordinates",             {"Show Coordinates",                "Display coordinates in the 2D view", CmdCategory::View}},
		{"ShowWorkzone",                {"Show Workzone",                   "Display the current workzone area", CmdCategory::View}},
		{"ShowWindowOutline",           {"Show Window Outline",             "Display outlines around docked windows", CmdCategory::View}},

		// Pointfile
		{"ChooseAndTogglePointfile",    {"Toggle Pointfile",                "Load and display the map leak pointfile", CmdCategory::Map}},

		// Misc
		{"clear",                       {"Clear Console",                   "Clear the console output"}},
		{"RunBenchmark",                {"Run Benchmark",                   "Run a rendering performance benchmark"}},
		{"ShowRenderMemoryStats",       {"Render Memory Stats",             "Show GPU memory usage statistics"}},
		{"SaveAutomaticBackup",         {"Save Automatic Backup",           "Save an automatic backup of the map", CmdCategory::Map}},
		{"ToggleSpacePartitionRendering", {"Show Space Partition",          "Visualise the BSP space partition tree", CmdCategory::View}},

		// Terrain
		{"TerrainGeneratorDialog",      {"Terrain Generator",               "Generate terrain from a heightmap", CmdCategory::Patch}},

		// Game connection (TDM)
		{"GameConnectionBackSyncCamera",  {"Sync Camera from Game",         "Move camera to the player's in-game position", CmdCategory::Camera}},
		{"GameConnectionPauseGame",       {"Pause Game",                    "Pause the connected game instance"}},
		{"GameConnectionReloadMap",       {"Game: Reload Map",              "Tell the game to reload the current map", CmdCategory::Map}},
		{"GameConnectionUpdateMap",       {"Game: Update Map",              "Push map changes to the running game", CmdCategory::Map}},

		// TDM-specific editors
		{"ConversationEditor",          {"Conversation Editor",             "Edit AI conversation scripts", CmdCategory::Entity}},
		{"DifficultyEditor",            {"Difficulty Editor",               "Edit mission difficulty settings"}},
		{"StimResponseEditor",          {"Stim/Response Editor",            "Edit entity stimulus and response setup", CmdCategory::Entity}},
		{"ReadableEditorDialog",        {"Readable Editor",                 "Edit in-game readable documents", CmdCategory::Entity}},
		{"MissionInfoEditDialog",       {"Mission Info Editor",             "Edit mission metadata and description", CmdCategory::Map}},
		{"FixupMapDialog",              {"Fixup Map",                       "Apply batch fixes to the map", CmdCategory::Map}},

		// Git integration
		{"GitFetch",                    {"Git: Fetch",                      "Fetch updates from the remote repository"}},

		// Texture tool
		{"TexToolFlipS",                {"Tex Tool: Flip S",                "Flip selected UV coordinates horizontally", CmdCategory::Texture}},
		{"TexToolFlipT",                {"Tex Tool: Flip T",                "Flip selected UV coordinates vertically", CmdCategory::Texture}},
		{"TexToolMergeItems",           {"Tex Tool: Merge",                 "Merge selected UV vertices", CmdCategory::Texture}},
		{"TexToolNormaliseItems",       {"Tex Tool: Normalize",             "Normalize selected UV coordinates", CmdCategory::Texture}},
		{"TexToolSnapToGrid",           {"Tex Tool: Snap to Grid",          "Snap UV coordinates to the grid", CmdCategory::Texture}},
		{"TexToolSelectRelated",        {"Tex Tool: Select Related",        "Select related UV elements", CmdCategory::Texture}},
		{"TexToolToggleGrid",           {"Tex Tool: Toggle Grid",           "Show or hide the UV grid", CmdCategory::Texture}},
		{"TextureToolResetView",        {"Tex Tool: Reset View",            "Reset the UV editor view", CmdCategory::Texture}},

		// Polygon
		{"FinishPolygon",               {"Finish Polygon",                  "Complete the polygon being drawn", CmdCategory::Brush}},
		{"CancelPolygon",               {"Cancel Polygon",                  "Cancel the polygon being drawn", CmdCategory::Brush}},

		// Array
		{"ArrayCloneSelectionLine",     {"Array: Linear",                   "Clone in a straight line", CmdCategory::Entity}},
		{"ArrayCloneSelectionCircle",   {"Array: Circular",                 "Clone in a circular pattern", CmdCategory::Entity}},
		{"ArrayCloneSelectionSpline",   {"Array: Along Spline",             "Clone along a spline curve", CmdCategory::Entity}},
	};

	return metadata;
}

} // namespace ui
