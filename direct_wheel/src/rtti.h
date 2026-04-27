#pragma once

namespace direct_wheel::rtti
{
    // Hook up the RED4ext RTTI register / post-register callbacks. Call once
    // from OnLoad after the Sdk pointer is stored.
    void Register();
}
