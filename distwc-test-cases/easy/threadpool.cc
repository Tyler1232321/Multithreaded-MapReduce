// threadpool.cc
// Author: John Tyler Beckingham
//
// This is an implementation of a threadpool using pthreads. For more
// information see the readme.md

#include <stdio.h>
#include <pthread.h>
#include <vector>
#include <string>
#include <queue>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include "threadpool.h"


using namespace std;

// Functio to call when creating threads, simply calls Thread_run repeatedly
void* run_thread(void* args_p ) 
{
	ThreadPool_t *tp = (ThreadPool_t*) args_p;
	while(1)
	{
		Thread_run(tp);
	}
}

// Safely gets work and does the work
void *Thread_run(ThreadPool_t *tp){
    ThreadPool_work_t work = *ThreadPool_get_work(tp);
	work.func(work.arg);
	return NULL;
}

// Adds work to the work queue
bool ThreadPool_add_work(ThreadPool_t *tp, thread_func_t func, void *arg){
	// create new work object
    ThreadPool_work_t *task = new ThreadPool_work_t();
    task->func = func;
    task->arg = arg;

    // lock shared data structure
    pthread_mutex_lock(&tp->work_q_lock);
    // push work to queue
    tp->work_q.tasks.push(*task);
    // signal that there is work available and unlock mutex
    pthread_cond_signal(&tp->work_avail);
    pthread_mutex_unlock(&tp->work_q_lock);

    //delete(task);

    return true;
}

//TODO: change logic and names in here
ThreadPool_work_t *ThreadPool_get_work(ThreadPool_t *tp){

	// lock shared data structures
    pthread_mutex_lock(&tp->work_q_lock);

    // if no tasks available, wait for them
    while( tp->work_q.tasks.empty() ){
    	// stop waiting if threadpool is being destroyed
        if( tp->destroy )
        {
        	break;
        }
        pthread_cond_wait(&tp->work_avail, &tp->work_q_lock);
    }
    // kill the thread if threadpool is being destroyed and work is finished
    if(tp->destroy && tp->work_q.tasks.empty()){
        pthread_mutex_unlock(&tp->work_q_lock);
        pthread_cond_signal(&tp->work_avail);
        pthread_exit(NULL);
    }
    // get the work
    ThreadPool_work_t *task = &tp->work_q.tasks.front();
    tp->work_q.tasks.pop();
    pthread_mutex_unlock(&tp->work_q_lock);
    return task;
}

// Safely destroys the threadpool
void ThreadPool_destroy(ThreadPool_t *tp){
    tp->destroy = true;
    for( unsigned i = 0; i < tp->threads.size(); i++ )
    {
    	pthread_t temp_tid = tp->threads[i];
    	pthread_join(temp_tid, NULL);
    }
    delete(tp);
}

// Creates the threadpool, initializes thread mutexes and conditionals, 
// and adds num threads to threadpool
ThreadPool_t *ThreadPool_create(int num){
    ThreadPool_t *thread_pool = new ThreadPool_t();
    pthread_mutex_init(&thread_pool->work_q_lock, NULL);
    pthread_cond_init(&thread_pool->work_avail, NULL);
    for(int i = 0; i < num; i++){
        pthread_t thread;
        pthread_create(&thread, NULL, run_thread, thread_pool);
        thread_pool->threads.push_back(thread);
    }
    return thread_pool;
}

