// CS527 OpenMP Lab - Task 1: Vector Addition
// c[i] = a[i] + b[i]
//
// Compile: g++ -fopenmp -O2 -Wall task1_vecadd.cpp -o task1
// Run:     ./task1

#include <omp.h>
#include <iomanip>
#include <iostream>
using namespace std;

void vec_add_serial(double *a, double *b, double *c, long n) {
    for (long i = 0; i < n; i++)
        c[i] = a[i] + b[i];
}

void vec_add_parallel(double *a, double *b, double *c, long n) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++)
        c[i] = a[i] + b[i];
}

int main() {
    long sizes[4] = {100000, 1000000, 10000000, 100000000};
    int threads[5] = {1, 2, 4, 8, 16}; // my laptop has 8 cores / 16 logical CPUs

    cout << "Task 1 - Vector Addition" << endl;
    cout << fixed; // print decimals normally (not in scientific form)

    for (int s = 0; s < 4; s++) {
        long n = sizes[s];

        double *a = new double[n];
        double *b = new double[n];
        double *c_serial = new double[n];
        double *c_parallel = new double[n];

        // Whole numbers, so a[i] + b[i] is exact and the results can be
        // compared with == (no tolerance needed).
        for (long i = 0; i < n; i++) {
            a[i] = i;
            b[i] = 2 * i;
            c_serial[i] = 0;
            c_parallel[i] = 0;
        }

        // omp_get_wtime() only changes every 1 ms on my machine, and adding
        // a small array is much faster than that. So every function is
        // called `repeat` times and the average time of one call is used.
        int repeat = 1000000000 / n; // 10000, 1000, 100, 10

        // ---- serial ----
        double start = omp_get_wtime();
        for (int r = 0; r < repeat; r++)
            vec_add_serial(a, b, c_serial, n);
        double end = omp_get_wtime();
        double t_serial = (end - start) / repeat;

        cout << endl << "N = " << n << " (average of " << repeat << " calls)" << endl;
        cout << "Threads    Time (ms)    Speedup    Same as serial?" << endl;
        cout << setw(7) << "serial"
             << setw(13) << setprecision(4) << t_serial * 1000
             << setw(11) << setprecision(2) << 1.0
             << "    -" << endl;

        // ---- parallel, for each thread count ----
        for (int k = 0; k < 5; k++) {
            omp_set_num_threads(threads[k]);

            start = omp_get_wtime();
            for (int r = 0; r < repeat; r++)
                vec_add_parallel(a, b, c_parallel, n);
            end = omp_get_wtime();
            double t_parallel = (end - start) / repeat;

            // verify: compare every element with the serial result
            bool same = true;
            for (long i = 0; i < n; i++) {
                if (c_serial[i] != c_parallel[i])
                    same = false;
            }

            cout << setw(7) << threads[k]
                 << setw(13) << setprecision(4) << t_parallel * 1000
                 << setw(11) << setprecision(2) << t_serial / t_parallel
                 << "    " << (same ? "yes" : "NO") << endl;
        }

        delete[] a;
        delete[] b;
        delete[] c_serial;
        delete[] c_parallel;
    }

    return 0;
}
