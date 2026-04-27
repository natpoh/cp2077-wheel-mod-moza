#include "logging.h"

#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>

#include <cstdio>

namespace direct_wheel::log
{
    const char* HresultName(long hr)
    {
        switch (static_cast<HRESULT>(hr))
        {
        // Several DIERR_* codes alias to standard E_* codes; we list the
        // standard name once and DIERR_* uniques below.
        case S_OK:                          return "S_OK";
        case S_FALSE:                       return "S_FALSE";
        case E_FAIL:                        return "E_FAIL (aka DIERR_GENERIC)";
        case E_INVALIDARG:                  return "E_INVALIDARG (aka DIERR_INVALIDPARAM)";
        case E_OUTOFMEMORY:                 return "E_OUTOFMEMORY (aka DIERR_OUTOFMEMORY)";
        case E_NOINTERFACE:                 return "E_NOINTERFACE";
        case E_POINTER:                     return "E_POINTER";
        case E_ACCESSDENIED:                return "E_ACCESSDENIED (aka DIERR_OTHERAPPHASPRIO)";
        case E_HANDLE:                      return "E_HANDLE";
        case DIERR_INPUTLOST:               return "DIERR_INPUTLOST";
        case DIERR_NOTACQUIRED:             return "DIERR_NOTACQUIRED";
        case DIERR_NOTINITIALIZED:          return "DIERR_NOTINITIALIZED";
        case DIERR_UNSUPPORTED:             return "DIERR_UNSUPPORTED";
        case DIERR_DEVICENOTREG:            return "DIERR_DEVICENOTREG";
        case DIERR_BETADIRECTINPUTVERSION:  return "DIERR_BETADIRECTINPUTVERSION";
        case DIERR_OLDDIRECTINPUTVERSION:   return "DIERR_OLDDIRECTINPUTVERSION";
        case DIERR_NOAGGREGATION:           return "DIERR_NOAGGREGATION";
        case DIERR_NOTFOUND:                return "DIERR_NOTFOUND / DIERR_OBJECTNOTFOUND";
        case DIERR_HASEFFECTS:              return "DIERR_HASEFFECTS";
        case DIERR_DEVICEFULL:              return "DIERR_DEVICEFULL";
        case DIERR_MOREDATA:                return "DIERR_MOREDATA";
        case DIERR_NOTDOWNLOADED:           return "DIERR_NOTDOWNLOADED";
        case DIERR_NOTEXCLUSIVEACQUIRED:    return "DIERR_NOTEXCLUSIVEACQUIRED";
        case DIERR_INCOMPLETEEFFECT:        return "DIERR_INCOMPLETEEFFECT";
        case DIERR_ACQUIRED:                return "DIERR_ACQUIRED";
        default: break;
        }
        static thread_local char buf[16];
        std::snprintf(buf, sizeof(buf), "0x%08lX", hr);
        return buf;
    }
}
