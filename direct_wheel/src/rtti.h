#pragma once

namespace direct_wheel::rtti
{
    // Hook up the RED4ext RTTI register / post-register callbacks. Call once
    // from OnLoad after the Sdk pointer is stored.
    void Register();

    // Call once from pump thread when TweakDB is fully loaded to apply
    // configured steering multipliers to all VehicleDriveModelData records.
    // Returns true if records were found and patched.
    bool ApplySteeringMultAllRecords();
}
