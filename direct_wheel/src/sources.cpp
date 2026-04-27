#include "sources.h"

#include <atomic>
#include <mutex>

namespace direct_wheel::sources
{
    namespace
    {
        struct State
        {
            std::mutex         mtx;
            Frame              frame{};
            std::atomic<bool>  inVehicle{false};
            std::atomic<float> engineRpmNormalized{0.f};
            std::atomic<bool>  radioActive{false};
        };

        State& S() { static State s; return s; }
    }

    void Publish(const Frame& f)
    {
        auto& st = S();
        std::lock_guard lk(st.mtx);
        st.frame = f;
    }

    Frame Current()
    {
        auto& st = S();
        std::lock_guard lk(st.mtx);
        return st.frame;
    }

    void SetInVehicle(bool v) { S().inVehicle.store(v, std::memory_order_release); }
    bool InVehicle()          { return S().inVehicle.load(std::memory_order_acquire); }

    void SetEngineRpmNormalized(float v)
    {
        const float clamped = (v < 0.f) ? 0.f : (v > 1.f ? 1.f : v);
        S().engineRpmNormalized.store(clamped, std::memory_order_release);
    }
    float EngineRpmNormalized()
    {
        return S().engineRpmNormalized.load(std::memory_order_acquire);
    }

    void SetRadioActive(bool v) { S().radioActive.store(v, std::memory_order_release); }
    bool RadioActive()          { return S().radioActive.load(std::memory_order_acquire); }
}
