#include <vector>
#include <string>
#include <cstring>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <iostream>
#include <sys/types.h>
#include <sys/syscall.h>
#include "threadpool.h"

using namespace std;

void* idle_thread(void* args_p ) 
{
	ThreadPool_t *tp = (ThreadPool_t*) args_p;
	while(1)
	{
		Thread_run(tp);
	}
}

void *Thread_run(ThreadPool_t *tp)
{
	ThreadPool_work_t work = *ThreadPool_get_work(tp);
	work.func(work.arg);
	// the thread can be safely cancelled now
	pthread_setcancelstate( PTHREAD_CANCEL_ENABLE, NULL );
	return NULL;
}

ThreadPool_work_t *ThreadPool_get_work(ThreadPool_t *tp)
{
	if (pthread_mutex_lock(&tp->work_q_lock) != 0 )
	{
		cout << "mut fail 1" << endl;
	}
	while ( tp->queue.work_q.empty())
	{
		pthread_cond_wait(&tp->work_avail, &tp->work_q_lock);
	}
	// we dont want the thread to be canceled while it is doing work
	pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, NULL );

	ThreadPool_work_t work = tp->queue.work_q.front();
	tp->queue.work_q.pop();

	// we can unlock the work_q now
	pthread_mutex_unlock(&tp->work_q_lock);	

	return &work;
}


ThreadPool_t *ThreadPool_create(int num)
{
	ThreadPool_t *thread_pool = new ThreadPool_t();
	thread_pool->work_q_lock = PTHREAD_MUTEX_INITIALIZER;
	thread_pool->work_avail = PTHREAD_COND_INITIALIZER;
    pthread_mutex_init(&thread_pool->work_q_lock, NULL);
    pthread_cond_init(&thread_pool->work_avail, NULL);
	for( int i = 0; i < num; i++ )
	{
		pthread_t *thread = new pthread_t();
		pthread_create(thread, NULL, idle_thread, thread_pool);
		thread_pool->threads.push_back(*thread);
	}

	return thread_pool;
}

void ThreadPool_destroy(ThreadPool_t *tp)
{
	delete(tp);
}

bool ThreadPool_add_work(ThreadPool_t *tp, thread_func_t func, void *arg)
{
	ThreadPool_work_t *work = new ThreadPool_work_t();
	work->func = func;
	work->arg = (char *)arg;
	pthread_mutex_lock(&tp->work_q_lock);
	tp->queue.work_q.push( *work );
	
	pthread_mutex_unlock(&tp->work_q_lock);
	pthread_cond_signal(&tp->work_avail);

	delete(work);
}



