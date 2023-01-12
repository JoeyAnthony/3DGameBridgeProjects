#pragma once

#ifdef SR_RESHADEAPI_EXPORTS
#define SR_RESHADE_API __declspec(dllexport)
#else
#define SR_RESHADE_API __declspec(dllimport)
#endif

namespace srreshade {

    // Subsribe to process events
    extern "C" SR_RESHADE_API void subscribe_to_pocess_events();

    // Subsribe to process events
    extern "C" SR_RESHADE_API void unsubscribe_from_process_events();

    // Subsribe to process events
    extern "C" SR_RESHADE_API void set_process_event_callback();

    // Subsribe to process events
    extern "C" SR_RESHADE_API void inject_into_process();
}
