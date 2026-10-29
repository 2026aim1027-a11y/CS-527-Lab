// CS527 OpenMP Lab - Task 3: Maximum and Minimum of an Array
//
// Finds the max/min value, and also the index where it occurs.
//
// Compile: g++ -fopenmp -O2 -Wall task3_maxmin.cpp -o task3
// Run:     ./task3

#include <omp.h>
#include <cstdlib>
#include <iomanip>
#include <iostream>
using namespace std;

const int MAX_THREADS = 64;

// ---------------- serial versions (used to check the answers) ----------------

void find_max_idx_serial(int *a, long n, int &max_val, long &max_idx) {
    int best_val = a[0];
    long best_idx = 0;
    for (long i = 1; i < n; i++) {
        if (a[i] > best_val) {
            best_val = a[i];
            best_idx = i;
        }
    }
    max_val = best_val;
    max_idx = best_idx;
}

void find_min_idx_serial(int *a, long n, int &min_val, long &min_idx) {
    int best_val = a[0];
    long best_idx = 0;
    for (long i = 1; i < n; i++) {
        if (a[i] < best_val) {
            best_val = a[i];
            best_idx = i;
        }
    }
    min_val = best_val;
    min_idx = best_idx;
}

// ---------------- 1. value only, using the reduction clause ----------------

int max_reduction(int *a, long n) {
    int max_val = a[0];
    #pragma omp parallel for reduction(max:max_val)
    for (long i = 1; i < n; i++) {
        if (a[i] > max_val)
            max_val = a[i];
    }
    return max_val;
}

int min_reduction(int *a, long n) {
    int min_val = a[0];
    #pragma omp parallel for reduction(min:min_val)
    for (long i = 1; i < n; i++) {
        if (a[i] < min_val)
            min_val = a[i];
    }
    return min_val;
}

// ---------------- 2a. value + index, using a critical section ----------------

void find_max_idx_critical(int *a, long n, int &max_val, long &max_idx) {
    int best_val = a[0];
    long best_idx = 0;

    #pragma omp parallel for
    for (long i = 1; i < n; i++) {
        #pragma omp critical
        {
            if (a[i] > best_val) {
                best_val = a[i];
                best_idx = i;
            }
        }
    }
    max_val = best_val;
    max_idx = best_idx;
}

void find_min_idx_critical(int *a, long n, int &min_val, long &min_idx) {
    int best_val = a[0];
    long best_idx = 0;

    #pragma omp parallel for
    for (long i = 1; i < n; i++) {
        #pragma omp critical
        {
            if (a[i] < best_val) {
                best_val = a[i];
                best_idx = i;
            }
        }
    }
    min_val = best_val;
    min_idx = best_idx;
}

// ---------------- 2b. value + index, using a manual reduction ----------------
// Each thread finds the best (value, index) of its own part of the array in
// private variables, saves it in its own slot of an array, and then a small
// serial loop picks the overall best.

void find_max_idx_manual(int *a, long n, int &max_val, long &max_idx) {
    int local_val[MAX_THREADS];
    long local_idx[MAX_THREADS];
    int num_threads = omp_get_max_threads();

    for (int t = 0; t < num_threads; t++) {
        local_val[t] = a[0];
        local_idx[t] = 0;
    }

    #pragma omp parallel
    {
        int id = omp_get_thread_num();
        int my_val = a[0]; // private to this thread
        long my_idx = 0;

        #pragma omp for
        for (long i = 1; i < n; i++) {
            if (a[i] > my_val) {
                my_val = a[i];
                my_idx = i;
            }
        }

        local_val[id] = my_val; // written only once per thread
        local_idx[id] = my_idx;
    }

    // final serial pass over the per-thread results
    int best_val = local_val[0];
    long best_idx = local_idx[0];
    for (int t = 1; t < num_threads; t++) {
        if (local_val[t] > best_val) {
            best_val = local_val[t];
            best_idx = local_idx[t];
        }
    }
    max_val = best_val;
    max_idx = best_idx;
}

void find_min_idx_manual(int *a, long n, int &min_val, long &min_idx) {
    int local_val[MAX_THREADS];
    long local_idx[MAX_THREADS];
    int num_threads = omp_get_max_threads();

    for (int t = 0; t < num_threads; t++) {
        local_val[t] = a[0];
        local_idx[t] = 0;
    }

    #pragma omp parallel
    {
        int id = omp_get_thread_num();
        int my_val = a[0];
        long my_idx = 0;

        #pragma omp for
        for (long i = 1; i < n; i++) {
            if (a[i] < my_val) {
                my_val = a[i];
                my_idx = i;
            }
        }

        local_val[id] = my_val;
        local_idx[id] = my_idx;
    }

    int best_val = local_val[0];
    long best_idx = local_idx[0];
    for (int t = 1; t < num_threads; t++) {
        if (local_val[t] < best_val) {
            best_val = local_val[t];
            best_idx = local_idx[t];
        }
    }
    min_val = best_val;
    min_idx = best_idx;
}

int main() {
    long sizes[2] = {10000000, 100000000};
    int threads[5] = {1, 2, 4, 8, 16}; // my laptop has 8 cores / 16 logical CPUs

    cout << "Task 3 - Maximum and Minimum of an Array" << endl;
    cout << "Each time is for finding the max AND the min (one call each)." << endl;
    cout << fixed; // print decimals normally (not in scientific form)

    for (int s = 0; s < 2; s++) {
        long n = sizes[s];

        int *a = new int[n];

        // Random values, plus one clear maximum and one clear minimum placed
        // at positions we know, so the correct answer is known in advance.
        srand(1);
        for (long i = 0; i < n; i++)
            a[i] = rand() % 10000;
        a[n / 3] = 99999;      // the maximum
        a[2 * (n / 3)] = -5;   // the minimum

        // omp_get_wtime() only changes every 1 ms on my machine, so every
        // function is called `repeat` times and the average time is used.
        int repeat = 1000000000 / n; // 100, 10

        // ---- serial ----
        int max_s = 0, min_s = 0;
        long max_idx_s = 0, min_idx_s = 0;
        double start = omp_get_wtime();
        for (int r = 0; r < repeat; r++) {
            find_max_idx_serial(a, n, max_s, max_idx_s);
            find_min_idx_serial(a, n, min_s, min_idx_s);
        }
        double end = omp_get_wtime();
        double t_serial = (end - start) / repeat;

        cout << endl << "N = " << n << " (average of " << repeat << " calls)" << endl;
        cout << "Serial: max = " << max_s << " at index " << max_idx_s
             << ", min = " << min_s << " at index " << min_idx_s
             << ", time = " << setprecision(3) << t_serial * 1000 << " ms" << endl;
        cout << "Threads    reduction (ms)    manual (ms)    manual speedup    critical (ms)    Correct?" << endl;

        for (int k = 0; k < 5; k++) {
            omp_set_num_threads(threads[k]);
            bool correct = true;
            int max_v = 0, min_v = 0;
            long max_i = 0, min_i = 0;

            // ---- reduction clause (value only) ----
            start = omp_get_wtime();
            for (int r = 0; r < repeat; r++) {
                max_v = max_reduction(a, n);
                min_v = min_reduction(a, n);
                if (max_v != max_s || min_v != min_s)
                    correct = false;
            }
            end = omp_get_wtime();
            double t_reduction = (end - start) / repeat;

            // ---- manual reduction (value + index) ----
            start = omp_get_wtime();
            for (int r = 0; r < repeat; r++) {
                find_max_idx_manual(a, n, max_v, max_i);
                find_min_idx_manual(a, n, min_v, min_i);
                if (max_v != max_s || max_i != max_idx_s || min_v != min_s || min_i != min_idx_s)
                    correct = false;
            }
            end = omp_get_wtime();
            double t_manual = (end - start) / repeat;

            cout << setw(7) << threads[k]
                 << setw(18) << setprecision(3) << t_reduction * 1000
                 << setw(15) << t_manual * 1000
                 << setw(18) << setprecision(2) << t_serial / t_manual;

            // ---- critical section (value + index) ----
            // very slow, so it is run only once and only for N = 10^7
            if (s == 0) {
                start = omp_get_wtime();
                find_max_idx_critical(a, n, max_v, max_i);
                find_min_idx_critical(a, n, min_v, min_i);
                end = omp_get_wtime();
                if (max_v != max_s || max_i != max_idx_s || min_v != min_s || min_i != min_idx_s)
                    correct = false;
                cout << setw(17) << (end - start) * 1000;
            } else {
                cout << setw(17) << "-";
            }

            cout << "    " << (correct ? "yes" : "NO") << endl;
        }

        delete[] a;
    }

    return 0;
}
