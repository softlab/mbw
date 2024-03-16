#if defined(__MSVCRT__)
    #include <windows.h>
#endif
#include <unistd.h>
#include <stddef.h>

#if defined(__GLIBC__)
unsigned long long get_availalbe_memory() {
    return (sysconf(_SC_AVPHYS_PAGES)*sysconf(_SC_PAGESIZE));
}

#elif defined(__MSVCRT__)

unsigned long long get_availalbe_memory() {
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof (statex);
    GlobalMemoryStatusEx(&statex);    

   return  statex.ullAvailPhys;
}

#endif




