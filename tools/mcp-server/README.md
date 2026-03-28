# HellForge MCP Server

An [MCP](https://modelcontextprotocol.io/) server that lets AI
agents operate the HellForge level editor and build maps.

- HellForge MCP Plugin: C++ plugin loaded by HellForge. Starts a TCP server on `localhost:13646` and exposes editor operations as JSON-RPC methods. All operations are dispatched to the main wxWidgets thread for thread safety.
- Python MCP Server: Bridges the MCP protocol to the HellForge plugin. This is what Claude Code connects to.

## Setup

### 1. Configure Claude Code

Add to your project's `.claude/settings.json`:

    {
      "mcpServers": {
        "hellforge": {
          "command": "/path/to/hellforge/tools/mcp-server/run.sh",
          "args": []
        }
      }
    }

The `run.sh` wrapper handles `cd` and `uv run` automatically. Requires [uv](https://docs.astral.sh/uv/) to be installed.

Alternatively, install the package and run directly:

    $ cd tools/mcp-server
    $ uv pip install .

Then configure with:

    {
      "mcpServers": {
        "hellforge": {
          "command": "uv",
          "args": ["run", "hellforge_mcp.py"],
          "cwd": "/path/to/hellforge/tools/mcp-server"
        }
      }
    }

### 2. Launch HellForge

Start HellForge normally. The MCP plugin will begin listening on port 13646. The Python server connects to it automatically when a tool is called.

### 3. Verify

In Claude Code, run `/mcp` to check the server is connected. You should see `hellforge` listed with 75+ tools.

## Example: Creating a Simple Room

    get_game_config() # learn player dimensions
    list_materials(filter="floor", limit=10) # find a floor texture
    list_materials(filter="wall", limit=10) # find a wall texture

    create_brush(min=[-128,-128,0], max=[128,128,8], # floor
         materials={top: "textures/base_floor/clangfloor1",
                    bottom: "textures/common/caulk"},
         material="textures/common/caulk")

    create_brush(min=[-128,-128,120], max=[128,128,128], # ceiling
         material="textures/common/caulk")

    create_brush(min=[-128,-128,0], max=[-120,128,120], # west wall
         material="textures/base_wall/lfwall27")
    create_brush(min=[120,-128,0], max=[128,128,120], # east wall
         material="textures/base_wall/lfwall27")
    create_brush(min=[-128,-128,0], max=[128,-120,120], # south wall
         material="textures/base_wall/lfwall27")
    create_brush(min=[-128,120,0], max=[128,128,120], # north wall
         material="textures/base_wall/lfwall27")

    create_entity(classname="light", origin=[0,0,96],
          properties={"light_radius": "200 200 200"})
    create_entity(classname="info_player_start", origin=[0,0,16])

    query_region(min=[-8,-8,0], max=[8,8,74]) # verify spawn is clear
    save_map(path="/path/to/my_room.map")

## Troubleshooting

MCP server not showing in `/mcp`:
- Check that `run.sh` is executable (`chmod +x tools/mcp-server/run.sh`)
- Check that `uv` is installed and on PATH
- Try running `uv run hellforge_mcp.py` manually from `tools/mcp-server/` to see errors

Tools return "Connection error":
- Make sure HellForge is running with the MCP plugin loaded
- Check the HellForge console for `[MCP] Listening on localhost:13646`
- Verify nothing else is using port 13646

`list_materials` returns empty:
- This requires HellForge to have loaded a game configuration with .mtr files
- Make sure a game is configured and the VFS paths are set
