#pragma once

#if defined(PLATFORM_QEMU)
    #include "../platform/qemu/include.h"
#elif defined(PLATFORM_OPIRV2)
    #include "../platform/opirv2/include.h"
#else
    #error "No platform defined!"
#endif