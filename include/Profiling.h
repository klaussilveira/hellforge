#pragma once

#if defined(HF_TRACY_ENABLED)
    #include <tracy/Tracy.hpp>
#else
    #define ZoneScoped
    #define ZoneScopedN(name)
    #define ZoneScopedC(color)
    #define ZoneScopedNC(name, color)
    #define ZoneText(text, size)
    #define ZoneName(name, size)
    #define ZoneValue(value)
    #define FrameMark
    #define FrameMarkNamed(name)
    #define FrameMarkStart(name)
    #define FrameMarkEnd(name)
    #define TracyPlot(name, value)
    #define TracyMessage(text, size)
    #define TracyMessageL(text)
#endif
