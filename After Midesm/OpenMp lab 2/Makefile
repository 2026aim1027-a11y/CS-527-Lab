CXX = g++
CXXFLAGS = -fopenmp -O2 -Wall

all: task1 task2 task3 task4 task5

task1: src/task1_search.cpp
	$(CXX) $(CXXFLAGS) src/task1_search.cpp -o task1

task2: src/task2_sort.cpp
	$(CXX) $(CXXFLAGS) src/task2_sort.cpp -o task2

task3: src/task3_histogram.cpp
	$(CXX) $(CXXFLAGS) src/task3_histogram.cpp -o task3

task4: src/task4_transpose.cpp
	$(CXX) $(CXXFLAGS) src/task4_transpose.cpp -o task4

task5: src/task5_conv2d.cpp
	$(CXX) $(CXXFLAGS) src/task5_conv2d.cpp -o task5

# extra measurement used in the Task 2 explanation (not built by `make`)
barrier_cost: src/barrier_cost.cpp
	$(CXX) $(CXXFLAGS) src/barrier_cost.cpp -o barrier_cost

clean:
	rm -f task1 task2 task3 task4 task5 barrier_cost *.exe

.PHONY: all clean task1 task2 task3 task4 task5 barrier_cost
