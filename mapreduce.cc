// mapreduce.cc
// Author: John Tyler Beckingham
//
// This is an implementation of mapreduce using pthreads for more
// information see the readme.md

#include "mapreduce.h"
#include "threadpool.h"
#include <pthread.h>
#include <vector>
#include <string>
#include <stdio.h>
#include <sys/stat.h>
#include <algorithm>
#include <iostream>
#include <unistd.h>
#include <map>

using namespace std;

// definition of global shared data structure
typedef struct Partitions{
    vector<map<string, vector<string> > > part;
    vector<pthread_mutex_t> part_lock;
    vector<int> indices;
    int num_partitions;
    Partitions(int partitions){
        this->num_partitions = partitions;
        for(int i = 0; i < partitions; i++){
        	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
            this->part_lock.push_back( mutex );
            pthread_mutex_init( &this->part_lock[i], NULL );
            this->part.push_back( map<string, vector<string> >() );
            this->indices.push_back( 0 );
        }
    }
} Partitions;

// Global variables for the shared data structure and the user defined reduce function
Partitions *partitions;
Reducer red_func;

/* Helper function for determining the larger file size given two files
 * used for sorting the file list
 */
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


/* Takes a key and the number of partitions and returns
 * the partition that a particular key must be in
 */
unsigned long MR_Partition(char *key, int num_partitions) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++) != '\0')
        hash = hash * 33 + c;
    return hash % num_partitions;
}

/* print_partitions is a helper function to print the entire shared data 
 * structure for testing mapping functionality
 */
void print_partitions()
{
	for( int i = 0; i < partitions->num_partitions; i++ )
	{
		cout << "Partition " << i << endl;
		::map< string, vector<string> >::iterator it;
		for(it = partitions->part[i].begin(); it != partitions->part[i].end(); it++)
		{
		    cout << "Key " << it->first << endl;
		    cout << it->second.size() << endl;
		    for( unsigned j = 0; j < it->second.size(); j++ )
		    {
		    	//cout << it->second[j] << ", ";
		    }
		    cout << endl;
		}
	}
}

/* MR_run is the main function of this mapreduce library. It 
 * creates 'num_mappers' threads in a threadpool for processing the files, 
 * and adds all the files to the threadpool work queue with the given 'map' function 
 * as the file processing function. The result of each of these work tasks is saved 
 * in a shared data structure. 'num_reducers' threads are created, and each are assigned 
 * a portion of the shared data structure to "reduce" to final answer values using the 'concate'
 * given function. 
 */
void MR_Run(int num_files, char *filenames[],
            Mapper map, int num_mappers,
            Reducer concate, int num_reducers) {

	// instantiate the shared data structure and define the reducing function
    partitions = new Partitions(num_reducers);
    red_func = concate;

    // create the mappers
    ThreadPool_t *mappers = ThreadPool_create(num_mappers);

    // sort the filenames by length of file
    sort(filenames, filenames + num_files, compare_file_size);

    // add each file as work
    for (int i = 0; i < num_files; i++) {
        ThreadPool_add_work( mappers, (thread_func_t) map, (void *) filenames[i] );
    }
    // destroy the mappers
    ThreadPool_destroy(mappers);

    // for each partition, create a reducer and task it with processing its given partition
    vector<pthread_t> reducers;
    for( intptr_t i = 0; i < num_reducers; i++ )
    {
    	pthread_t t;
    	pthread_create(&t, NULL, (void *(*)(void *) ) MR_ProcessPartition, (void*)i);
    	reducers.push_back(t);
    }

    // join the reducer threads
    for( unsigned i = 0; i < reducers.size(); i++ )
    {
    	pthread_join(reducers[i], NULL);
    }

    // Done!
}

/* MR_Emit finds the correct partition according to our hash function and safely writes the 
 * value to that partition
 */
void MR_Emit(char *key, char *value) {
	// Get the index to emit to
    unsigned long partition_index = MR_Partition(key, partitions->num_partitions);

    // lock the shared data structure
    pthread_mutex_lock(&(partitions->part_lock[partition_index]));

    // add the value to the data structure
    string s = key;
    partitions->part[partition_index][s].push_back(string(value));

    // unlock shared data structure
    pthread_mutex_unlock( &partitions->part_lock[partition_index] );
}

/* Process Partition is called on each partition by each
 * Reducer thread. This function invokes user defined
 * Reduce function on the next unprocessed key.
 */
void MR_ProcessPartition(int partition_number) {
	map<string, vector<string> >::iterator it;
	for( it = partitions->part[partition_number].begin(); it != partitions->part[partition_number].end(); it++ )
	{
		red_func( (char*) it->first.c_str(), partition_number );
	}
}

/* Get and return the next unprocessed value for the given key
 * returns NULL is all values have been processed for the given key
 */
char *MR_GetNext(char *key, int partition_number) {
	string s = key;
	// get the current index
	unsigned index = partitions->indices[partition_number];

	// if we have gone through all the elements for this key
	// reset the index and return null
	if( index == partitions->part[partition_number][s].size() ) 
	{
		partitions->indices[partition_number] = 0;
		return NULL;
	}
	// else increase index and return the value
	partitions->indices[partition_number]++;
	return (char*)partitions->part[partition_number][s][index].c_str();
}
