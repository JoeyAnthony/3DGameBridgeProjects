#include "srreshade.h"
#include <iostream>

namespace srreshade {

    SR_RESHADE_API void subscribe_to_pocess_events()
    {
        std::cout << "Hello World" << std::endl;
    }
    SR_RESHADE_API void unsubscribe_from_process_events()
    {

    }
    SR_RESHADE_API void set_process_event_callback()
    {

    }
    SR_RESHADE_API void inject_into_process()
    {

    }
}
