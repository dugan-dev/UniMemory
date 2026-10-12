#pragma once

#ifndef UNIMEMORY_PERFORMANCE_TESTS
#define UNIMEMORY_PERFORMANCE_TESTS 0
#endif

#if UNIMEMORY_PERFORMANCE_TESTS
#ifndef NDEBUG
#error "Performance acceptance requires a Release build"
#endif
static_assert(UNIMEMORY_CONFIG_STATISTICS == 0, "Performance acceptance requires statistics OFF");
static_assert(UNIMEMORY_CHECKS == 0, "Performance acceptance requires Release without debug checks");
#endif
