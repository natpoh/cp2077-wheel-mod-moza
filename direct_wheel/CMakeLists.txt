set(DIRECT_WHEEL_SOURCES
  src/dllmain.cpp
  src/plugin.cpp
  src/device_table.cpp
  src/wheel.cpp
  src/sources.cpp
  src/vehicle_hook.cpp
  src/input_bindings.cpp
  src/kbd_hook.cpp
  src/config.cpp
  src/rtti.cpp
  src/rtti_dump.cpp
  src/rtti_offsets.cpp
  src/logging.cpp
  src/audio_monitor.cpp
  src/led.cpp
  src/mod_settings_seed.cpp
)

set(DIRECT_WHEEL_HEADERS
  src/plugin.h
  src/logging.h
  src/device_table.h
  src/wheel.h
  src/sources.h
  src/vehicle_hook.h
  src/vehicle_physics_ext.h
  src/input_bindings.h
  src/kbd_hook.h
  src/config.h
  src/rtti.h
  src/rtti_dump.h
  src/rtti_offsets.h
  src/audio_monitor.h
  src/led.h
  src/mod_settings_seed.h
  include/direct_wheel_abi.h
)

add_library(direct_wheel SHARED
  ${DIRECT_WHEEL_SOURCES}
  ${DIRECT_WHEEL_HEADERS}
)

target_include_directories(direct_wheel
  PRIVATE
    src
    include
)

target_link_libraries(direct_wheel
  PRIVATE
    RED4ext::SDK
    dinput8
    dxguid
    # WASAPI loopback capture for the LED music visualizer:
    #   ole32     — CoInitializeEx / CoCreateInstance
    #   uuid      — GUID constants for MMDevice / IAudioClient / KSDATAFORMAT_*
    #   mmdevapi  — ActivateAudioInterfaceAsync (per-process loopback API)
    ole32
    uuid
    mmdevapi
)

set_target_properties(direct_wheel PROPERTIES
  OUTPUT_NAME "direct_wheel"
  PREFIX ""
  SUFFIX ".dll"
)
