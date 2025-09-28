#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    struct thread_data * ctx = (struct thread_data*)thread_param;
    usleep(ctx->wait_to_obtain_ms * 1000);
    if (pthread_mutex_lock (ctx->mutex) != 0) {
        perror("Can't lock the mutex");
    }
    usleep(ctx->wait_to_release_ms* 1000);
    if (pthread_mutex_unlock (ctx->mutex) != 0) {
        perror("Can't unlock the mutex");
    }

    ctx->thread_complete_success = true;
    
    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    struct thread_data * ctx = malloc(sizeof(struct thread_data));
    memset (ctx, 0, sizeof(struct thread_data));
    ctx->mutex = mutex;
    ctx->wait_to_obtain_ms = wait_to_obtain_ms;
    ctx->wait_to_release_ms = wait_to_release_ms;
    
    if (pthread_create(thread, NULL, threadfunc, ctx) != 0) {
        perror("Can't create a thread");
        return false;
    }
    
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    return true;
}

