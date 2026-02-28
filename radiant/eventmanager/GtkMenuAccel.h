#pragma once

#ifdef __WXGTK__

class wxMenuItem;

namespace ui
{

// Sets the GtkAccelLabel on a GTK menu item to display the given shortcut.
// This bypasses wxWidgets' accelerator parsing which fails with the "~" separator.
void setGtkAccelLabel(wxMenuItem* item, int wxKeyCode, unsigned int modifiers);

// Clears the GtkAccelLabel shortcut display from a GTK menu item.
void clearGtkAccelLabel(wxMenuItem* item);

} // namespace ui

#endif
