#include "Accelerator.h"

#include "itextstream.h"
#include <cctype>
#include <wx/defs.h>
#include "wxutil/KeyCode.h"
#include "wxutil/Modifier.h"
#include "string/case_conv.h"

namespace ui
{

Accelerator::Accelerator() :
    _key(0),
    _modifiers(0),
    _isEmpty(true)
{}

// Construct an accelerator out of the key/modifier plus a command
Accelerator::Accelerator(const int key,
                         const unsigned int modifiers) :
    _key(key),
    _modifiers(modifiers),
    _isEmpty(false)
{}

Accelerator Accelerator::CreateEmpty()
{
    return Accelerator();
}

// Returns true if the key/modifier combination matches this accelerator
bool Accelerator::match(const int key, const unsigned int modifiers) const {
    return (_key == key && _modifiers == modifiers);
}

bool Accelerator::match(const IEventPtr& event) const
{
    // Only return true if the internal event is not NULL, otherwise false positives may be returned
    return _event == event && !_event->empty();
}

int Accelerator::getKey() const {
    return _key;
}

unsigned int Accelerator::getModifiers() const {
    return _modifiers;
}

const std::string& Accelerator::getStatement() const
{
    return _statement;
}

void Accelerator::setStatement(const std::string& statement)
{
    _statement = statement;
}

void Accelerator::setKey(const int key)
{
    _key = key;
    _isEmpty = _key != 0;
}

// Make the accelerator use the specified accelerators
void Accelerator::setModifiers(const unsigned int modifiers) {
    _modifiers = modifiers;
}

const IEventPtr& Accelerator::getEvent()
{
    return _event;
}

void Accelerator::setEvent(const IEventPtr& ev)
{
    _event = ev;
}

bool Accelerator::isEmpty() const
{
    return _isEmpty;
}

std::string Accelerator::getString(bool forMenu) const
{
    const std::string keyStr = _key != 0 ? Accelerator::getNameFromKeyCode(_key) : "";

    if (!keyStr.empty())
    {
        // Return a modifier string for a menu
        const std::string modifierStr = forMenu ?
            wxutil::Modifier::GetLocalisedModifierString(_modifiers) :
            wxutil::Modifier::GetModifierString(_modifiers);

        const std::string connector = (forMenu) ? "~" : "+";

        std::string returnValue = modifierStr;
        returnValue += !modifierStr.empty() ? connector : "";
        returnValue += keyStr;

        return returnValue;
    }

    return "";
}

unsigned int Accelerator::getKeyCodeFromName(const std::string& name)
{
    return wxutil::keycode::getKeyCodeFromName(name);
}

std::string Accelerator::getNameFromKeyCode(unsigned int keyCode)
{
    return wxutil::keycode::getNameFromKeyCode(keyCode);
}

}
