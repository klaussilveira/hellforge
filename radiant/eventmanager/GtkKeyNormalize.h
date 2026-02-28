#pragma once

#ifdef __WXGTK__

#include <wx/defs.h>

namespace ui
{

inline int normalizeGtkKeyCode(int keyCode)
{
    // ASCII-range keycodes dont need mapping
    if (keyCode >= 0 && keyCode < 0x100)
    {
        return keyCode;
    }

    switch (keyCode)
    {
    case 0xFF08: return WXK_BACK;
    case 0xFF09: return WXK_TAB;
    case 0xFF0D: return WXK_RETURN;
    case 0xFF1B: return WXK_ESCAPE;
    case 0xFF13: return WXK_PAUSE;
    case 0xFF14: return WXK_SCROLL;

    case 0xFF50: return WXK_HOME;
    case 0xFF51: return WXK_LEFT;
    case 0xFF52: return WXK_UP;
    case 0xFF53: return WXK_RIGHT;
    case 0xFF54: return WXK_DOWN;
    case 0xFF55: return WXK_PAGEUP;
    case 0xFF56: return WXK_PAGEDOWN;
    case 0xFF57: return WXK_END;
    case 0xFF63: return WXK_INSERT;
    case 0xFFFF: return WXK_DELETE;

    case 0xFFBE: return WXK_F1;
    case 0xFFBF: return WXK_F2;
    case 0xFFC0: return WXK_F3;
    case 0xFFC1: return WXK_F4;
    case 0xFFC2: return WXK_F5;
    case 0xFFC3: return WXK_F6;
    case 0xFFC4: return WXK_F7;
    case 0xFFC5: return WXK_F8;
    case 0xFFC6: return WXK_F9;
    case 0xFFC7: return WXK_F10;
    case 0xFFC8: return WXK_F11;
    case 0xFFC9: return WXK_F12;
    case 0xFFCA: return WXK_F13;
    case 0xFFCB: return WXK_F14;
    case 0xFFCC: return WXK_F15;
    case 0xFFCD: return WXK_F16;
    case 0xFFCE: return WXK_F17;
    case 0xFFCF: return WXK_F18;
    case 0xFFD0: return WXK_F19;
    case 0xFFD1: return WXK_F20;
    case 0xFFD2: return WXK_F21;
    case 0xFFD3: return WXK_F22;
    case 0xFFD4: return WXK_F23;
    case 0xFFD5: return WXK_F24;

    // Modifier keys (used in isModifier checks)
    case 0xFFE1: // GDK_KEY_Shift_L
    case 0xFFE2: return WXK_SHIFT;   // GDK_KEY_Shift_R
    case 0xFFE3: // GDK_KEY_Control_L
    case 0xFFE4: return WXK_CONTROL; // GDK_KEY_Control_R
    case 0xFFE5: return WXK_CAPITAL; // GDK_KEY_Caps_Lock
    case 0xFFE9: // GDK_KEY_Alt_L
    case 0xFFEA: return WXK_ALT;     // GDK_KEY_Alt_R
    case 0xFFEB: return WXK_WINDOWS_LEFT;  // GDK_KEY_Super_L
    case 0xFFEC: return WXK_WINDOWS_RIGHT; // GDK_KEY_Super_R
    case 0xFF67: return WXK_WINDOWS_MENU;  // GDK_KEY_Menu
    case 0xFF7F: return WXK_NUMLOCK;

    case 0xFF80: return WXK_NUMPAD_SPACE;
    case 0xFF89: return WXK_NUMPAD_TAB;
    case 0xFF8D: return WXK_NUMPAD_ENTER;
    case 0xFF91: return WXK_NUMPAD_F1;
    case 0xFF92: return WXK_NUMPAD_F2;
    case 0xFF93: return WXK_NUMPAD_F3;
    case 0xFF94: return WXK_NUMPAD_F4;
    case 0xFF95: return WXK_NUMPAD_HOME;
    case 0xFF96: return WXK_NUMPAD_LEFT;
    case 0xFF97: return WXK_NUMPAD_UP;
    case 0xFF98: return WXK_NUMPAD_RIGHT;
    case 0xFF99: return WXK_NUMPAD_DOWN;
    case 0xFF9A: return WXK_NUMPAD_PAGEUP;
    case 0xFF9B: return WXK_NUMPAD_PAGEDOWN;
    case 0xFF9C: return WXK_NUMPAD_END;
    case 0xFF9D: return WXK_NUMPAD_BEGIN;
    case 0xFF9E: return WXK_NUMPAD_INSERT;
    case 0xFF9F: return WXK_NUMPAD_DELETE;
    case 0xFFAA: return WXK_NUMPAD_MULTIPLY;
    case 0xFFAB: return WXK_NUMPAD_ADD;
    case 0xFFAC: return WXK_NUMPAD_SEPARATOR;
    case 0xFFAD: return WXK_NUMPAD_SUBTRACT;
    case 0xFFAE: return WXK_NUMPAD_DECIMAL;
    case 0xFFAF: return WXK_NUMPAD_DIVIDE;
    case 0xFFB0: return WXK_NUMPAD0;
    case 0xFFB1: return WXK_NUMPAD1;
    case 0xFFB2: return WXK_NUMPAD2;
    case 0xFFB3: return WXK_NUMPAD3;
    case 0xFFB4: return WXK_NUMPAD4;
    case 0xFFB5: return WXK_NUMPAD5;
    case 0xFFB6: return WXK_NUMPAD6;
    case 0xFFB7: return WXK_NUMPAD7;
    case 0xFFB8: return WXK_NUMPAD8;
    case 0xFFB9: return WXK_NUMPAD9;
    case 0xFFBD: return WXK_NUMPAD_EQUAL;
    case 0xFE20: return WXK_TAB;

    default:
        return keyCode;
    }
}

} // namespace ui

#else

namespace ui
{

inline int normalizeGtkKeyCode(int keyCode)
{
    return keyCode;
}

} // namespace ui

#endif
