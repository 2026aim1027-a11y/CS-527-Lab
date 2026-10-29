CXX = g++
CXXFLAGS = -fopenmp -O2 -Wall

all: task1 task2 task3 task4 task5

task1: src/task1_vecadd.cpp
	$(CXX) $(CXXFLAGS) src/task1_vecadd.cpp -o task1

task2: src/task2_sum.cpp
	$(CXX) $(CXXFLAGS) src/task2_sum.cpp -o task2

task3: src/task3_maxmin.cpp
	$(CXX) $(CXXFLAGS) src/task3_maxmin.cpp -o task3

task4: src/task4_matadd.cpp
	$(CXX) $(CXXFLAGS) src/task4_matadd.cpp -o task4

task5: src/task5_matmul.cpp
	$(CXX) $(CXXFLAGS) src/task5_matmul.cpp -o task5

clean:
	rm -f task1 task2 task3 task4 task5 *.exe

.PHONY: all clean task1 task2 task3 task4 task5
