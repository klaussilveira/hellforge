#!/bin/bash
#
# Renders all SVG files in this directory to PNG, applying
# theme variable substitutions. Generates two icon themes:
#   dark/  - Light icons for dark UI backgrounds
#   light/ - Dark icons for light UI backgrounds
#
# Requirements: rsvg-convert (from librsvg)
#   Ubuntu/Debian: sudo apt install librsvg2-bin
#   Fedora: sudo dnf install librsvg2-tools
#   macOS: brew install librsvg
#
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

OUTPUT_BASE="../install/bitmaps"

# Dark theme: light icons for dark backgrounds (existing scheme)
declare -A THEME_DARK=(
    # Colours
    ["iconStroke"]="#e5e5e5"
    ["iconFill"]="#e5e5e5"
    ["iconStrokeDimmed"]="#525252"
    ["iconFillDimmed"]="#525252"

    # Semantic colours
    ["iconPrimary"]="#b96639"
    ["iconAdd"]="#8cc73d"
    ["iconRemove"]="#ed4247"
    ["iconInsert"]="#4285f5"
    ["xAxis"]="#ed4247"
    ["yAxis"]="#8cc73d"
    ["zAxis"]="#4285f5"

    # Stroke widths
    ["strokeWidth"]="1.5"
    ["strokeWidthThin"]="1"
    ["strokeWidthThick"]="2"
)

# Light theme: dark icons for light backgrounds
declare -A THEME_LIGHT=(
    # Colours
    ["iconStroke"]="#333333"
    ["iconFill"]="#333333"
    ["iconStrokeDimmed"]="#999999"
    ["iconFillDimmed"]="#999999"

    # Semantic colours
    ["iconPrimary"]="#a0522d"
    ["iconAdd"]="#5a9e1e"
    ["iconRemove"]="#d42b30"
    ["iconInsert"]="#2a6fd4"
    ["xAxis"]="#d42b30"
    ["yAxis"]="#5a9e1e"
    ["zAxis"]="#2a6fd4"

    # Stroke widths
    ["strokeWidth"]="1.5"
    ["strokeWidthThin"]="1"
    ["strokeWidthThick"]="2"
)

# Default render size (can be overridden via command line)
SIZE="${1:-24}"

# Check for rsvg-convert
if ! command -v rsvg-convert &> /dev/null; then
    echo "Error: rsvg-convert not found. Please install librsvg:"
    echo "  Ubuntu/Debian: sudo apt install librsvg2-bin"
    echo "  Fedora: sudo dnf install librsvg2-tools"
    echo "  macOS: brew install librsvg"
    exit 1
fi

# Render all SVGs with the given theme into an output directory.
# Usage: render_theme <assoc_array_name> <output_dir>
render_theme() {
    local -n theme_ref=$1
    local out_dir="$2"

    mkdir -p "$out_dir"

    # Build sed replacement arguments
    local sed_args=()
    for key in "${!theme_ref[@]}"; do
        sed_args+=(-e "s|{{${key}}}|${theme_ref[$key]}|g")
    done

    echo "Theme: iconStroke=${theme_ref[iconStroke]}, iconFill=${theme_ref[iconFill]}"
    echo "Output: $out_dir"
    echo ""

    local count=0
    for svg in *.svg; do
        [ -f "$svg" ] || continue

        local png="${out_dir}/${svg%.svg}.png"

        if sed "${sed_args[@]}" "$svg" | rsvg-convert -w "$SIZE" -h "$SIZE" -o "$png"; then
            echo "  $svg -> $png"
            count=$((count + 1))
        else
            echo "  $svg -> FAILED"
        fi
    done

    echo ""
    echo "Rendered $count SVG files."
    echo ""
}

echo "Rendering SVGs to PNGs at ${SIZE}x${SIZE}..."
echo ""

render_theme THEME_DARK "${OUTPUT_BASE}/dark"
render_theme THEME_LIGHT "${OUTPUT_BASE}/light"

echo "Done."
