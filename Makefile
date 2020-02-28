CC	= g++ -pthread -Wall -Werror -O2 -std=c++11
SOURCES	= $(wildcard *.cc) $(wildcard *.c)
OBJECTS	= $(SOURCES:%.cc=%.o)

wc: $(OBJECTS)
	$(CC) -o wordcount $(OBJECTS)
	@echo 'wordcount.exe produced'

compile:
	$(CC) -c $(SOURCES)
	@echo 'code compiled'

clean:
	@rm -f *.o wordcount
	@echo 'directory cleaned'

compress:
	zip "mapreduce.zip" "distwc.c" "mapreduce.cc" "readme.md" "Makefile" "mapreduce.h" "threadpool.cc" "threadpool.h"