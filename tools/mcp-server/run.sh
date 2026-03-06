#!/bin/bash
cd "$(dirname "$0")"
exec uv run hellforge_mcp.py "$@"
