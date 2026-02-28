#ifdef __WXGTK__

#include "GtkMenuAccel.h"

#include <wx/menuitem.h>
#include <wx/gtk/menuitem.h>
#include <wx/defs.h>
#include <gtk/gtk.h>

namespace ui
{

namespace
{

guint wxKeyToGdkKeysym(int wxKey)
{
    if (wxKey >= 'A' && wxKey <= 'Z') {
        return static_cast<guint>(wxKey);
    }

    if (wxKey >= '0' && wxKey <= '9') {
        return static_cast<guint>(wxKey);
    }

    if (wxKey >= 0x20 && wxKey < 0x7F) {
        return static_cast<guint>(wxKey);
    }

    switch (wxKey)
    {
    case WXK_BACK:       return GDK_KEY_BackSpace;
    case WXK_TAB:        return GDK_KEY_Tab;
    case WXK_RETURN:     return GDK_KEY_Return;
    case WXK_ESCAPE:     return GDK_KEY_Escape;
    case WXK_SPACE:      return GDK_KEY_space;
    case WXK_DELETE:     return GDK_KEY_Delete;
    case WXK_INSERT:     return GDK_KEY_Insert;
    case WXK_HOME:       return GDK_KEY_Home;
    case WXK_END:        return GDK_KEY_End;
    case WXK_PAGEUP:     return GDK_KEY_Page_Up;
    case WXK_PAGEDOWN:   return GDK_KEY_Page_Down;
    case WXK_LEFT:       return GDK_KEY_Left;
    case WXK_RIGHT:      return GDK_KEY_Right;
    case WXK_UP:         return GDK_KEY_Up;
    case WXK_DOWN:       return GDK_KEY_Down;
    case WXK_PAUSE:      return GDK_KEY_Pause;
    case WXK_SCROLL:     return GDK_KEY_Scroll_Lock;
    case WXK_CAPITAL:    return GDK_KEY_Caps_Lock;
    case WXK_NUMLOCK:    return GDK_KEY_Num_Lock;
    case WXK_PRINT:      return GDK_KEY_Print;
    case WXK_F1:         return GDK_KEY_F1;
    case WXK_F2:         return GDK_KEY_F2;
    case WXK_F3:         return GDK_KEY_F3;
    case WXK_F4:         return GDK_KEY_F4;
    case WXK_F5:         return GDK_KEY_F5;
    case WXK_F6:         return GDK_KEY_F6;
    case WXK_F7:         return GDK_KEY_F7;
    case WXK_F8:         return GDK_KEY_F8;
    case WXK_F9:         return GDK_KEY_F9;
    case WXK_F10:        return GDK_KEY_F10;
    case WXK_F11:        return GDK_KEY_F11;
    case WXK_F12:        return GDK_KEY_F12;
    case WXK_NUMPAD0:    return GDK_KEY_KP_0;
    case WXK_NUMPAD1:    return GDK_KEY_KP_1;
    case WXK_NUMPAD2:    return GDK_KEY_KP_2;
    case WXK_NUMPAD3:    return GDK_KEY_KP_3;
    case WXK_NUMPAD4:    return GDK_KEY_KP_4;
    case WXK_NUMPAD5:    return GDK_KEY_KP_5;
    case WXK_NUMPAD6:    return GDK_KEY_KP_6;
    case WXK_NUMPAD7:    return GDK_KEY_KP_7;
    case WXK_NUMPAD8:    return GDK_KEY_KP_8;
    case WXK_NUMPAD9:    return GDK_KEY_KP_9;
    case WXK_NUMPAD_ADD:      return GDK_KEY_KP_Add;
    case WXK_NUMPAD_SUBTRACT: return GDK_KEY_KP_Subtract;
    case WXK_NUMPAD_MULTIPLY: return GDK_KEY_KP_Multiply;
    case WXK_NUMPAD_DIVIDE:   return GDK_KEY_KP_Divide;
    case WXK_NUMPAD_DECIMAL:  return GDK_KEY_KP_Decimal;
    case WXK_NUMPAD_ENTER:    return GDK_KEY_KP_Enter;
    case WXK_NUMPAD_DELETE:   return GDK_KEY_KP_Delete;
    case WXK_NUMPAD_INSERT:   return GDK_KEY_KP_Insert;
    default:
        return 0;
    }
}

GdkModifierType wxModifiersToGdk(unsigned int modifiers)
{
    guint gdkMods = 0;

    // wxutil::Modifier::SHIFT = 1 << 6, CONTROL = 1 << 7, ALT = 1 << 8
    if (modifiers & (1 << 6))  gdkMods |= GDK_SHIFT_MASK;
    if (modifiers & (1 << 7))  gdkMods |= GDK_CONTROL_MASK;
    if (modifiers & (1 << 8))  gdkMods |= GDK_MOD1_MASK;

    return static_cast<GdkModifierType>(gdkMods);
}

} // anonymous namespace

void setGtkAccelLabel(wxMenuItem* item, int wxKeyCode, unsigned int modifiers)
{
    if (!item || wxKeyCode == 0)
        return;

    GtkWidget* gtkItem = item->GetMenuItem();
    if (!gtkItem)
        return;

    GtkWidget* child = gtk_bin_get_child(GTK_BIN(gtkItem));
    if (!child || !GTK_IS_ACCEL_LABEL(child))
        return;

    guint gdkKey = wxKeyToGdkKeysym(wxKeyCode);
    GdkModifierType gdkMods = wxModifiersToGdk(modifiers);

    gtk_accel_label_set_accel(GTK_ACCEL_LABEL(child), gdkKey, gdkMods);
}

void clearGtkAccelLabel(wxMenuItem* item)
{
    if (!item)
        return;

    GtkWidget* gtkItem = item->GetMenuItem();
    if (!gtkItem)
        return;

    GtkWidget* child = gtk_bin_get_child(GTK_BIN(gtkItem));
    if (!child || !GTK_IS_ACCEL_LABEL(child))
        return;

    gtk_accel_label_set_accel(GTK_ACCEL_LABEL(child), 0, static_cast<GdkModifierType>(0));
}

} // namespace ui

#endif
