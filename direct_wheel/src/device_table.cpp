#include "device_table.h"

#include <array>

namespace direct_wheel
{
    namespace
    {
        // Columns: pid, id, name, ffb, clutch, shifter, right_cluster, rev_leds, steering_range_deg
        //
        // right_cluster = the +/- buttons + scroll wheel + scroll-click on the
        // right grip. Present on G29 / G923 / G PRO; absent on G920 (the only
        // modern G-series Xbox wheel that omits them) and on every pre-G29
        // wheel. Driving Force GT has the 24-position rotary dial in roughly
        // the same place but no scroll wheel; we still flag it as having a
        // right cluster so its dial maps to the same UI bindings.
        //
        // rev_leds = the 10-segment LED bar across the top of the wheel.
        // Introduced on the G29; G920 omits it; present on G923 / G PRO.
        constexpr std::array<ModelInfo, 22> kTable = {{
            { 0xC202, Model::WingmanFormulaYellow,     "WingMan Formula (Yellow)",    false, false, false, false, false, 200 },
            { 0xC20E, Model::WingmanFormulaGp,         "WingMan Formula GP",          false, false, false, false, false, 200 },
            { 0xC291, Model::WingmanFormulaForce,      "WingMan Formula Force",       true,  false, false, false, false, 240 },
            { 0xC293, Model::WingmanFormulaForceGp,    "WingMan Formula Force GP",    true,  false, false, false, false, 240 },
            { 0xC294, Model::DrivingForce,             "Driving Force",               false, false, false, false, false, 240 },
            { 0xC295, Model::MomoForce,                "Momo Force",                  true,  false, false, false, false, 270 },
            { 0xC298, Model::DrivingForcePro,          "Driving Force Pro",           true,  false, false, false, false, 900 },
            { 0xC299, Model::G25,                      "G25 Racing Wheel",            true,  true,  true,  false, false, 900 },
            { 0xC29A, Model::DrivingForceGt,           "Driving Force GT",            true,  false, false, true,  false, 900 },
            { 0xC29B, Model::G27,                      "G27 Racing Wheel",            true,  true,  true,  false, false, 900 },
            { 0xC24F, Model::G29Native,                "G29 Driving Force",           true,  true,  false, true,  true,  900 },
            { 0xC260, Model::G29Ps,                    "G29 Driving Force (PS mode)", true,  true,  false, true,  true,  900 },
            { 0xC261, Model::G920Variant,              "G920 Driving Force",          true,  true,  false, false, false, 900 },
            { 0xC262, Model::G920,                     "G920 Driving Force",          true,  true,  false, false, false, 900 },
            { 0xC266, Model::G923Xbox,                 "G923 (Xbox)",                 true,  true,  false, true,  true,  900 },
            { 0xC267, Model::G923PsPc,                 "G923 (PS/PC)",                true,  true,  false, true,  true,  900 },
            { 0xC268, Model::GProRacingPs,             "G PRO Racing Wheel (PS/PC)",  true,  true,  false, true,  true,  1080 },
            { 0xC26D, Model::G923Ps,                   "G923 (PS mode)",              true,  true,  false, true,  true,  900 },
            { 0xC26E, Model::G923,                     "G923 (PC/USB)",               true,  true,  false, true,  true,  900 },
            { 0xC272, Model::GProRacingXbox,           "G PRO Racing Wheel (Xbox/PC)",true,  true,  false, true,  true,  1080 },
            { 0xCA03, Model::MomoRacing,               "Momo Racing",                 true,  false, false, false, false, 240 },
            { 0xCA04, Model::FormulaVibrationFeedback, "Formula Vibration Feedback",  false, false, false, false, false, 200 },
        }};
    }

    const ModelInfo* LookupByPid(uint32_t pid)
    {
        for (const auto& row : kTable)
        {
            if (row.pid == pid) return &row;
        }
        return nullptr;
    }
}
