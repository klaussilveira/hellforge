#pragma once

#include <string>

namespace wxutil
{

namespace keycode
{

// Converts a string representation of a key to the corresponding wxKeyCode,
// e.g. "A" => 65, "TAB" => WXK_TAB. Names are case-insensitive ("a" also returns 65).
// Returns WXK_NONE if no conversion exists.
unsigned int getKeyCodeFromName(const std::string& name);

// Converts the given keycode to a string suitable for serialisation.
// ASCII characters are uppercased ('a' => 'A').
std::string getNameFromKeyCode(unsigned int keyCode);

}

}
