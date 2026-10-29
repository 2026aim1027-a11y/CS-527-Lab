CXX = g++
CXXFLAGS = -fopenmp -O2 -Wall

all: task1 task2 task3

task1: src/task1_mergesort.cpp
	$(CXX) $(CXXFLAGS) src/task1_mergesort.cpp -o task1

task2: src/task2_schedule.cpp
	$(CXX) $(CXXFLAGS) src/task2_schedule.cpp -o task2

task3: src/task3_evenodd.cpp
	$(CXX) $(CXXFLAGS) src/task3_evenodd.cpp -o task3

clean:
	rm -f task1 task2 task3 *.exe

.PHONY: all clean task1 task2 task3
