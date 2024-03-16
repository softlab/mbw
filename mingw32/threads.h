# ifndef __THREADS_H__
# define __THREADS_H__
//# (C) Yurii Ingultsov <yuri@softlab.in.ua>


# define USE_BEGIN_THREAD
#if defined( USE_BEGIN_THREAD_EX ) 

# define THREAD_ABI __attribute__((stdcall)) 
typedef unsigned int thrdret_t;

#elif defined( USE_BEGIN_THREAD ) 

# define THREAD_ABI 
typedef void thrdret_t;

#endif

typedef void * thrd_t;

typedef THREAD_ABI thrdret_t THREAD_FUNC(void *);


int thrd_create( thrd_t *threead_id, THREAD_FUNC* thread_func, void *thread_func_params);
int thrd_join(thrd_t threead_id, void *unused);


# endif //__THREADS_H__