/*
 * vim: ai ts=4 sts=4 sw=4 cinoptions=>4 expandtab
 */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#include <threads.h>
#include <stdatomic.h>

/* how many runs to average by default */
#define DEFAULT_NR_LOOPS 10

/* we have 3 tests at the moment */
#define MAX_TESTS 3
/* max number opf simulatenous threads to run. default 1*/
#define MAX_THREADS 257

/* default block size for test 2, in bytes */
#define DEFAULT_BLOCK_SIZE 262144

/* test types */
#define TEST_MEMCPY 0
#define TEST_DUMB 1
#define TEST_MCBLOCK 2

/* version number */
#define VERSION "3.0.0"

struct result {
    double elapsed_time;    /* seconds? */
    double bandwidth;       /* memory bandwidth MiB/s */
};

struct worker_results {
    struct result tests[MAX_TESTS];
};

/* this data are shared amound all running threads */
struct worker_params {
    unsigned long long asize; /* array size (elements in array) */
    unsigned long long block_size;
    int *tests;
    int nr_loops;
    double mt;  /* MiBytes transferred == array size in MiB */
    int quiet; /* do not print results after every iteration */
    int showavg; /* show average, -a */

    struct worker_results* thrd_res;

    atomic_int lock;
    atomic_int threads_initialized;
};

/*
 * MBW memory bandwidth benchmark
 *
 * 2006, 2012 Andras.Horvath@gmail.com
 * 2013 j.m.slocum@gmail.com
 * (Special thanks to Stephen Pasich)
 *
 * http://github.com/raas/mbw
 *
 * compile with:
 *			gcc -O -o mbw mbw.c
 *
 * run with eg.:
 *
 *			./mbw 300
 *
 * or './mbw -h' for help
 *
 * watch out for swap usage (or turn off swap)
 */

void usage()
{
    printf("mbw memory benchmark v%s, https://github.com/raas/mbw\n", VERSION);
    printf("Usage: mbw [options] array_size_in_MiB\n");
    printf("Options:\n");
    printf("	-n: number of runs per test (0 to run forever)\n");
    printf("	-a: Don't display average\n");
    printf("	-t: number of threads to run\n");
    printf("	-m%d: memcpy test method\n", TEST_MEMCPY);
    printf("	-m%d: dumb (b[i]=a[i] style) test method\n", TEST_DUMB);
    printf("	-m%d: memcpy test method with fixed block size\n", TEST_MCBLOCK);
    printf("	-f: force run measurements in event if thereare no enough free memory available\n");
    printf("	-b <size>: block size in bytes for -t2 (default: %d)\n", DEFAULT_BLOCK_SIZE);
    printf("	-q: quiet (print average  statistics only)\n");
    printf("    -v  prints statistics after every measurements");
    printf("(will then use two arrays, watch out for swapping)\n");
    printf("'Bandwidth' is amount of data copied over the time this operation took.\n");
    printf("\nThe default is to run all tests available.\n");
}

/* ------------------------------------------------------ */

/* allocate a test array and fill it with data
 * so as to force Linux to _really_ allocate it */
long *make_array(unsigned long long asize)
{
    unsigned long long t;
    unsigned int long_size=sizeof(long);
    long *a;

    a=calloc(asize, long_size);

    if(NULL==a) {
        perror("Error allocating memory");
        exit(1);
    }

    /* make sure both arrays are allocated, fill with pattern */
    for(t=0; t<asize; t++) {
        a[t]=0xaa;
    }
    return a;
}

/* actual benchmark */
/* asize: number of type 'long' elements in test arrays
 * long_size: sizeof(long) cached
 * type: 0=use memcpy, 1=use dumb copy loop (whatever GCC thinks best)
 *
 * return value: elapsed time in seconds
 */
double worker(unsigned long long asize, long *a, long *b, int type, unsigned long long block_size)
{
    unsigned long long t;
    struct timeval starttime, endtime;
    double te;
    unsigned int long_size=sizeof(long);
    /* array size in bytes */
    unsigned long long array_bytes=asize*long_size;

    if(type==TEST_MEMCPY) { /* memcpy test */
        /* timer starts */
        gettimeofday(&starttime, NULL);
        memcpy(b, a, array_bytes);
        /* timer stops */
        gettimeofday(&endtime, NULL);
    } else if(type==TEST_MCBLOCK) { /* memcpy block test */
        char* src = (char*)a;
        char* dst = (char*)b;
        gettimeofday(&starttime, NULL);
        for (t=array_bytes; t >= block_size; t-=block_size, src+=block_size){
            dst=(char *) memcpy(dst, src, block_size) + block_size;
        }
        if(t) {
            dst=(char *) memcpy(dst, src, t) + t;
        }
        gettimeofday(&endtime, NULL);
    } else if(type==TEST_DUMB) { /* dumb test */
        gettimeofday(&starttime, NULL);
        for(t=0; t<asize; t++) {
            b[t]=a[t];
        }
        gettimeofday(&endtime, NULL);
    }

    te=((double)(endtime.tv_sec*1000000-starttime.tv_sec*1000000+endtime.tv_usec-starttime.tv_usec))/1000000;

    return te;
}

/* ------------------------------------------------------ */

const char *testno2str(int testno) {
    switch(testno) {
        case TEST_MEMCPY: return "MEMCPY";
        case TEST_DUMB: return "DUMB";
        case TEST_MCBLOCK: return "MCBLOCK";
        default:
        break;
    }
    return "UNKNOWN";
}

/* pretty print worker's output in human-readable terms */
/* te: elapsed time in seconds
 * mt: amount of transferred data in MiB
 * type: see 'worker' above
 *
 * return value: -
 */
void printout(double te, double mt, int testno)
{
    printf("Method: %s\t", testno2str(testno));
    printf("Elapsed: %.5f\t", te);
    printf("MiB: %.5f\t", mt);
    printf("Copy: %.3f MiB/s\n", mt/te);
    return;
}

int worker_thread(void *v) {
    struct worker_params * params = (struct worker_params *)v;
    long *a, *b; /* the two arrays to be copied from/to */
    unsigned long testno;
    int i;
    double te, te_sum; /* time elapsed */
    
    a=make_array( params->asize );
    b=make_array( params->asize );

    int thread_no = atomic_fetch_add(&params->threads_initialized, 1);
    while( atomic_load(&params->lock) == 0) 
        /* spin wait */;

    /* run all tests requested, the proper number of times */
    for(testno=0; testno<MAX_TESTS; testno++) {
        te_sum=0;
        if(params->tests[testno]) {
            for (i=0; params->nr_loops == 0 || i < params->nr_loops; i++) {
                te=worker( params->asize, a, b, testno, params->block_size );
                te_sum+=te;
		        if ( ! params->quiet ) {
                    printf("%d\t", i);
                    printout(te, params->mt, testno);
		        }
            }
            
            if (params->nr_loops != 0) {
                params->thrd_res[thread_no].tests[testno].elapsed_time = te_sum/params->nr_loops;
                params->thrd_res[thread_no].tests[testno].bandwidth = params->mt/(te_sum/params->nr_loops);

                if( params->showavg) {
                    printf("AVG\t");
                    printout( te_sum/params->nr_loops, params->mt, testno);
                }
            }
        }
    }

    free(a);
    free(b);
    return 0;
}

/* ------------------------------------------------------ */

int main(int argc, char **argv)
{
    unsigned int long_size=0;
    unsigned long long asize=0; /* array size (elements in array) */
    int o; /* getopt options */
    unsigned int num_threads = 1;
    unsigned long mem_avail_mib; /* size of available free memory (MiBs)*/
    unsigned long mem_required_mib;
    int force_mem = 0;
    thrd_t threads[MAX_THREADS];
    int t;
    


    /* options */

    /* how many runs to average? */
    int nr_loops=DEFAULT_NR_LOOPS;
    /* fixed memcpy block size for -t2 */
    unsigned long long block_size=DEFAULT_BLOCK_SIZE;
    /* show average, -a */
    int showavg=1;
    /* what tests to run (-t x) */
    int tests[MAX_TESTS];
    double mt=0; /* MiBytes transferred == array size in MiB */
    int testno = 0;
    int verbose=0; /* prints extra messages */
    int quiet=0; /* do not print results after every test. print only average */

    tests[0]=0;
    tests[1]=0;
    tests[2]=0;

    while((o=getopt(argc, argv, "havqfn:m:b:t:")) != EOF) {
        switch(o) {
            case 'h':
                usage();
                exit(1);
                break;
            case 'a': /* suppress printing average */
                showavg=0;
                break;
            case 'n': /* no. loops */
                nr_loops=strtoul(optarg, (char **)NULL, 10);
                break;
            case 'm': /* test (m)ethod to run */
                testno=strtoul(optarg, (char **)NULL, 10);
                if(testno>MAX_TESTS-1) {
                    printf("Error: test number must be between 0 and %d\n", MAX_TESTS-1);
                    exit(1);
                }
                tests[testno]=1;
                break;
            case 't': /* number of threeads */
                num_threads = strtoul(optarg, (char **)NULL, 10);
                if(num_threads > MAX_THREADS-1 || num_threads < 1) {
                    printf("Error: number of threads must be between 1 and %d but %d specified\n", MAX_THREADS-1, num_threads);
                    exit(1);
                }
                break;
            case 'f':
                force_mem = 1;
                break;
            case 'b': /* block size in bytes*/
                block_size=strtoull(optarg, (char **)NULL, 10);
                if(0>=block_size) {
                    printf("Error: what block size do you mean?\n");
                    exit(1);
                }
                break;
            case 'v': /* verbose */
                verbose=1;
                break;
	        case 'q': /* quiet, do not print every test result */
		        quiet = 1;
                break;
            default:
                break;
        }
    }

    /* default is to run all tests if no specific tests were requested */
    if( (tests[0]+tests[1]+tests[2]) == 0) {
        tests[0]=1;
        tests[1]=1;
        tests[2]=1;
    }

    if( nr_loops==0 && ((tests[0]+tests[1]+tests[2]) != 1) ) {
        printf("Error: nr_loops can be zero if only one test selected!\n");
        exit(1);
    }

    if(optind<argc) {
        mt=strtoul(argv[optind++], (char **)NULL, 10);
    } else {
        printf("Error: no array size given!\n");
        exit(1);
    }

    if(0>=mt) {
        printf("Error: %f array size wrong!\n", mt);
        exit(1);
    }

    /* ------------------------------------------------------ */

    long_size=sizeof(long); /* the size of long on this platform */
    asize=1024*1024/long_size*mt; /* how many longs then in one array? */

    if( asize * long_size < block_size) {
        printf("Error: block size (%llu bytes) larger than array size (%llu bytes)!\n", block_size, asize);
        exit(1);
    }

    /* check if we can fit into available free memory. 
       For measurements one thead requires 2 arays of mt MiB size. 
    */
    mem_avail_mib = (sysconf(_SC_AVPHYS_PAGES)*sysconf(_SC_PAGESIZE))/(1024*1024);
    mem_required_mib = 2 * mt * num_threads;
    if ( (mem_required_mib > mem_avail_mib) && (!force_mem) ) {
        printf("Error: You need %ld MiB for measurements but only (%ld MiB) as available. Use -f to override\n", mem_required_mib, mem_avail_mib);
        exit(1);
    }

    if(verbose) {
        printf("Long uses %d bytes. ", long_size);
        printf("Will start %d threads. ", num_threads);
        printf("Allocating 2*%lld elements = %lld bytes of memory per worker thread.\n", asize, 2*asize*long_size);
        if(tests[2]) {
            printf("Using %lld bytes as blocks for memcpy block copy test.\n", block_size);
        }
        printf("Getting down to business... Doing %d runs per test.\n", nr_loops);
    }

    /* ------------------------------------------------------ */

    struct worker_results *results = calloc(num_threads, sizeof(struct worker_results));
    struct worker_params params = {
        asize, block_size, tests, nr_loops, mt, quiet, showavg, results
    };
    atomic_init(&params.threads_initialized, 0);
    atomic_init(&params.lock, 0);

    for(t = 0; t < num_threads; ++t) {
        thrd_create( &threads[t], worker_thread, &params);
    }

    while ( atomic_load(&params.threads_initialized) != num_threads ) {
        /* spin wait */
    }
    atomic_fetch_add(&params.lock, 1);

    for(t = 0; t < num_threads; ++t) {
        thrd_join(threads[t], NULL);
    }

    /* ------------------------------------------------------ */
    
    double total_bandwidt[MAX_TESTS] = {0};
    double et_min[MAX_TESTS] = {0};
    double et_max[MAX_TESTS] = {0};
    double et_total[MAX_TESTS] = {0};
    for(testno = 0; testno < MAX_TESTS; ++testno) {
        if (params.tests[testno]) {
            for(t = 0; t < num_threads; ++t) {
                total_bandwidt[testno] += params.thrd_res[t].tests[testno].bandwidth;
                double et = params.thrd_res[t].tests[testno].elapsed_time;
                if (et_min[testno] > et || t == 0) 
                    et_min[testno] = et;
                if (et_max[testno] < et || t == 0)
                    et_max[testno] = et;
                et_total[testno] += et;
            }
        }
    }

    /* ------------------------------------------------------ */
    for(testno = 0; testno < MAX_TESTS; ++testno) {
        if (params.tests[testno]) {
            printf("TOT\tMethod: %s\tElapsed min/max/avg/tot: %.5f/%.5f/%.5f/%.5f\tSize: %.2f\tBW: %.2f MiB/s\n",
                testno2str(testno),
                et_min[testno], et_max[testno], et_total[testno]/num_threads, et_total[testno],
                params.mt,
                total_bandwidt[testno]
            );
        }
    }

    free(results);

    return 0;
}

