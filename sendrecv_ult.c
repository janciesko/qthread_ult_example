/*
 * Copyright (C) by Argonne National Laboratory
 *     See COPYRIGHT in top-level directory
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <qthread.h>
#include <qthread/barrier.h>
#include "mpi.h"

#define DEFAULT_NUM_THREADS     2
#define NUM_LOOP                10
#define BUF_SIZE                16384

int rank;
int size;
int buf_size;
int *send_buf;
int *recv_buf;
int num_loop;
int sum;
int verbose = 1;
int profiling = 1;
double t1, t2;

qt_barrier_t * qb;


#define FIELD_WIDTH 16

typedef struct  {
    int arg;
    aligned_t ret;  
} Args;

static aligned_t thread_send_func(void *arg)
{
    int my_id =  * (int *) arg;
    if (verbose)
        printf("[rank %i, threadId %i]: send\n", rank, my_id);
    
    int i, j;
    for (i = 0; i < num_loop; i++) {
        for (j = 1; j < size; j++) {
            int dest = (rank + j) % size;
            send_buf[0] = rank;
            if (verbose)
                printf("[rank %i, threadId %i]: MPI_Send to %i\n", rank, my_id, dest);
            MPI_Send(send_buf, buf_size, MPI_INT, dest, 0, MPI_COMM_WORLD);
        }
    }

    // Increment barrier counter
    qt_barrier_enter (qb); 

}

static aligned_t thread_recv_func(void *arg)
{
    int my_id = * (int * ) arg;
    if (verbose)
        printf("[rank %i, threadId %i]: receive\n", rank, my_id);

    int i, j;
    for (i = 0; i < num_loop; i++) {
        for (j = 1; j < size; j++) {
            int source = (rank + size - j) % size;
            if (verbose)
                printf("[rank %i, threadId %i]: MPI_Recv from %i\n", rank, my_id, source);
            MPI_Recv(recv_buf, buf_size, MPI_INT, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            sum += recv_buf[0];
        }
    }

    // Increment barrier counter
    qt_barrier_enter (qb);
}

int main(int argc, char *argv[])
{
    int provided;
    int status;
    int num_threads = DEFAULT_NUM_THREADS;
    if (argc > 1)
        num_threads = atoi(argv[1]);
    if (num_threads % 2 != 0) {
        if (rank == 0)
            printf("The number of user-level threads should be even.\n");
        exit(0);
    }
    assert(num_threads >= 0);
    num_loop = NUM_LOOP;
    if (argc > 2)
        num_loop = atoi(argv[2]);
    assert(num_loop >= 0);
    buf_size = BUF_SIZE;
    if (argc > 3)
        buf_size = atoi(argv[3]);
    assert(buf_size >= 0);

    /* Initialize */
    status = qthread_initialize();
    assert(status == QTHREAD_SUCCESS);

    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided != MPI_THREAD_MULTIPLE) {
        printf("Cannot initialize with MPI_THREAD_MULTIPLE.\n");
        return EXIT_FAILURE;
    }

    send_buf = (int *) malloc(buf_size * sizeof(int));
    recv_buf = (int *) malloc(buf_size * sizeof(int));
    Args * args = (Args *) malloc(num_threads * sizeof(Args));

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (profiling)
        t1 = MPI_Wtime();
    
    qb = qt_barrier_create(num_threads, REGION_BARRIER);

    for (int i = 0; i < num_threads; i++) {
        args[i].arg = i + 1;
        if (i % 2 == 0) {
            status = qthread_fork(thread_send_func, &args[i].arg, &args[i].ret);
            assert(status == QTHREAD_SUCCESS);
        } else {
            status = qthread_fork(thread_recv_func, &args[i].arg, &args[i].ret);
            assert(status == QTHREAD_SUCCESS);
        }
    }

    qt_barrier_enter (qb);

    if (profiling) {
        t2 = MPI_Wtime();
        if (rank == 0) {
            fprintf(stdout, "%*s%*f\n", FIELD_WIDTH, "Time", FIELD_WIDTH, t2 - t1);
        }
    }

    /* Finalize */
    free(send_buf);
    free(recv_buf);
    free(args);

    qt_barrier_destroy(qb);
    MPI_Finalize();
    qthread_finalize ();
    return EXIT_SUCCESS;
}
