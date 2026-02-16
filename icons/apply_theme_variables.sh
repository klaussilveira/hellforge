#!/bin/bash
#
# Replaces hardcoded colors in SVG files with theme variable placeholders.
# Run this after adding new SVG icons to convert them to use the theme system.
#
# Usage:
#   ./apply_theme_variables.sh           # Process all SVGs in current directory
#   ./apply_theme_variables.sh icon.svg  # Process specific file(s)
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Replacements to apply (order matters - more specific patterns first)
apply_replacements() {
    sed -i \
        -e 's/stroke="currentColor"/stroke="{{iconStroke}}"/g' \
        -e 's/stroke-width="2"/stroke-width="{{strokeWidth}}"/g' \
        -e 's/stroke-width="1.5"/stroke-width="{{strokeWidth}}"/g' \
        -e 's/stroke-width="1"/stroke-width="{{strokeWidthThin}}"/g' \
        -e 's/fill="currentColor"/fill="{{iconFill}}"/g' \
        -e 's/fill="#fff"/fill="{{iconFill}}"/g' \
        -e 's/fill="#ffffff"/fill="{{iconFill}}"/g' \
        -e 's/fill="#FFF"/fill="{{iconFill}}"/g' \
        -e 's/fill="#FFFFFF"/fill="{{iconFill}}"/g' \
        -e 's/fill="#f9f9f9"/fill="{{iconFill}}"/g' \
        -e 's/fill="white"/fill="{{iconFill}}"/g' \
        "$1"
}

# Determine which files to process
if [ $# -gt 0 ]; then
    files=("$@")
else
    files=(*.svg)
fi

echo "Applying theme variable placeholders to SVGs..."
echo ""

count=0
for svg in "${files[@]}"; do
    [ -f "$svg" ] || continue

    # Check if file has any replaceable patterns
    if grep -qE 'stroke="currentColor"|fill="currentColor"|fill="#fff"|fill="#ffffff"|fill="white"' "$svg"; then
        apply_replacements "$svg"
        echo "  $svg - updated"
        count=$((count + 1))
    else
        echo "  $svg - skipped (no replacements needed)"
    fi
done

echo ""
echo "Done. Updated $count SVG files."
echo ""
echo "Available placeholders:"
echo "  {{iconStroke}}      - Main stroke color (from system theme)"
echo "  {{iconFill}}        - Main fill color (from system theme)"
echo "  {{iconStrokeDimmed}} - Dimmed stroke color"
echo "  {{iconFillDimmed}}   - Dimmed fill color"
echo "  {{iconAdd}}         - Green (for add/create actions)"
echo "  {{iconRemove}}      - Red (for remove/delete actions)"
echo "  {{iconInsert}}      - Blue (for insert actions)"
echo "  {{strokeWidth}}     - Default stroke width (1.5)"
echo "  {{strokeWidthThin}} - Thin stroke width (1)"
echo "  {{strokeWidthThick}} - Thick stroke width (2)"
