// CS527 Lab 8 - extra: cost of one OpenMP barrier on this machine
//
// Used to explain the Task 2 (odd-even sort) results: that sort needs one
// barrier per phase, i.e. n barriers in total.
//
// Compile: g++ -fopenmp -O2 -Wall barrier_cost.cpp -o barrier_cost
// Run:     ./barrier_cost

#include <omp.h>
#include <iomanip>
#include <iostream>
using namespace std;

int main() {
    int threads[5] = {1, 2, 4, 8, 16};
    const int R = 20000; // barriers per measurement

    cout << "Cost of one barrier (average of " << R << ")" << endl;
    cout << "Threads   barrier (us)   empty omp for (us)" << endl;
    cout << fixed;

    for (int k = 0; k < 5; k++) {
        omp_set_num_threads(threads[k]);

        #pragma omp parallel // warm-up: create the threads first
        { }

        double start = omp_get_wtime();
        #pragma omp parallel
        for (int r = 0; r < R; r++) {
            #pragma omp barrier
        }
        double end = omp_get_wtime();
        double t_barrier = (end - start) / R;

        // an omp for with almost no work, as in one phase of the sort
        start = omp_get_wtime();
        #pragma omp parallel
        for (int r = 0; r < R; r++) {
            #pragma omp for
            for (int i = 0; i < threads[k]; i++) {
            }
        }
        end = omp_get_wtime();
        double t_for = (end - start) / R;

        cout << setw(7) << threads[k]
             << setw(15) << setprecision(2) << t_barrier * 1e6
             << setw(21) << t_for * 1e6 << endl;
    }

    return 0;
}
