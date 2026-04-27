#include "rtti_dump.h"
#include "logging.h"

#include <RED4ext/RED4ext.hpp>

#include <cstring>
#include <string>

namespace direct_wheel::rtti_dump
{
    namespace
    {
        bool NameLooksVehicular(const char* s)
        {
            if (!s) return false;
            for (const char* p = s; *p; ++p)
            {
                if (_strnicmp(p, "vehicle", 7) == 0) return true;
            }
            return false;
        }

        void DumpClass(RED4ext::CClass* cls)
        {
            if (!cls) return;
            const char* clsName = cls->name.ToString();
            if (!clsName) return;

            const uint32_t funcCount   = cls->funcs.Size();
            const uint32_t staticCount = cls->staticFuncs.Size();
            log::InfoF("[direct_wheel:rtti] class %s (funcs=%u statics=%u size=%u)",
                       clsName, funcCount, staticCount, cls->size);

            for (uint32_t i = 0; i < funcCount; ++i)
            {
                auto* fn = cls->funcs[i];
                if (!fn) continue;
                const char* name = fn->shortName.ToString();
                if (!name) continue;

                const bool isNative = fn->flags.isNative != 0;
                const bool isStatic = fn->flags.isStatic != 0;
                const bool hasBC    = fn->bytecode.bytecode.buffer.size > 0;

                std::string params;
                const uint32_t paramCount = fn->params.Size();
                for (uint32_t p = 0; p < paramCount; ++p)
                {
                    auto* pp = fn->params[p];
                    if (!pp) continue;
                    if (!params.empty()) params += ", ";
                    const char* pname = pp->name.ToString();
                    const char* ptypeName = pp->type ? pp->type->GetName().ToString() : nullptr;
                    params += (pname ? pname : "?");
                    params += ":";
                    params += (ptypeName ? ptypeName : "?");
                }

                const char* retTypeName = "void";
                if (fn->returnType && fn->returnType->type)
                {
                    const char* r = fn->returnType->type->GetName().ToString();
                    if (r) retTypeName = r;
                }

                log::InfoF("[direct_wheel:rtti]   .%s(%s) -> %s%s%s%s",
                           name, params.c_str(), retTypeName,
                           isNative ? " [native]" : "",
                           hasBC ? " [scripted]" : "",
                           isStatic ? " [static]" : "");
            }
        }
    }

    void DumpVehicleClasses()
    {
        auto* rtti = RED4ext::CRTTISystem::Get();
        if (!rtti)
        {
            log::Warn("[direct_wheel:rtti] RTTI system unavailable; skipping vehicle class dump");
            return;
        }

        log::Info("[direct_wheel:rtti] ================ VEHICLE CLASS DUMP ================");

        static const char* const priorityClasses[] = {
            "VehicleComponent",
            "vehicleVehicleComponent",
            "VehicleObject",
            "vehicleBaseObject",
            "vehicleCarBaseObject",
            "VehicleCarObject",
            "VehicleControllerPS",
            "VehicleComponentPS",
            "PlayerPuppet",
            "gameInputActionListener",
            "gameInputListener",
        };

        for (const char* name : priorityClasses)
        {
            if (auto* cls = rtti->GetClass(RED4ext::CName(name)))
            {
                DumpClass(cls);
            }
        }

        auto* scriptableRoot = rtti->GetClass(RED4ext::CName("IScriptable"));
        if (scriptableRoot)
        {
            RED4ext::DynArray<RED4ext::CClass*> classes;
            rtti->GetClasses(scriptableRoot, classes);
            uint32_t swept = 0;
            const uint32_t total = classes.Size();
            for (uint32_t i = 0; i < total; ++i)
            {
                auto* cls = classes[i];
                if (!cls) continue;
                const char* name = cls->name.ToString();
                if (!name || !NameLooksVehicular(name)) continue;
                DumpClass(cls);
                swept++;
            }
            log::InfoF("[direct_wheel:rtti] ============ END VEHICLE DUMP (swept %u of %u classes) ============",
                       swept, total);
        }
        else
        {
            log::Info("[direct_wheel:rtti] (couldn't find IScriptable root, sweep skipped)");
            log::Info("[direct_wheel:rtti] ============ END VEHICLE DUMP ============");
        }
    }
}
