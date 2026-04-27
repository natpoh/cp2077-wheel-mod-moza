#pragma once

#include <cstdint>
#include <string_view>

namespace direct_wheel
{
    inline constexpr uint32_t kLogitechVid = 0x046D;

    enum class Model : uint8_t
    {
        Unknown = 0,
        WingmanFormulaYellow,
        WingmanFormulaGp,
        WingmanFormulaForce,
        WingmanFormulaForceGp,
        DrivingForce,
        MomoForce,
        DrivingForcePro,
        G25,
        DrivingForceGt,
        G27,
        G29Native,
        G29Ps,
        G920Variant,
        G920,
        G923Xbox,
        G923PsPc,
        G923Ps,
        G923,
        GProRacingPs,
        GProRacingXbox,
        MomoRacing,
        FormulaVibrationFeedback,
    };

    struct ModelInfo
    {
        uint32_t         pid;
        Model            id;
        std::string_view name;
        bool             ffb_default;
        bool             has_clutch;
        bool             has_shifter;
        bool             has_right_cluster;   // Plus / Minus / Scroll-click cluster on G29/G923/etc.
        bool             has_rev_leds;        // 10-segment rev-strip on top of the wheel
        uint16_t         steering_range_deg;
    };

    // Returns nullptr if the PID is not a supported Logitech G-series wheel.
    const ModelInfo* LookupByPid(uint32_t pid);
}
