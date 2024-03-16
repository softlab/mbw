# ifndef __MBW_H__
# define __MBW_H__

//# (C) Yurii Ingultsov <yuri@softlab.in.ua>

#include <stddef.h>

#if defined(__GLIBC__)
    # define __NONNULL __nonnull
    # include <threads.h>

    # define THREAD_ABI
    typedef int thrdret_t;

#elif defined(__MSVCRT__)

    #if defined(__MINGW64__) && (__GNUC__ <= 10)
        # include <malloc.h>
        # include "mingw32/threads.h"

        # define __NONNULL(x)
        # define aligned_alloc(alignment, size) _aligned_malloc((size), (alignment))
    
    #endif
#endif


unsigned long long get_availalbe_memory();

/* 
    AVX 265
    copies by 4 of 4k pages, 
    into every page copies by 64 bytes (64 moves/page) chunks. (64 loops per page) ymm0.1, ... ymm6..7
    prefetch every chunk at every loop
*/
__attribute__ ((sysv_abi)) 
size_t mc32nt4p_s(void * restrict dst, const void * restrict src, size_t sz) __NONNULL ((1, 2));


# endif //__MBW_H__
