#pragma once

namespace direct_wheel::rtti_dump
{
    // Walks the game's RTTI and dumps the methods of vehicle-related classes
    // (anything whose name contains "vehicle" or matches specific targets
    // like PlayerPuppet) to the plugin log. Used for discovery before we
    // commit to specific native-hook targets.
    void DumpVehicleClasses();
}
