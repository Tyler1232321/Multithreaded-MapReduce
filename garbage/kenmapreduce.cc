//
// Created by ken on 2019-10-26.
//
#include "mapreduce.h"
#include "threadpool.h"
#include <vector>
#include <string>
#include <sys/stat.h>
#include <algorithm>
#include <iostream>
#include <unistd.h>
#include <map>

using namespace std;

typedef struct Partitions_t{
    vector<map<string, vector<string>>> partitionMaps;
    vector<pthread_mutex_t> partitionLock;
    int n_partitions;
    Partitions_t(int partitions){
        this->n_partitions = partitions;
        for(int i = 0; i<partitions; i++){
            pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
            this->partitionLock.push_back(mutex);
            map<string, vector<string>> map;
            this->partitionMaps.push_back(map);
        }
    }
} Partitions_t;

Partitions_t *partitions;
Reducer func_Reducer;

bool job_comparator(string file1, string file2) {
    struct stat stat1, stat2;
    stat(file1.c_str(), &stat1);
    stat(file2.c_str(), &stat2);
    return (stat1.st_size < stat2.st_size);
}

/* Main Program in MapReduce. Passed an array of file names,
 * number of reducers and mappers, and function pointers to the
 * map and reduce functions.
 * Needs to create all the Mapping threads
 * When the mapping is complete, it needs to create all the
 * Reducing threads.
 *
 */

void MR_Run(int num_files, char *filenames[],
            Mapper map, int num_mappers,
            Reducer concate, int num_reducers) {
    partitions = new Partitions_t(num_reducers);
    func_Reducer = concate;
    // Create the reducer threads
    ThreadPool_t *mappers = ThreadPool_create(num_mappers);
    vector<string> jobs;
    for (int i = 0; i < num_files; i++) {
        string file = filenames[i];
        jobs.push_back(file);
    }
    //sort jobs by length of file
    sort(jobs.begin(), jobs.end(), job_comparator);
    for (int i = 0; i < num_files; i++) {
        ThreadPool_add_work(mappers, (thread_func_t) map, (void *) jobs[i].c_str());
    }

    ThreadPool_destroy(mappers);
    /*
    for (auto& map:partitions->partitionMaps){
        for(pair<string, vector<string>> elem : map){
            cout<<"key"<<" :: "<<elem.first<<endl;
            for(auto &s: elem.second){
                cout<<s<<",";
            }
            cout << endl;
        }

    }*/

    ThreadPool_t *reducers = ThreadPool_create(num_reducers);
    for (int i = 0; i < partitions->n_partitions; i++){
        ThreadPool_add_work(reducers, (thread_func_t) MR_ProcessPartition, (void *) i);
    }
    ThreadPool_destroy(reducers);

}

/* Called by a mapper thread. Determines which partition to write k/v
 * pair from the MR_Partition function. MR_Emit then inserts the pair
 * in the correct partition, while keeping the partition sorted.
 *
 */
void MR_Emit(char *key, char *value) {
    unsigned long partition_index = MR_Partition(key, partitions->n_partitions);
    pthread_mutex_lock(&(partitions->partitionLock[partition_index]));
    partitions->partitionMaps[partition_index][string(key)].push_back(string(value));
    pthread_mutex_unlock(&(partitions->partitionLock[partition_index]));
}

/* Takes a key and the number of partitions and returns
 * the partition that a particular key must be in
 *
 */
unsigned long MR_Partition(char *key, int num_partitions) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++) != '\0')
        hash = hash * 33 + c;
    return hash % num_partitions;
}

/* Process Partition is called on each partition by each
 * Reducer thread. This function invokes user defined
 * Reduce function on the next unprocessed key.
 *
 */
void MR_ProcessPartition(int partition_number) {
    pthread_mutex_lock(&(partitions->partitionLock[partition_number]));

    for(std::pair<string, vector<string>> elem: partitions->partitionMaps[partition_number]){
        func_Reducer( (char *) elem.first.c_str(), partition_number);
    }
    pthread_mutex_unlock(&(partitions->partitionLock[partition_number]));
}

/* Returns the next value associated with the given key in the
 * sorted partition or Null when it has been completely processed
 */
char *MR_GetNext(char *key, int partition_number) {

    string skey = key;
    string next;
    if(!partitions->partitionMaps[partition_number][skey].empty()){
        next = partitions->partitionMaps[partition_number][skey].back();
        partitions->partitionMaps[partition_number][skey].pop_back();
    }
    if(next.empty()){
        return NULL;
    }
    return (char *) next.c_str();
}
