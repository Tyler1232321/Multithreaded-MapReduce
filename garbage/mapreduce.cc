#include <vector>
#include <string>
#include <cstring>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <iostream>
#include <map>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <errno.h>
#include <stdlib.h>

#include "threadpool.h"
#include "mapreduce.h"


using namespace std;

typedef struct partition
{
	vector< map< string, vector< string > > > part;
	vector<pthread_mutex_t> part_locks;
	vector<int> indeces;
	int num_partitions;
} partition;

struct partition my_partition;
Reducer red;

bool compare_file_size(string file1, string file2)
{
	struct stat stat1;
	struct stat stat2;
	if(stat(file1.c_str(), &stat1) != 0) {
        return 0;
    }
    if(stat(file2.c_str(), &stat2) != 0) {
        return 0;
    }
    return stat1.st_size > stat2.st_size;
}

unsigned long MR_Partition(char *key, int num_partitions) { 
	unsigned long hash = 5381;
	int c;
	while ((c = *key++) != '\0')
		hash = hash * 33 + c; 
	return hash % num_partitions;
}

void MR_Emit(char *key, char *value)
{
	unsigned long index = MR_Partition( key, my_partition.num_partitions );

	pthread_mutex_lock(&my_partition.part_locks[index]);
	string s = key;
	my_partition.part[index][s].push_back(value);
	pthread_mutex_unlock(&my_partition.part_locks[index]);

}

char *MR_GetNext(char *key, int partition_number)
{
	string s = key;
	int index = my_partition.indeces[partition_number];

	if( index == my_partition.part[partition_number][s].size() )
	{
		cout << s << endl;
		return NULL;
	}
	return (char*)my_partition.part[partition_number][s][index].c_str();
}

void print_partitions()
{
	for( int i = 0; i < my_partition.num_partitions; i++ )
	{
		cout << "Partition " << i << endl;
		::map< string, vector<string> >::iterator it;
		for(it = my_partition.part[i].begin(); it != my_partition.part[i].end(); it++)
		{
		    cout << "Key " << it->first << endl;
		    for( int j = 0; j < it->second.size(); j++ )
		    {
		    	cout << it->second[j] << ", ";
		    }
		    cout << endl;
		}
	}
}

void MR_ProcessPartition(int partition_number)
{
	::map< string, vector<string> >::iterator it;
	for(it = my_partition.part[partition_number].begin(); it != my_partition.part[partition_number].end(); it++)
	{
	    red( (char*)it->first.c_str(), partition_number );
	}
	pthread_exit(NULL);
}

void MR_Run(int num_files, char *filenames[],
            Mapper map, int num_mappers,
            Reducer concate, int num_reducers)
{
	red = concate;
	// initializing shared data structure is safe
	for( int i = 0; i < num_reducers; i++ )
	{
		my_partition.part.push_back( ::map< string, vector< string > >() );
		my_partition.indeces.push_back( 0 );
		
		my_partition.part_locks.push_back( PTHREAD_MUTEX_INITIALIZER );
		pthread_mutex_init( &my_partition.part_locks[i], NULL );
	}
	my_partition.num_partitions = num_reducers;

	ThreadPool_t *tp = ThreadPool_create( num_mappers );
	cout << num_mappers << " threads created" << endl;

	sort(filenames, filenames + num_files, compare_file_size);
	
	cout << "Files sorted" << endl;

	for( int i = 0; i < num_files; i++ )
	{
		cout << filenames[i] << endl;
		ThreadPool_add_work(tp, (thread_func_t)map, (void*)filenames[i]);
	}

	while(1)
	{
		pthread_mutex_lock( &tp->work_q_lock );
		int q_size = tp->queue.work_q.size();
		pthread_mutex_unlock( &tp->work_q_lock );
		if( q_size == 0 )
		{
			break;
		}
		usleep(50000);
	}

	for( int i = 0; i < num_mappers; i++ )
	{
		pthread_cancel( tp->threads[i] );
	}
	ThreadPool_destroy(tp);

	//print_partitions();
	
	vector<pthread_t> threads;
	for( int i = 0; i < num_reducers; i++ )
	{
		pthread_t *t = new pthread_t();
		pthread_create(t, NULL, (void *(*)(void *) )MR_ProcessPartition, (void*)i);
		threads.push_back(*t);
	}
}

