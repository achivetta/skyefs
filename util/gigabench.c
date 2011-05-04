/*
 * dir_benchmark.c
 *
 * This program is used to benchmark directory operations, such as 
 * file creates, file lookups, deleting files and moving files from one 
 * directory to another.
 *
 * written by Swapnil V. Patil (svp @ cs.cmu.edu)
 *
 * TODO:
 * - functions:
 *      create: write n-bytes to each file
 *      move: move half the directory to a destination directory
 * - features:
 *      add a progress indicator for every operation
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "gigabench.h"
#include "gigabench_ops.h"

//struct BenchmarkDescriptor bench_desc;
//struct WorkerThreadDescriptor *thread_desc;

//TODO: incorporate this as a command line parameter
// 1 for different FUSE mount points
// 0 for local FS testing
static int diff_mount_points = 1;   
static char dir_entry_name_prefix[MAX_PATH_LEN];

static void usage(char *program_name)
{
    printf("USAGE: %s PARAMETERS\n"
           "where PARAMERTERS are,\n"
           "\t-D <dir_path>       Path name of the directory\n"
           "\t-N <num_files>      Number of file ops per thread (default: 1)\n"
           "\t-T <num_threads>    Number of threads (default: 1)\n"
           "\t-W <workload_type>  Directory workload (default: create)\n"
           "\n",program_name);
}

static void init_settings()
{
    bench_desc.num_threads = 1;
    bench_desc.num_files = 1;
    bench_desc.dir_path = NULL;
    bench_desc.log_file_dir = NULL;

    // hostname is a component used in constructing the file name
    memset(bench_desc.host_name, 0, sizeof(MAX_PATH_LEN));
    if (gethostname(bench_desc.host_name, sizeof(bench_desc.host_name)) < 0) {
        fprintf(stderr, "gethostname() error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    bench_desc.workload_type = CREATE_WORKLOAD;
    bench_desc.mixed_frac = 1.0;

    return;
}

static void print_settings()
{
    printf("Benchmark bench_desc.\n");
    printf("===================\n");
    printf("\tNum Threads: %d\n",bench_desc.num_threads);
    printf("\tNum Files (per thd): %d\n",bench_desc.num_files);
    printf("\tDirectory Path: %s\n",bench_desc.dir_path);
    printf("\tLog File Location: %s\n",bench_desc.log_file_dir);
    
    switch (bench_desc.workload_type) {
        case SCAN_WORKLOAD:
            printf("\tWorkload Type: SCAN\n");
            break;
        case LOOKUP_WORKLOAD:
            printf("\tWorkload Type: LOOKUP\n");
            break;
        case MIXED_WORKLOAD:
            printf("\tWorkload Type: MIXED\n");
            printf("\tCreate-to-Lookup Ratio: %0.2lf\n",bench_desc.mixed_frac);
            break;
        default:
            printf("\tWorkload Type: CREATEs\n");
            break;
    }
}

static double get_time()
{
    struct timeval t;
    gettimeofday(&t, NULL);
    return ((double)t.tv_sec + ((double)(t.tv_usec)/1000000));
}

static void *create_ops_thread(void *arg)
{
    int i;
    double start_time, end_time, curr_time, begin_time, time_diff;
    char file_name[MAX_PATH_LEN] = {0};


    char buf[1024]={0};
    memset(buf,'1',1024);
    //buf[1024-1]='\n';

    struct WorkerThreadDescriptor *my_thread_desc = 
        (struct WorkerThreadDescriptor*)arg;
    
    //XXX: pthread_self() is the real system id; "thread_desc" has logical id
    //int my_thread_id = pthread_self();
    int my_thread_id = my_thread_desc->id;
    
    //struct PerWorkerThreadStats my_thread_stats = thread_stats[my_thread_id];
    //assert(my_thread_stats.id == my_thread_id);
   

    double mult_factor = (double)(((double)MSMT_GRANULARITY)/1000.0);

    int num_seconds_elapsed = 0;
    thread_stats[my_thread_id].inst_throughput[num_seconds_elapsed] = 0;

    start_time = get_time();
    thread_stats[my_thread_id].start_time = start_time;
    for (i=0; i<bench_desc.num_files; i++) {
    
        // create file name for multiple mountpoints on each client
        //
        memset(file_name, 0, sizeof(MAX_PATH_LEN));
        /*
        if (diff_mount_points == 1)
            snprintf(file_name, sizeof(file_name),
                     "%s/%d/%s_%s_th%d_f%d", 
                     bench_desc.dir_path, 
                     my_thread_id, 
                     bench_desc.host_name, kFilePrefix, my_thread_id, i);
        else
            snprintf(file_name, sizeof(file_name),
                     "%s/%s_%d_%s_th%d_f%d", 
                     bench_desc.dir_path, 
                     bench_desc.host_name, getpid(), 
                     kFilePrefix, my_thread_id, i);
        */
        if (diff_mount_points == 1)
            snprintf(file_name, sizeof(file_name),
                     "%s/%d/%s.%d", 
                     bench_desc.dir_path, my_thread_id, 
                     my_thread_desc->filename_prefix, i);
        else
            snprintf(file_name, sizeof(file_name),
                     "%s/%s.%d", 
                     bench_desc.dir_path,
                     my_thread_desc->filename_prefix, i);

        //create a file using mknod(), which doesn't return anything
        //  why not use creat() instead? 
        //  - it returns an FD, which will be needed to write data into file 
        
        begin_time = get_time();

        /*
        //int fd = open(file_name, 'w');
        int fd = creat(file_name, 0666);
        if (fd < 0) {
            fprintf(stdout, "open(%s) error in thread-%d. [%s]\n", 
                    file_name, my_thread_id, strerror(errno));
            pthread_exit(NULL);
        }
        int bytes_written = write(fd, buf, sizeof(buf));
        if (bytes_written < 0) {
            fprintf(stdout, "write(%s) error in thread-%d. [%s]\n", 
                    file_name, my_thread_id, strerror(errno));
            pthread_exit(NULL);
        }
        close(fd);
        */

        if (mknod(file_name, 0666, 0) < 0) {
            fprintf(stdout, "mknod(%s) error in thread-%d. [%s]\n", 
                    file_name, my_thread_id, strerror(errno));
            // XXX: why do we need per-thread log file??
            //close(my_log_fd);
            pthread_exit(NULL);
        }
        curr_time = get_time();

        // throughput stats
        time_diff = curr_time - thread_stats[my_thread_id].start_time;
        if (time_diff > (num_seconds_elapsed+1)*mult_factor) {
            num_seconds_elapsed += 1;
            thread_stats[my_thread_id].inst_throughput[num_seconds_elapsed] = 0;
        }
        thread_stats[my_thread_id].inst_throughput[num_seconds_elapsed] += 1;
        thread_stats[my_thread_id].completed_ops += 1;

        /*TODO:
        // latency stats (in milliseconds)
        thread_stats[my_thread_id].per_op_latency[i] 
            = (curr_time-begin_time)*1000.0;
        */
    }
    end_time = get_time();
    thread_stats[my_thread_id].end_time = end_time;

    // XXX: why do we need per-thread log file??
    //close(my_log_fd);

    //fprintf(stdout, "exiting thread %d ... \n", my_thread_id);
    pthread_exit(NULL);
}

static void* lookup_ops_thread(void *arg)
{
    int i;
    double start_time, end_time, curr_time, begin_time, time_diff;
    char file_name[MAX_PATH_LEN] = {0};

    struct WorkerThreadDescriptor *my_thread_desc = 
        (struct WorkerThreadDescriptor*)arg;
    
    //XXX: pthread_self() is the real system id; "thread_desc" has logical id
    //int my_thread_id = pthread_self();
    int my_thread_id = my_thread_desc->id;
    
    double mult_factor = (double)(((double)MSMT_GRANULARITY)/1000.0);
    int num_seconds_elapsed = 0;
    thread_stats[my_thread_id].inst_throughput[num_seconds_elapsed] = 0;

    int rand_thread_id, rand_file_num;
    start_time = get_time();
    thread_stats[my_thread_id].start_time = start_time;
    for (i=0; i<bench_desc.num_files; i++) {
        struct stat sb;
    
        //randomly choose {a thread-id, a file name} to construct path
        unsigned int seed;
        rand_thread_id = rand_r(&seed)%bench_desc.num_threads;
        rand_file_num = rand_r(&seed)%bench_desc.num_files;

        // create file name for multiple mountpoints on each client
        //
        memset(file_name, 0, sizeof(MAX_PATH_LEN));
        if (diff_mount_points == 1)
            snprintf(file_name, sizeof(file_name),
                     "%s/%d/%s_%s_th%d_f%d", 
                     bench_desc.dir_path, 
                     //rand_thread_id, 
                     my_thread_id,
                     bench_desc.host_name, 
                     kFilePrefix, rand_thread_id, rand_file_num);
        else
            snprintf(file_name, sizeof(file_name),
                     "%s/%s_%s_th%d_f%d", 
                     bench_desc.dir_path, 
                     bench_desc.host_name, 
                     kFilePrefix, rand_thread_id, rand_file_num);

        begin_time = get_time();

        stat(file_name, &sb);
        /*
        if (stat(file_name, &sb) < 0) {
            fprintf(stdout, "stat(%s) error in thread-%d. [%s]\n", 
                    file_name, my_thread_id, strerror(errno));
            // XXX: why do we need per-thread log file??
            //close(my_log_fd);
            pthread_exit(NULL);
        }
        */
        curr_time = get_time();

        // throughput stats
        time_diff = curr_time - thread_stats[my_thread_id].start_time;
        //if (time_diff > num_seconds_elapsed+1) {
        if (time_diff > (num_seconds_elapsed+1)*mult_factor) {
            num_seconds_elapsed += 1;
            thread_stats[my_thread_id].inst_throughput[num_seconds_elapsed] = 0;
        }
        thread_stats[my_thread_id].inst_throughput[num_seconds_elapsed] += 1;
        thread_stats[my_thread_id].completed_ops += 1;

        /* TODO:
        // latency stats (in milliseconds)
        thread_stats[my_thread_id].per_op_latency[i] 
            = (curr_time-begin_time)*1000.0;
        */
    }
    end_time = get_time();
    thread_stats[my_thread_id].end_time = end_time;

    // XXX: why do we need per-thread log file??
    //close(my_log_fd);

    //fprintf(stdout, "exiting thread %d ... \n", my_thread_id);
    pthread_exit(NULL);
}

/*
static void* scan_ops_thread(void *arg)
{
    struct WorkerThreadDescriptor *my_thread_desc = 
        (struct WorkerThreadDescriptor*)arg;
    
    int my_thread_id = my_thread_desc->id;
    
    DIR *pDir;
    struct dirent *pDirEnt;

    if ((pDir = opendir(bench_desc.dir_path)) == NULL) {
        printf("ERR: opendir(%s) for readdir. [%s]\n", 
               bench_desc.dir_path, strerror(errno));
        exit(1);
    }

    thread_stats[my_thread_id].completed_ops = 0;
    thread_stats[my_thread_id].start_time = get_time();
    while (1) {
        errno = 0;
        pDirEnt = readdir(pDir);
        if (pDirEnt) {
            //TODO: do something else here? may be "ls"?
            //how about local rename to emulate local split ops?
            //
            thread_stats[my_thread_id].completed_ops += 1;
        }
        else if (errno != 0) {
            printf("ERR: during readdir(%s), entry=%s. [%s]\n", 
               bench_desc.dir_path, pDirEnt->d_name, strerror(errno));
            exit(1);
        }
        else
            break;
    }
    thread_stats[my_thread_id].end_time = get_time();

    if (closedir(pDir) != 0) {
        printf("ERR: during closedir(%s). [%s]\n", 
                bench_desc.dir_path, strerror(errno));
        exit(1);
    }

    pthread_exit(NULL);
}
*/

static void* scan_ops_thread(void *arg)
{
    struct WorkerThreadDescriptor *my_thread_desc = 
        (struct WorkerThreadDescriptor*)arg;
    
    int my_thread_id = my_thread_desc->id;
    
    thread_stats[my_thread_id].start_time = get_time();
    scan_readdir(bench_desc.dir_path, 0);
    thread_stats[my_thread_id].end_time = get_time();

    pthread_exit(NULL);
}


static void init_threads()
{
    int i;
    int status;
    
    pthread_t *thread;
    pthread_attr_t th_attr;
    //void *join_status;

    // Allocating the thread_desc structure (array of structures) ,and the
    // initializing it with appropriate values.
    //
    thread_desc = calloc(bench_desc.num_threads, 
                         sizeof(struct WorkerThreadDescriptor));
    if (!thread_desc) {
        fprintf(stderr, "calloc() error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    snprintf(dir_entry_name_prefix,
             sizeof(dir_entry_name_prefix),
             "%s/%s_%s", bench_desc.dir_path, kFilePrefix, bench_desc.host_name);

    for (i=0; i<bench_desc.num_threads; i++) {
        thread_desc[i].id = i;
        thread_desc[i].num_files = bench_desc.num_files;
        thread_desc[i].workload_type = bench_desc.workload_type;

        snprintf(thread_desc[i].filename_prefix,
                 sizeof(thread_desc[i].filename_prefix),
                 "%s_host%s_th%d", kLogFilePrefix, bench_desc.host_name, i);
        
        snprintf(thread_desc[i].create_thr_log_file,
                 sizeof(thread_desc[i].create_thr_log_file),
                 "%s/%s_%s_th%d.%s", bench_desc.log_file_dir, 
                 kLogFilePrefix, bench_desc.host_name, i, kCreateThrLogSuffix);
        snprintf(thread_desc[i].create_lat_log_file,
                 sizeof(thread_desc[i].create_lat_log_file),
                 "%s/%s_%s_th%d.%s", bench_desc.log_file_dir, 
                 kLogFilePrefix, bench_desc.host_name, i, kCreateLatLogSuffix);
        snprintf(thread_desc[i].lookup_lat_log_file,
                 sizeof(thread_desc[i].lookup_lat_log_file),
                 "%s/%s_%s_th%d.%s", bench_desc.log_file_dir, 
                 kLogFilePrefix, bench_desc.host_name, i, kLookupLatLogSuffix);
        snprintf(thread_desc[i].lookup_thr_log_file,
                 sizeof(thread_desc[i].lookup_thr_log_file),
                 "%s/%s_%s_th%d.%s", bench_desc.log_file_dir, 
                 kLogFilePrefix, bench_desc.host_name, i, kLookupThrLogSuffix);
    }
 
    // initialize the per-thread statistics collection structures
    //
    init_thread_stats();

    // Allocating the thread book-keeping array
    //
    thread = calloc(bench_desc.num_threads, sizeof(pthread_t));
    if (!thread) {
        fprintf(stderr, "calloc() error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Initialize the thread attributes, particularly make them
    // joinable so that we can collect stats after all threads have
    // completed execution (and before pthread_exit() is called).
    //
    pthread_attr_init(&th_attr);
    pthread_attr_setdetachstate(&th_attr, PTHREAD_CREATE_JOINABLE);

    if (bench_desc.workload_type == CREATE_WORKLOAD) {
        for (i=0; i<bench_desc.num_threads; i++) {
            // Create create() worker threads
            status = pthread_create(&thread[i], &th_attr, 
                                    create_ops_thread, &thread_desc[i]);
            if (status != 0) {
                printf("ERR: pthread_create() during creates. [%s]\n", 
                        strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
    }
    else if (bench_desc.workload_type == LOOKUP_WORKLOAD) {
        for (i=0; i<bench_desc.num_threads; i++) {
            // Create lookup() worker threads
            status = pthread_create(&thread[i], &th_attr, 
                                    lookup_ops_thread, &thread_desc[i]);
            if (status != 0) {
                printf("ERR: pthread_create() during lookups. [%s]\n", 
                        strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
    }
    else if (bench_desc.workload_type == SCAN_WORKLOAD) {
        for (i=0; i<bench_desc.num_threads; i++) {
            // Create readdir() worker threads
            status = pthread_create(&thread[i], &th_attr, 
                                    scan_ops_thread, &thread_desc[i]);
            if (status != 0) {
                printf("ERR: pthread_create() during scan. [%s]\n", 
                        strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
    }

    // Free attribute and wait for the other threads to complete.
    //
    pthread_attr_destroy(&th_attr);
    for (i=0; i<bench_desc.num_threads; i++) {
        //status = pthread_join(thread[i], join_status);
        status = pthread_join(thread[i], NULL);
        if (status != 0) {
            fprintf(stdout, "pthread_join() error: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        //printf("Join successful: Th%d w/ status %ld\n",i,(long)join_status);
    }

    // If this_func() returns before the threads it has created, and exits with 
    // pthread_exit(), the other threads will continue to execute. 
    // Otherwise, they will be terminated when this_fun() finishes.
    //
    
    FILE* my_thr_log_fp = NULL; 
    FILE* my_lat_log_fp = NULL;

    if (bench_desc.workload_type == CREATE_WORKLOAD) {
        for (i=0; i<bench_desc.num_threads; i++) {
            my_thr_log_fp = fopen(thread_desc[i].create_thr_log_file, "w");
            if (my_thr_log_fp == NULL) {
                fprintf(stdout, "[%s] open(%s) error. [%s] \n", __func__, 
                        thread_desc[i].create_thr_log_file, strerror(errno));
                pthread_exit(NULL);
            }

            my_lat_log_fp = fopen(thread_desc[i].create_lat_log_file, "w");
            if (my_lat_log_fp == NULL) {
                fprintf(stdout, "[%s] open(%s) error. [%s] \n", __func__, 
                        thread_desc[i].create_lat_log_file, strerror(errno));
                pthread_exit(NULL);
            }
            
            print_throughput_stats(my_thr_log_fp, i); 
            print_latency_stats(my_lat_log_fp, i); 
            
            fclose(my_thr_log_fp);
            fclose(my_lat_log_fp);
        } 
    }
    else if (bench_desc.workload_type == LOOKUP_WORKLOAD) {
        for (i=0; i<bench_desc.num_threads; i++) {
            my_thr_log_fp = fopen(thread_desc[i].lookup_thr_log_file, "w");
            if (my_thr_log_fp == NULL) {
                fprintf(stdout, "[%s] open(%s) error. [%s] \n", __func__, 
                        thread_desc[i].lookup_thr_log_file, strerror(errno));
                pthread_exit(NULL);
            }

            my_lat_log_fp = fopen(thread_desc[i].lookup_lat_log_file, "w");
            if (my_lat_log_fp == NULL) {
                fprintf(stdout, "[%s] open(%s) error. [%s] \n", __func__, 
                        thread_desc[i].lookup_lat_log_file, strerror(errno));
                pthread_exit(NULL);
            }
            
            print_throughput_stats(my_thr_log_fp, i); 
            print_latency_stats(my_lat_log_fp, i); 
            
            fclose(my_thr_log_fp);
            fclose(my_lat_log_fp);
        } 
    }
    else if (bench_desc.workload_type == SCAN_WORKLOAD) {

    }
    print_aggr_thread_stats(); //print stats before exiting

    cleanup_thread_stats();

    // Free attribute and wait for the other threads to complete.
    pthread_exit(NULL);
}

static void init_thread_stats()
{
    int i, j;
    
    // Allocating the thread_desc structure (array of structures) ,and the
    // initializing it with appropriate values.
    //
    thread_stats = calloc(bench_desc.num_threads, 
                          sizeof(struct PerWorkerThreadStats));
    if (!thread_stats) {
        fprintf(stderr, "calloc() error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    for (i=0; i<bench_desc.num_threads; i++) {
        thread_stats[i].id = i;
        thread_stats[i].start_time = 0.0;
        thread_stats[i].end_time = 0.0;
        thread_stats[i].completed_ops = 0;
        thread_stats[i].inst_throughput = 
            (int *)malloc(MAX_RUN_TIME*sizeof(int));
        if (thread_stats[i].inst_throughput == NULL) {
            fprintf(stdout, "malloc() error during latency tracking. [%s]\n", 
                    strerror(errno));
            exit(EXIT_FAILURE);
        }
        for (j=0; j<MAX_RUN_TIME; j++)
            thread_stats[i].inst_throughput[j] = -1;


        /* TODO:
        thread_stats[i].per_op_latency = 
            (double *)malloc(bench_desc.num_files*sizeof(double));
        if (thread_stats[i].per_op_latency == NULL) {
            fprintf(stdout, "malloc() error during latency tracking. [%s]\n", 
                    strerror(errno));
            exit(EXIT_FAILURE);
        }
        for (j=0; j<bench_desc.num_files; j++)
            thread_stats[i].per_op_latency[j] = -1.0;
        */
    }
}

static void cleanup_thread_stats()
{
    int i;
    for (i=0; i<bench_desc.num_threads; i++) {
        /* TODO: free(thread_stats[i].per_op_latency);*/
        free(thread_stats[i].inst_throughput);
    }

}


static void print_throughput_stats(FILE *log_fp, int thd_id)
{
    int j;
    for (j=0; j<MAX_RUN_TIME; j++) {
        int inst_thru = thread_stats[thd_id].inst_throughput[j];
        if (inst_thru < 0)
            break;
        fprintf(log_fp, "%d\n",inst_thru);
    }
}

static void print_latency_stats(FILE *log_fp, int thd_id)
{
    (void)log_fp;
    (void)thd_id;
/* TODO:
    int j;
    for (j=0; j<bench_desc.num_files; j++) {
        double latency = thread_stats[thd_id].per_op_latency[j];
        if (latency < 0)
            break;
        fprintf(log_fp, "%lf\n", latency);
    }
*/
}

static void print_aggr_thread_stats()
{
    int i;

    printf("\nPrinting per-thread statistics ... \n");
    printf("===================================\n");
    
    for (i=0; i<bench_desc.num_threads; i++) {
        
        int id = thread_stats[i].id;
        int total_ops = thread_stats[i].completed_ops;
        double end = thread_stats[i].end_time;
        double start = thread_stats[i].start_time;

        double total_run_time = end - start;
        double throughput = (double)(((double)(total_ops))/total_run_time);
        
        printf("Thread%d (start=%lf,end=%lf) \n", id, start, end);
        if (total_run_time > 0)
            printf("TotalOps=%d \t Completion time=%lf \t Throughput=%lf \n", 
                    total_ops, total_run_time, throughput); 
        else
            printf("Completion time=ERR \t Throughput=ERR \n"); 

        /*
        printf("Per-second instantaneous throughput ... \n");
        printf("( ");
        
        for (j=0; j<MAX_RUN_TIME; j++) {
            int inst_thru = thread_stats[i].inst_throughput[j];
            if (inst_thru < 0)
                break;
            printf("%d,",inst_thru);
            if (log_fp != NULL)
                fprintf(log_fp, "%d\n",inst_thru);
        }
        
        printf(") \n");
        */
    }
}


int main(int argc, char **argv)
{
    char c;

    init_settings();
    setbuf(stderr,NULL);

    if (argc < 2) {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    while (-1 != (c = getopt(argc, argv,
                             "F:"
                             "D:"
                             "T:"
                             "N:"
                             "W:"
                             "f:"
           ))) {

        switch(c) {
            case 'F':
                bench_desc.log_file_dir = optarg;
                break;
            case 'D':
                bench_desc.dir_path = optarg;
                break;
            case 'T':
                bench_desc.num_threads = atoi(optarg);
                if (bench_desc.num_threads < 1) {
                    fprintf(stderr, 
                            "Number of threads (T) must be greater than 0\n");
                    exit(EXIT_FAILURE);
                }
                if (bench_desc.num_threads > MAX_THREADS) {
                    fprintf(stderr, 
                            "WARNING: Number of threads (T) is too large. "
                            "This is not recommended and may lead to weird " 
                            "behavior.\n");
                }
                break;
            case 'N':
                bench_desc.num_files = atoi(optarg);
                break;
            case 'W':
                if (strcasecmp(optarg, "create") == 0)
                    bench_desc.workload_type = CREATE_WORKLOAD;
                else if (strcasecmp(optarg, "lookup") == 0)
                    bench_desc.workload_type = LOOKUP_WORKLOAD;
                else if (strcasecmp(optarg, "scan") == 0) {
                    bench_desc.workload_type = SCAN_WORKLOAD;
                    bench_desc.num_threads = 1;
                }
                else if (strcasecmp(optarg, "mixed") == 0)
                    bench_desc.workload_type = MIXED_WORKLOAD;
                else {
                    fprintf(stderr,
                            "Invalid workload (w): %s \n"
                            "We only support: create, lookup, scan or mixed."
                            "\n",optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'f':
                if (bench_desc.workload_type == MIXED_WORKLOAD) {
                    bench_desc.mixed_frac = atof(optarg);
                    if ((bench_desc.mixed_frac < 0.0) ||
                        (bench_desc.mixed_frac > 1.0))
                        fprintf(stderr,
                                "range of mixed_frac (f) is [0.0-1.0] \n");
                        exit(EXIT_FAILURE);
                }
                else 
                    fprintf(stderr,
                            "WARNING: mixed_frac (f) is valid only with "
                            "mixed workloads (-O m). We will default it to "
                            "an all create workload. \n");
            default:
                fprintf(stderr, "Illegal parameter: %c\n", c);
                break;
        }
    }

    print_settings();

    init_threads();
    
    return 1;
}


