#pragma once

#include "plugin.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>

namespace direct_wheel::log
{
    // When false, Debug/DebugF are runtime no-ops regardless of the SDK logger
    // being present. Toggled from config (ffb.debugLogging, etc.) at runtime.
    inline std::atomic<bool> g_debugEnabled{false};

    inline void SetDebugEnabled(bool v) { g_debugEnabled.store(v, std::memory_order_relaxed); }
    inline bool DebugEnabled()          { return g_debugEnabled.load(std::memory_order_relaxed); }

    namespace detail
    {
        using LogFn = void (*)(RED4ext::v1::PluginHandle, const char*);

        inline void Dispatch(LogFn fn, const char* msg)
        {
            auto& ctx = Ctx();
            if (fn && ctx.sdk) fn(ctx.handle, msg);
        }

        inline void DispatchF(LogFn fn, const char* fmt, va_list args)
        {
            if (!fn) return;
            char buf[1024];
            vsnprintf(buf, sizeof(buf), fmt, args);
            Dispatch(fn, buf);
        }

        inline LogFn PickFn(LogFn (*member)(const RED4ext::v1::Logger*))
        {
            auto& ctx = Ctx();
            if (!ctx.sdk || !ctx.sdk->logger) return nullptr;
            return member(ctx.sdk->logger);
        }
    }

    inline void Trace(const char* msg)    { auto& c = Ctx(); if (c.sdk && c.sdk->logger) detail::Dispatch(c.sdk->logger->Trace,    msg); }
    // Debug lines are routed through Info when DebugEnabled() is true, because
    // RED4ext's spdlog sink filters out Debug level messages at the file-sink
    // level regardless of our app-level gate. Promoting to Info guarantees
    // they reach disk. No-op when DebugEnabled() is false.
    inline void Debug(const char* msg)    { if (!DebugEnabled()) return; auto& c = Ctx(); if (c.sdk && c.sdk->logger) detail::Dispatch(c.sdk->logger->Info, msg); }
    inline void Info(const char* msg)     { auto& c = Ctx(); if (c.sdk && c.sdk->logger) detail::Dispatch(c.sdk->logger->Info,     msg); }
    inline void Warn(const char* msg)     { auto& c = Ctx(); if (c.sdk && c.sdk->logger) detail::Dispatch(c.sdk->logger->Warn,     msg); }
    inline void Error(const char* msg)    { auto& c = Ctx(); if (c.sdk && c.sdk->logger) detail::Dispatch(c.sdk->logger->Error,    msg); }
    inline void Critical(const char* msg) { auto& c = Ctx(); if (c.sdk && c.sdk->logger) detail::Dispatch(c.sdk->logger->Critical, msg); }

    inline void TraceF(const char* fmt, ...)    { auto& c = Ctx(); if (!c.sdk || !c.sdk->logger) return; va_list a; va_start(a, fmt); detail::DispatchF(c.sdk->logger->Trace,    fmt, a); va_end(a); }
    inline void DebugF(const char* fmt, ...)    { if (!DebugEnabled()) return; auto& c = Ctx(); if (!c.sdk || !c.sdk->logger) return; va_list a; va_start(a, fmt); detail::DispatchF(c.sdk->logger->Info, fmt, a); va_end(a); }
    inline void InfoF(const char* fmt, ...)     { auto& c = Ctx(); if (!c.sdk || !c.sdk->logger) return; va_list a; va_start(a, fmt); detail::DispatchF(c.sdk->logger->Info,     fmt, a); va_end(a); }
    inline void WarnF(const char* fmt, ...)     { auto& c = Ctx(); if (!c.sdk || !c.sdk->logger) return; va_list a; va_start(a, fmt); detail::DispatchF(c.sdk->logger->Warn,     fmt, a); va_end(a); }
    inline void ErrorF(const char* fmt, ...)    { auto& c = Ctx(); if (!c.sdk || !c.sdk->logger) return; va_list a; va_start(a, fmt); detail::DispatchF(c.sdk->logger->Error,    fmt, a); va_end(a); }
    inline void CriticalF(const char* fmt, ...) { auto& c = Ctx(); if (!c.sdk || !c.sdk->logger) return; va_list a; va_start(a, fmt); detail::DispatchF(c.sdk->logger->Critical, fmt, a); va_end(a); }

    // Human-readable DirectInput HRESULT codes for the common ones we see.
    // Unknown codes come out as 0xNNNNNNNN.
    const char* HresultName(long hr);
}
