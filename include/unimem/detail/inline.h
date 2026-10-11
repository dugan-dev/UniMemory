#pragma once

#if defined(_MSC_VER)
#define UNIMEMORY_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define UNIMEMORY_FORCE_INLINE inline __attribute__((always_inline))
#else
#define UNIMEMORY_FORCE_INLINE inline
#endif
