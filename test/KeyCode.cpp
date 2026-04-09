/**
 * Tests for the wxutil::keycode string<->wxKeyCode conversion utilities.
 * These do not require a Radiant environment.
 */

#include "gtest/gtest.h"

#include "wxutil/KeyCode.h"

#include <wx/defs.h>

namespace test
{

TEST(KeyCodeTest, EmptyStringReturnsNone)
{
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName(""), static_cast<unsigned int>(WXK_NONE));
}

TEST(KeyCodeTest, SingleCharIsUppercased)
{
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("a"), static_cast<unsigned int>('A'));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("A"), static_cast<unsigned int>('A'));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("z"), static_cast<unsigned int>('Z'));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("Z"), static_cast<unsigned int>('Z'));
}

TEST(KeyCodeTest, SingleDigitIsLiteralAscii)
{
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("0"), static_cast<unsigned int>('0'));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("5"), static_cast<unsigned int>('5'));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("9"), static_cast<unsigned int>('9'));
}

TEST(KeyCodeTest, NamedSpecialKeys)
{
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("SPACE"),     static_cast<unsigned int>(WXK_SPACE));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("BACKSPACE"), static_cast<unsigned int>(WXK_BACK));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("ESCAPE"),    static_cast<unsigned int>(WXK_ESCAPE));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("TAB"),       static_cast<unsigned int>(WXK_TAB));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("RETURN"),    static_cast<unsigned int>(WXK_RETURN));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("DELETE"),    static_cast<unsigned int>(WXK_DELETE));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("INSERT"),    static_cast<unsigned int>(WXK_INSERT));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("HOME"),      static_cast<unsigned int>(WXK_HOME));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("END"),       static_cast<unsigned int>(WXK_END));
}

TEST(KeyCodeTest, ArrowKeys)
{
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("UP"),    static_cast<unsigned int>(WXK_UP));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("DOWN"),  static_cast<unsigned int>(WXK_DOWN));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("LEFT"),  static_cast<unsigned int>(WXK_LEFT));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("RIGHT"), static_cast<unsigned int>(WXK_RIGHT));
}

TEST(KeyCodeTest, FunctionKeys)
{
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("F1"),  static_cast<unsigned int>(WXK_F1));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("F5"),  static_cast<unsigned int>(WXK_F5));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("F12"), static_cast<unsigned int>(WXK_F12));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("F24"), static_cast<unsigned int>(WXK_F24));
}

TEST(KeyCodeTest, NumpadDigits)
{
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("NUMPAD0"), static_cast<unsigned int>(WXK_NUMPAD0));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("NUMPAD5"), static_cast<unsigned int>(WXK_NUMPAD5));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("NUMPAD9"), static_cast<unsigned int>(WXK_NUMPAD9));
}

TEST(KeyCodeTest, PageDownAliases)
{
    // These three names should all map to WXK_PAGEDOWN
    auto pagedown = static_cast<unsigned int>(WXK_PAGEDOWN);
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("NEXT"),      pagedown);
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("PAGE_DOWN"), pagedown);
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("PAGEDOWN"),  pagedown);
}

TEST(KeyCodeTest, PageUpAliases)
{
    auto pageup = static_cast<unsigned int>(WXK_PAGEUP);
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("PRIOR"),   pageup);
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("PAGE_UP"), pageup);
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("PAGEUP"),  pageup);
}

TEST(KeyCodeTest, NumpadAliases)
{
    // KP_ and NUMPAD_ prefixes should both work
    auto sub = static_cast<unsigned int>(WXK_NUMPAD_SUBTRACT);
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("KP_SUBTRACT"),     sub);
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("NUMPAD_SUBTRACT"), sub);

    auto add = static_cast<unsigned int>(WXK_NUMPAD_ADD);
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("KP_ADD"),     add);
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("NUMPAD_ADD"), add);
}

TEST(KeyCodeTest, IsoLeftTabMapsToTab)
{
    // ISO_LEFT_TAB is a legacy GDK alias for TAB
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("ISO_LEFT_TAB"),
              static_cast<unsigned int>(WXK_TAB));
}

TEST(KeyCodeTest, PunctuationNames)
{
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("PERIOD"),    static_cast<unsigned int>('.'));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("COMMA"),     static_cast<unsigned int>(','));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("MINUS"),     static_cast<unsigned int>('-'));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("PLUS"),      static_cast<unsigned int>('+'));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("BACKSLASH"), static_cast<unsigned int>('\\'));
}

TEST(KeyCodeTest, CaseInsensitive)
{
    // Multi-character names should be matched case-insensitively
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("space"), static_cast<unsigned int>(WXK_SPACE));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("Tab"),   static_cast<unsigned int>(WXK_TAB));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("f1"),    static_cast<unsigned int>(WXK_F1));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("Up"),    static_cast<unsigned int>(WXK_UP));
}

TEST(KeyCodeTest, UnknownNameReturnsNone)
{
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("BOGUS_KEY"),
              static_cast<unsigned int>(WXK_NONE));
    EXPECT_EQ(wxutil::keycode::getKeyCodeFromName("F99"),
              static_cast<unsigned int>(WXK_NONE));
}

TEST(KeyCodeTest, NameFromCodeNoneReturnsEmpty)
{
    EXPECT_EQ(wxutil::keycode::getNameFromKeyCode(WXK_NONE), "");
}

TEST(KeyCodeTest, NameFromCodeNamedKeys)
{
    EXPECT_EQ(wxutil::keycode::getNameFromKeyCode(WXK_SPACE),  "SPACE");
    EXPECT_EQ(wxutil::keycode::getNameFromKeyCode(WXK_BACK),   "BACKSPACE");
    EXPECT_EQ(wxutil::keycode::getNameFromKeyCode(WXK_ESCAPE), "ESCAPE");
    EXPECT_EQ(wxutil::keycode::getNameFromKeyCode(WXK_TAB),    "TAB");
    EXPECT_EQ(wxutil::keycode::getNameFromKeyCode(WXK_RETURN), "RETURN");
    EXPECT_EQ(wxutil::keycode::getNameFromKeyCode(WXK_F1),     "F1");
    EXPECT_EQ(wxutil::keycode::getNameFromKeyCode(WXK_F12),    "F12");
    EXPECT_EQ(wxutil::keycode::getNameFromKeyCode(WXK_UP),     "UP");
    EXPECT_EQ(wxutil::keycode::getNameFromKeyCode(WXK_DELETE), "DELETE");
}

TEST(KeyCodeTest, NameFromCodeAsciiUppercased)
{
    // ASCII characters should round-trip uppercased
    EXPECT_EQ(wxutil::keycode::getNameFromKeyCode('A'), "A");
    EXPECT_EQ(wxutil::keycode::getNameFromKeyCode('a'), "A");
    EXPECT_EQ(wxutil::keycode::getNameFromKeyCode('z'), "Z");
    EXPECT_EQ(wxutil::keycode::getNameFromKeyCode('5'), "5");
}

TEST(KeyCodeTest, NameFromCodePunctuation)
{
    EXPECT_EQ(wxutil::keycode::getNameFromKeyCode('.'), "PERIOD");
    EXPECT_EQ(wxutil::keycode::getNameFromKeyCode(','), "COMMA");
    EXPECT_EQ(wxutil::keycode::getNameFromKeyCode('-'), "MINUS");
    EXPECT_EQ(wxutil::keycode::getNameFromKeyCode('+'), "PLUS");
}

TEST(KeyCodeTest, RoundTripCanonicalNames)
{
    // For canonical names (the ones returned by getNameFromKeyCode),
    // name -> code -> name should be the identity function.
    const std::vector<std::string> canonicalNames = {
        "SPACE", "BACKSPACE", "ESCAPE", "TAB", "RETURN", "PAUSE",
        "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
        "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24",
        "UP", "DOWN", "LEFT", "RIGHT",
        "DELETE", "INSERT", "END", "HOME",
        "PAGEUP", "PAGEDOWN",
        "NUMPAD0", "NUMPAD1", "NUMPAD2", "NUMPAD3", "NUMPAD4",
        "NUMPAD5", "NUMPAD6", "NUMPAD7", "NUMPAD8", "NUMPAD9",
        "NUMPAD_ADD", "NUMPAD_SUBTRACT", "NUMPAD_MULTIPLY", "NUMPAD_DIVIDE",
        "NUMLOCK", "SCROLL",
        "PERIOD", "COMMA", "MINUS", "PLUS",
    };

    for (const auto& name : canonicalNames)
    {
        unsigned int code = wxutil::keycode::getKeyCodeFromName(name);
        EXPECT_NE(code, static_cast<unsigned int>(WXK_NONE)) << "Name '" << name << "' did not resolve";
        EXPECT_EQ(wxutil::keycode::getNameFromKeyCode(code), name)
            << "Round-trip failed for '" << name << "'";
    }
}

TEST(KeyCodeTest, RoundTripAsciiLetters)
{
    // ASCII letters: 'a' parses to 'A', 'A' formats back to "A"
    for (char c = 'A'; c <= 'Z'; ++c)
    {
        std::string name(1, c);
        unsigned int code = wxutil::keycode::getKeyCodeFromName(name);
        EXPECT_EQ(code, static_cast<unsigned int>(c)) << "Lookup failed for '" << name << "'";
        EXPECT_EQ(wxutil::keycode::getNameFromKeyCode(code), name)
            << "Round-trip failed for '" << name << "'";
    }
}

TEST(KeyCodeTest, RoundTripDigits)
{
    for (char c = '0'; c <= '9'; ++c)
    {
        std::string name(1, c);
        unsigned int code = wxutil::keycode::getKeyCodeFromName(name);
        EXPECT_EQ(code, static_cast<unsigned int>(c)) << "Lookup failed for '" << name << "'";
        EXPECT_EQ(wxutil::keycode::getNameFromKeyCode(code), name)
            << "Round-trip failed for '" << name << "'";
    }
}

}
