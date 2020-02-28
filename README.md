# Author: John Tyler Beckingham


# How to Use

- This is a mapreduce library implemented using pthreads. It can be used to process multiple/many data files asynchronously and return a combined result from all of them.  

- It is called with MR_Run(...), with arguments int num_files, char* filenames, Mapper map, int num_mappers,
Reducer red, and int num_reducers, which correspond to the number of input files, the input filenames, the mapping function you wish to be called on each file for processing, the number of mapping threads you wish to have working on the data, the reducing function you wish to have called on the mapped data, and the number of reducer threads you wish to have, respectively. 

- The mapping function must have return type void and take a char * filename as parameter, representing the file that it will be processing. The reducing function must have return type void and take char * key and int partition_number as parameters, representing the key for which the reducer is reducing and the partition number (or ID number) of the reducer

- The mapping function is expected to call MR_Emit(char* key, char* value) for each key value pair result found by the mapping function. This will add the given value to a list of values mapped to by the given key. 

- The reducing function is expected to retrieve these values by calling MR_GetNext(char* key, int partition_number) to get each value one by one until NULL is returned, indicating the end of the values needing to be processed. 


# Design Choices

- For the purposes of this library, I created my own data structure to store the results of the mapping phase. 

- To represent the requirement that each key should be able to store multiple values, I mapped a string to a vector of strings, and since the keys will be distributed into 'num_reducers' partitions, I made a vector of these mappings, which I called "part" (short for partition). I also had a vector of indices that correspond to the vector of partitions that I used to keep track of which values were already processed in the 'MR_GetNext' function. I had a vector of mutexes corresponding to the partitions that were used to lock each partition when mappers were writing data to the structure. There was also a simple int representing the number of partitions in the structure, for simplicity in multiple areas. 

- My threadpool implementation was an important part of this library. It was implemented with a vector of threads, a queue to store the tasks, a mutex to ensure that only one thread is accessing the queue at a time, a pthread conditional to notify the threads when there is work available, and a boolean that marks whether we are actively trying to destroy the threadpool. 

- The mutex made it relatively simple to ensure safely of the work queue in the threadpool. It was simply locked everytime work was added or grabbed from the queue. To avoid spinlocks in the threads, I simply had them wait for a signal that more work has been added. 


# Runtime Analysis

- MR_Emit runs in O(l) time, where l is the length of the key (for example l would be 6 if key was friend). This is because the mutex lock and unlock runtime do not grow with the input, and neither does ::vector.push_back, and MR_Partition must iterate through each character of the key, thus our runtime only grows with the length of the key. 

- MR_GetNext runs in constant ( O( 1 ) ) time, because all relevant data is stored in my data structure, and each of the 4 queries that I make happen in constant time. Perhaps one could argue that it runs in O(l) like MR_Emit because we must convert the value from a string to a char* which must iterate through each char of the string. Thus we could say that MR_GetNext()'s runtime grows linearly with the size of the value that it is fetching. 


# Testing

- Testing was done at all three stages on my implementation. This was vital as trying to test and debug after everything had been written would have been an absolute nightmare, and detecting where the error is becomes much more difficult.

- After my implementation of the threadpool, a dummy function was written for printing out values and thousands of tasks were added with various sleep times between them to ensure the threadpool can handle lots of work and varying time.

- For the later two stages, I mainly used different combinations of sample files along with the sample program 'distwc.c'.

- After my implementation of the mapping, I wrote a function to print out my entire Partition data structure. This is simply to ensure that my mapper threads were correctly filling out my data structure. No point in working on reduce when there is nothing to reduce or the data is incorrect. 

- Finally I simply tested various combinations of sample test files, using linux system calls to verify my library's results.

