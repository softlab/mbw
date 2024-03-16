//# (C) Yurii Ingultsov <yuri@softlab.in.ua>

#include <windows.h>

#include <unistd.h>
#include <stdlib.h>
#include "threads.h"


int thrd_create( thrd_t *thread_id, THREAD_FUNC* thread_func, void *thread_func_params) {
    
#ifdef USE_BEGIN_THREAD_EX    
    unsigned threadId = 0;
    thrd_t hThrd = _beginthreadex(
            NULL,
            0 /*stack_size*/,
            thread_func,
            thread_func_params,
            0 /* run immediatelly */,
            &threadId
    );
#elif defined(USE_BEGIN_THREAD)
     thrd_t hThrd = (thrd_t) _beginthread(
        thread_func,
        40960, /*stack_size*/
        thread_func_params
    );    
#else 
    #error "Eithe USE_BEGIN_THREAD or USE_BEGIN_THREAD_EX must be defined"
#endif    
    if (thread_id != NULL) {
        *thread_id = hThrd ;
    }

    return 0;
}

int thrd_join(thrd_t threead_id, void *unused) {
    WaitForSingleObject(threead_id, INFINITE);
    return 0;
}
