/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 */

#include "config.h"
#include "PlatformKeyboardEvent.h"

#include "CString.h"
#include "NotImplemented.h"
#include "BlackBerryPlatformKeyboardCodes.h"
#include "BlackBerryPlatformKeyboardEvent.h"
#include "BlackBerryPlatformMisc.h"
#include "WindowsKeyboardCodes.h"

namespace WebCore {

static String keyIdentifierForOlympiaCharacter(unsigned short character)
{
    switch (character) {
    case Olympia::Platform::KEY_ENTER:
    case Olympia::Platform::KEY_ENTERALT:
        return "Enter";
    case Olympia::Platform::KEY_BACKSPACE:
        return "U+0008";
    case Olympia::Platform::KEY_DELETE:
        return "U+007F";
    case Olympia::Platform::KEY_LEFT:
        return "Left";
    case Olympia::Platform::KEY_RIGHT:
        return "Right";
    case Olympia::Platform::KEY_UP:
        return "Up";
    case Olympia::Platform::KEY_DOWN:
        return "Down";
    case Olympia::Platform::KEY_ESCAPE:
        return "Escape";
#if OS(QNX)
    case Olympia::Platform::KEYCODE_PAUSE:
        return "Pause";
    case Olympia::Platform::KEYCODE_PRINT:
        return "PrintScreen";
    case Olympia::Platform::KEY_TAB:
    case Olympia::Platform::KEY_BACK_TAB:
        return "U+0009";
    case Olympia::Platform::KEY_KP_LEFT:
        return "Left";
    case Olympia::Platform::KEY_KP_RIGHT:
        return "Right";
    case Olympia::Platform::KEY_KP_UP:
        return "Up";
    case Olympia::Platform::KEY_KP_DOWN:
        return "Down";
    case Olympia::Platform::KEY_KP_DELETE:
        return "U+007F";
    case Olympia::Platform::KEY_MENU:
    case Olympia::Platform::KEY_LEFT_ALT:
    case Olympia::Platform::KEY_RIGHT_ALT:
        return "Alt";
    case Olympia::Platform::KEY_HOME:
    case Olympia::Platform::KEY_KP_HOME:
        return "Home";
    case Olympia::Platform::KEY_INSERT:
    case Olympia::Platform::KEY_KP_INSERT:
        return "Insert";
    case Olympia::Platform::KEY_PG_UP:
    case Olympia::Platform::KEY_KP_PG_UP:
        return "PageDown";
    case Olympia::Platform::KEY_PG_DOWN:
    case Olympia::Platform::KEY_KP_PG_DOWN:
        return "PageUp";
    case Olympia::Platform::KEY_END:
    case Olympia::Platform::KEY_KP_END:
        return "End";
    case Olympia::Platform::KEY_F1:
        return "F1";
    case Olympia::Platform::KEY_F2:
        return "F2";
    case Olympia::Platform::KEY_F3:
        return "F3";
    case Olympia::Platform::KEY_F4:
        return "F4";
    case Olympia::Platform::KEY_F5:
        return "F5";
    case Olympia::Platform::KEY_F6:
        return "F6";
    case Olympia::Platform::KEY_F7:
        return "F7";
    case Olympia::Platform::KEY_F8:
        return "F8";
    case Olympia::Platform::KEY_F9:
        return "F9";
    case Olympia::Platform::KEY_F10:
        return "F10";
    case Olympia::Platform::KEY_F11:
        return "F11";
    case Olympia::Platform::KEY_F12:
        return "F12";
#endif
    default:
        return String::format("U+%04X", WTF::toASCIIUpper(character));
    }
}

static int windowsKeyCodeForOlympiaCharacter(unsigned short character)
{
    switch (character) {
    case Olympia::Platform::KEY_ENTER:
    case Olympia::Platform::KEY_ENTERALT:
        return VK_RETURN; //(0D) Return key
    case Olympia::Platform::KEY_BACKSPACE:
        return VK_BACK; // (08) BACKSPACE key
    case Olympia::Platform::KEY_DELETE:
        return VK_DELETE; // (2E) DEL key
    case Olympia::Platform::KEY_LEFT:
        return VK_LEFT; // (25) LEFT ARROW key
    case Olympia::Platform::KEY_RIGHT:
        return VK_RIGHT; // (27) RIGHT ARROW key
    case Olympia::Platform::KEY_UP:
        return VK_UP; // (26) UP ARROW key
    case Olympia::Platform::KEY_DOWN:
        return VK_DOWN; // (28) DOWN ARROW key
    case Olympia::Platform::KEY_ESCAPE:
        return VK_ESCAPE;
    case Olympia::Platform::KEY_SPACE:
        return VK_SPACE;
    case '0':
    case ')':
        return VK_0;
    case '1':
    case '!':
        return VK_1;
    case '2':
    case '@':
        return VK_2;
    case '3':
    case '#':
        return VK_3;
    case '4':
    case '$':
        return VK_4;
    case '5':
    case '%':
        return VK_5;
    case '6':
    case '^':
        return VK_6;
    case '7':
    case '&':
        return VK_7;
    case '8':
    case '*':
        return VK_8;
    case '9':
    case '(':
        return VK_9;
    case 'a':
    case 'A':
        return VK_A;
    case 'b':
    case 'B':
        return VK_B;
    case 'c':
    case 'C':
        return VK_C;
    case 'd':
    case 'D':
        return VK_D;
    case 'e':
    case 'E':
        return VK_E;
    case 'f':
    case 'F':
        return VK_F;
    case 'g':
    case 'G':
        return VK_G;
    case 'h':
    case 'H':
        return VK_H;
    case 'i':
    case 'I':
        return VK_I;
    case 'j':
    case 'J':
        return VK_J;
    case 'k':
    case 'K':
        return VK_K;
    case 'l':
    case 'L':
        return VK_L;
    case 'm':
    case 'M':
        return VK_M;
    case 'n':
    case 'N':
        return VK_N;
    case 'o':
    case 'O':
        return VK_O;
    case 'p':
    case 'P':
        return VK_P;
    case 'q':
    case 'Q':
        return VK_Q;
    case 'r':
    case 'R':
        return VK_R;
    case 's':
    case 'S':
        return VK_S;
    case 't':
    case 'T':
        return VK_T;
    case 'u':
    case 'U':
        return VK_U;
    case 'v':
    case 'V':
        return VK_V;
    case 'w':
    case 'W':
        return VK_W;
    case 'x':
    case 'X':
        return VK_X;
    case 'y':
    case 'Y':
        return VK_Y;
    case 'z':
    case 'Z':
        return VK_Z;
    case '+':
    case '=':
        return VK_OEM_PLUS;
    case '-':
    case '_':
        return VK_OEM_MINUS;
    case '<':
    case ',':
        return VK_OEM_COMMA;
    case '>':
    case '.':
        return VK_OEM_PERIOD;
    case ':':
    case ';':
        return VK_OEM_1;
    case '/':
    case '?':
        return VK_OEM_2;
    case '~':
    case '`':
        return VK_OEM_3;
    case '{':
    case '[':
        return VK_OEM_4;
    case '|':
    case '\\':
        return VK_OEM_5;
    case '}':
    case ']':
        return VK_OEM_6;
    case '"':
    case '\'':
        return VK_OEM_7;
#if OS(QNX)
    case Olympia::Platform::KEYCODE_PAUSE:
        return VK_PAUSE;
    case Olympia::Platform::KEYCODE_PRINT:
        return VK_PRINT;
    case Olympia::Platform::KEYCODE_SCROLL_LOCK:
        return VK_SCROLL;
    case Olympia::Platform::KEY_TAB:
    case Olympia::Platform::KEY_BACK_TAB:
        return VK_TAB;
    case Olympia::Platform::KEY_KP_LEFT:
        return VK_LEFT;
    case Olympia::Platform::KEY_KP_RIGHT:
        return VK_RIGHT;
    case Olympia::Platform::KEY_KP_UP:
        return VK_UP;
    case Olympia::Platform::KEY_KP_DOWN:
        return VK_DOWN;
    case Olympia::Platform::KEY_KP_DELETE:
        return VK_DELETE;
    case Olympia::Platform::KEY_MENU:
    case Olympia::Platform::KEY_LEFT_ALT:
    case Olympia::Platform::KEY_RIGHT_ALT:
        return VK_MENU;
    case Olympia::Platform::KEY_HOME:
    case Olympia::Platform::KEY_KP_HOME:
        return VK_HOME;
    case Olympia::Platform::KEY_INSERT:
    case Olympia::Platform::KEY_KP_INSERT:
        return VK_INSERT;
    case Olympia::Platform::KEY_PG_UP:
    case Olympia::Platform::KEY_KP_PG_UP:
        return VK_NEXT;
    case Olympia::Platform::KEY_PG_DOWN:
    case Olympia::Platform::KEY_KP_PG_DOWN:
        return VK_PRIOR;
    case Olympia::Platform::KEY_END:
    case Olympia::Platform::KEY_KP_END:
        return VK_END;
    case Olympia::Platform::KEY_CAPS_LOCK:
        return VK_CAPITAL;
    case Olympia::Platform::KEY_LEFT_SHIFT:
    case Olympia::Platform::KEY_RIGHT_SHIFT:
        return VK_SHIFT;
    case Olympia::Platform::KEY_LEFT_CTRL:
    case Olympia::Platform::KEY_RIGHT_CTRL:
        return VK_CONTROL;
    case Olympia::Platform::KEY_NUM_LOCK:
        return VK_NUMLOCK;
    case Olympia::Platform::KEY_KP_PLUS:
        return VK_ADD;
    case Olympia::Platform::KEY_KP_MINUS:
        return VK_SUBTRACT;
    case Olympia::Platform::KEY_KP_MULTIPLY:
        return VK_MULTIPLY;
    case Olympia::Platform::KEY_KP_DIVIDE:
        return VK_DIVIDE;
    case Olympia::Platform::KEY_KP_FIVE:
        return VK_NUMPAD5;
    case Olympia::Platform::KEY_F1:
        return VK_F1;
    case Olympia::Platform::KEY_F2:
        return VK_F2;
    case Olympia::Platform::KEY_F3:
        return VK_F3;
    case Olympia::Platform::KEY_F4:
        return VK_F4;
    case Olympia::Platform::KEY_F5:
        return VK_F5;
    case Olympia::Platform::KEY_F6:
        return VK_F6;
    case Olympia::Platform::KEY_F7:
        return VK_F7;
    case Olympia::Platform::KEY_F8:
        return VK_F8;
    case Olympia::Platform::KEY_F9:
        return VK_F9;
    case Olympia::Platform::KEY_F10:
        return VK_F10;
    case Olympia::Platform::KEY_F11:
        return VK_F11;
    case Olympia::Platform::KEY_F12:
        return VK_F12;
#endif
    default:
        return 0;
    }
}

unsigned short adjustCharacterFromOS(unsigned short character)
{
#if OS(BLACKBERRY)
    if (character == KEY_ENTERALT) // Convert from line feed to carriage return.
        return KEY_ENTER;
#elif OS(QNX)
    // Use windows key character as ASCII value when possible to enhance readability.
    switch (character) {
    case Olympia::Platform::KEY_BACKSPACE:
        return VK_BACK;
    case Olympia::Platform::KEY_KP_DELETE:
    case Olympia::Platform::KEY_DELETE:
        return 0x7f;
    case Olympia::Platform::KEY_ESCAPE:
        return VK_ESCAPE;
    case Olympia::Platform::KEY_ENTER:
    case Olympia::Platform::KEY_ENTERALT:
        return VK_RETURN;
    case Olympia::Platform::KEY_KP_PLUS:
        return VK_ADD;
    case Olympia::Platform::KEY_KP_MINUS:
        return VK_SUBTRACT;
    case Olympia::Platform::KEY_KP_MULTIPLY:
        return VK_MULTIPLY;
    case Olympia::Platform::KEY_KP_DIVIDE:
        return VK_DIVIDE;
    case Olympia::Platform::KEY_KP_FIVE:
    case Olympia::Platform::KEY_HOME:
    case Olympia::Platform::KEY_KP_HOME:
    case Olympia::Platform::KEY_END:
    case Olympia::Platform::KEY_KP_END:
    case Olympia::Platform::KEY_INSERT:
    case Olympia::Platform::KEY_KP_INSERT:
    case Olympia::Platform::KEY_PG_UP:
    case Olympia::Platform::KEY_KP_PG_UP:
    case Olympia::Platform::KEY_PG_DOWN:
    case Olympia::Platform::KEY_KP_PG_DOWN:
    case Olympia::Platform::KEY_MENU:
    case Olympia::Platform::KEY_LEFT_ALT:
    case Olympia::Platform::KEY_RIGHT_ALT:
    case Olympia::Platform::KEY_CAPS_LOCK:
    case Olympia::Platform::KEY_LEFT_SHIFT:
    case Olympia::Platform::KEY_RIGHT_SHIFT:
    case Olympia::Platform::KEY_LEFT_CTRL:
    case Olympia::Platform::KEY_RIGHT_CTRL:
    case Olympia::Platform::KEY_NUM_LOCK:
    case Olympia::Platform::KEY_F1:
    case Olympia::Platform::KEY_F2:
    case Olympia::Platform::KEY_F3:
    case Olympia::Platform::KEY_F4:
    case Olympia::Platform::KEY_F5:
    case Olympia::Platform::KEY_F6:
    case Olympia::Platform::KEY_F7:
    case Olympia::Platform::KEY_F8:
    case Olympia::Platform::KEY_F9:
    case Olympia::Platform::KEY_F10:
    case Olympia::Platform::KEY_F11:
    case Olympia::Platform::KEY_F12:
        return 0;
    default:
        break;
    }
#endif
    return character;
}

PlatformKeyboardEvent::PlatformKeyboardEvent(Olympia::Platform::KeyboardEvent event)
    : m_autoRepeat(false)
    , m_isKeypad(false)
    , m_shiftKey(event.shiftActive())
    , m_ctrlKey(event.ctrlActive())
    , m_altKey(event.altActive())
    , m_metaKey(false)
{
    m_type = event.keyDown() ? KeyDown : KeyUp;
    m_unmodifiedCharacter = event.character();
    m_keyIdentifier = keyIdentifierForOlympiaCharacter(event.character());
    m_windowsVirtualKeyCode = windowsKeyCodeForOlympiaCharacter(event.character());
    unsigned short character = adjustCharacterFromOS(event.character());
    m_text = String(&character, 1);
    m_unmodifiedText = m_text;

#if OS(QNX)
    if (event.character() == Olympia::Platform::KEY_BACK_TAB)
        m_shiftKey = true; // BackTab should be treated as Shift + Tab.
#endif

    Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "Keyboard event received text=%lc, keyIdentifier=%s, windowsVirtualKeyCode=%d", event.character(), m_keyIdentifier.latin1().data(), m_windowsVirtualKeyCode);
}

bool PlatformKeyboardEvent::currentCapsLockState()
{
    notImplemented();
    return false;
}

void PlatformKeyboardEvent::disambiguateKeyDownEvent(PlatformKeyboardEvent::Type type, bool backwardCompatibilityMode)
{
    // Can only change type from KeyDown to RawKeyDown or Char, as we lack information for other conversions.
    ASSERT(m_type == KeyDown);
    m_type = type;

    if (backwardCompatibilityMode)
        return;

    if (type == RawKeyDown) {
        m_text = String();
        m_unmodifiedText = String();
    } else {
        m_keyIdentifier = String();
        m_windowsVirtualKeyCode = 0;
    }
}

void PlatformKeyboardEvent::getCurrentModifierState(bool& shiftKey, bool& ctrlKey, bool& altKey, bool& metaKey)
{
    notImplemented();
}

} // namespace WebCore
