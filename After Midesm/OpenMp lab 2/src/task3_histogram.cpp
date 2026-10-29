// CS527 Lab 8 - Task 3: Histogram Calculation
//
// Counts how many times each value 0..9 appears in an array.
// Versions: naive (broken), critical, atomic, per-thread local histograms,
// and the array reduction reduction(+:hist[:10]).
//
// Compile: g++ -fopenmp -O2 -Wall task3_histogram.cpp -o task3
// Run:     ./task3

#include <omp.h>
#include <cstdlib>
#include <iomanip>
#include <iostream>
using namespace std;

const int BINS = 10;

// ---------------- serial version (used to check the answers) ----------------

void hist_serial(int *a, long n, long *hist) {
    for (int b = 0; b < BINS; b++)
        hist[b] = 0;
    for (long i = 0; i < n; i++)
        hist[a[i]]++;
}

// ---------------- 0. naive parallel: WRONG, has a data race ----------------
// hist[a[i]]++ is read - add - write. Two threads that see the same value
// at the same time both read the old count and one increment is lost.

void hist_naive_BROKEN(int *a, long n, long *hist) {
    for (int b = 0; b < BINS; b++)
        hist[b] = 0;
    #pragma omp parallel for
    for (long i = 0; i < n; i++)
        hist[a[i]]++;
}

// ---------------- 1. critical section around every increment ----------------

void hist_critical(int *a, long n, long *hist) {
    for (int b = 0; b < BINS; b++)
        hist[b] = 0;
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        #pragma omp critical
        hist[a[i]]++;
    }
}

// ---------------- 2. atomic increment ----------------

void hist_atomic(int *a, long n, long *hist) {
    for (int b = 0; b < BINS; b++)
        hist[b] = 0;
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        #pragma omp atomic
        hist[a[i]]++;
    }
}

// ---------------- 3. private local histogram per thread ----------------
// Each thread counts into its own local array (no sharing, no lock), then
// adds its 10 counts into the shared histogram once, inside a critical.

void hist_local(int *a, long n, long *hist) {
    for (int b = 0; b < BINS; b++)
        hist[b] = 0;

    #pragma omp parallel
    {
        long my_hist[BINS] = {0}; // private: declared inside the parallel region

        #pragma omp for
        for (long i = 0; i < n; i++)
            my_hist[a[i]]++;

        #pragma omp critical // once per thread, not once per element
        {
            for (int b = 0; b < BINS; b++)
                hist[b] += my_hist[b];
        }
    }
}

// ---------------- 4. array reduction (OpenMP 4.5) ----------------
// Same idea as version 3, but OpenMP makes the private copies and adds them
// together for us.

void hist_reduction(int *a, long n, long *hist) {
    for (int b = 0; b < BINS; b++)
        hist[b] = 0;
    #pragma omp parallel for reduction(+:hist[:BINS])
    for (long i = 0; i < n; i++)
        hist[a[i]]++;
}

bool same_hist(long *h1, long *h2) {
    for (int b = 0; b < BINS; b++) {
        if (h1[b] != h2[b])
            return false;
    }
    return true;
}

int main() {
    long sizes[2] = {10000000, 100000000};
    int threads[5] = {1, 2, 4, 8, 16}; // my laptop has 8 cores / 16 logical CPUs

    cout << "Task 3 - Histogram (values 0..9)" << endl;
    cout << fixed;

    for (int s = 0; s < 2; s++) {
        long n = sizes[s];
        int *a = new int[n];
        srand(1);
        for (long i = 0; i < n; i++)
            a[i] = rand() % BINS;

        long h_serial[BINS], h[BINS];

        // omp_get_wtime() only changes every 1 ms on my machine, so the fast
        // functions are called `repeat` times and the average time is used.
        int repeat = 1000000000 / n; // 100, 10

        hist_serial(a, n, h_serial); // warm-up call, not timed
        double start = omp_get_wtime();
        for (int r = 0; r < repeat; r++)
            hist_serial(a, n, h_serial);
        double end = omp_get_wtime();
        double t_serial = (end - start) / repeat;

        long total = 0;
        for (int b = 0; b < BINS; b++)
            total += h_serial[b];

        cout << endl << "N = " << n << " (average of " << repeat << " calls)" << endl;
        cout << "Serial histogram:";
        for (int b = 0; b < BINS; b++)
            cout << " " << h_serial[b];
        cout << endl << "Sum of counts = " << total << (total == n ? " (= N, ok)" : " (WRONG)") << endl;
        cout << "Serial time = " << setprecision(3) << t_serial * 1000 << " ms" << endl;

        // ---- show that the naive version is wrong (8 threads, 5 runs) ----
        if (s == 0) {
            omp_set_num_threads(8);
            cout << endl << "Naive parallel version, 8 threads, 5 runs:" << endl;
            for (int r = 1; r <= 5; r++) {
                hist_naive_BROKEN(a, n, h);
                long sum = 0;
                for (int b = 0; b < BINS; b++)
                    sum += h[b];
                cout << "Run " << r << ": bin 0 = " << setw(9) << h[0]
                     << " (correct " << h_serial[0] << "), sum of counts = " << setw(9) << sum
                     << (same_hist(h, h_serial) ? "   correct" : "   WRONG") << endl;
            }
        }

        cout << endl << "Threads   reduction (ms)  speedup   local (ms)  speedup   atomic (ms)   critical (ms)   Correct?" << endl;

        for (int k = 0; k < 5; k++) {
            omp_set_num_threads(threads[k]);
            bool correct = true;

            start = omp_get_wtime();
            for (int r = 0; r < repeat; r++) {
                hist_reduction(a, n, h);
                if (!same_hist(h, h_serial))
                    correct = false;
            }
            end = omp_get_wtime();
            double t_red = (end - start) / repeat;

            start = omp_get_wtime();
            for (int r = 0; r < repeat; r++) {
                hist_local(a, n, h);
                if (!same_hist(h, h_serial))
                    correct = false;
            }
            end = omp_get_wtime();
            double t_local = (end - start) / repeat;

            // atomic is slow, so it is run only once
            start = omp_get_wtime();
            hist_atomic(a, n, h);
            end = omp_get_wtime();
            double t_atomic = end - start;
            if (!same_hist(h, h_serial))
                correct = false;

            cout << setw(7) << threads[k]
                 << setw(17) << setprecision(3) << t_red * 1000
                 << setw(9) << setprecision(2) << t_serial / t_red
                 << setw(13) << setprecision(3) << t_local * 1000
                 << setw(9) << setprecision(2) << t_serial / t_local
                 << setw(14) << setprecision(1) << t_atomic * 1000;

            // critical is even slower: run once, and only for N = 10^7
            if (s == 0) {
                start = omp_get_wtime();
                hist_critical(a, n, h);
                end = omp_get_wtime();
                if (!same_hist(h, h_serial))
                    correct = false;
                cout << setw(16) << (end - start) * 1000;
            } else {
                cout << setw(16) << "-";
            }

            cout << "   " << (correct ? "yes" : "NO") << endl;
        }

        delete[] a;
    }

    return 0;
}
