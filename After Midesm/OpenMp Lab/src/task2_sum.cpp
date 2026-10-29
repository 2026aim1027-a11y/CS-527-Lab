// CS527 OpenMP Lab - Task 2: Sum of Array Elements
//
// Note: the handout uses `long`, but on Windows `long` is only 32 bits and
// the sum does not fit in it, so `long long` (64 bits) is used here.
//
// Compile: g++ -fopenmp -O2 -Wall task2_sum.cpp -o task2
// Run:     ./task2

#include <omp.h>
#include <iomanip>
#include <iostream>
using namespace std;

long long sum_serial(long long *a, long n) {
    long long sum = 0;
    for (long i = 0; i < n; i++)
        sum += a[i];
    return sum;
}

// WRONG on purpose: all threads update the shared variable `sum`
// at the same time without any protection (data race).
long long sum_naive_parallel_BROKEN(long long *a, long n) {
    long long sum = 0;
    #pragma omp parallel for
    for (long i = 0; i < n; i++)
        sum += a[i];
    return sum;
}

// Correct: every thread gets its own private copy of sum,
// and OpenMP adds the copies together at the end.
long long sum_reduction(long long *a, long n) {
    long long sum = 0;
    #pragma omp parallel for reduction(+:sum)
    for (long i = 0; i < n; i++)
        sum += a[i];
    return sum;
}

// Correct but slow: only one thread at a time can do sum += a[i].
long long sum_critical(long long *a, long n) {
    long long sum = 0;
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        #pragma omp critical
        sum += a[i];
    }
    return sum;
}

int main() {
    long sizes[2] = {10000000, 100000000};
    int threads[5] = {1, 2, 4, 8, 16}; // my laptop has 8 cores / 16 logical CPUs

    cout << "Task 2 - Sum of Array Elements" << endl;
    cout << fixed; // print decimals normally (not in scientific form)

    for (int s = 0; s < 2; s++) {
        long n = sizes[s];

        long long *a = new long long[n];

        // a = 1, 2, 3, ..., N so the correct sum is N(N+1)/2
        for (long i = 0; i < n; i++)
            a[i] = i + 1;
        long long formula = (long long)n * (n + 1) / 2;

        // ---- Step 1: show that the naive version is broken ----
        if (s == 0) {
            omp_set_num_threads(8);
            cout << endl << "Naive parallel sum (no reduction), N = " << n << ", 8 threads" << endl;
            cout << "Correct answer = " << formula << endl;
            for (int run = 1; run <= 10; run++) {
                long long result = sum_naive_parallel_BROKEN(a, n);
                cout << "Run " << setw(2) << run << ": sum = " << setw(15) << result
                     << "   " << (result == formula ? "correct" : "WRONG") << endl;
            }
        }

        // omp_get_wtime() only changes every 1 ms on my machine, so every
        // function is called `repeat` times and the average time is used.
        int repeat = 1000000000 / n; // 100, 10

        // ---- serial ----
        long long correct = 0;
        double start = omp_get_wtime();
        for (int r = 0; r < repeat; r++)
            correct = sum_serial(a, n);
        double end = omp_get_wtime();
        double t_serial = (end - start) / repeat;

        cout << endl << "N = " << n << " (average of " << repeat << " calls), serial sum = "
             << correct << ", N(N+1)/2 = " << formula << endl;
        cout << "Threads    reduction (ms)    Speedup    Matches serial?    critical (ms)" << endl;
        cout << setw(7) << "serial"
             << setw(18) << setprecision(4) << t_serial * 1000
             << setw(11) << setprecision(2) << 1.0
             << setw(19) << "-" << setw(17) << "-" << endl;

        // ---- Steps 2-4: reduction (and critical) for each thread count ----
        for (int k = 0; k < 5; k++) {
            omp_set_num_threads(threads[k]);

            bool matches = true; // stays true only if every run gives the serial sum
            start = omp_get_wtime();
            for (int r = 0; r < repeat; r++) {
                long long result = sum_reduction(a, n);
                if (result != correct)
                    matches = false;
            }
            end = omp_get_wtime();
            double t_reduction = (end - start) / repeat;

            cout << setw(7) << threads[k]
                 << setw(18) << setprecision(4) << t_reduction * 1000
                 << setw(11) << setprecision(2) << t_serial / t_reduction
                 << setw(19) << (matches ? "yes (every run)" : "NO");

            // critical is very slow, so it is run only once and only for N = 10^7
            if (s == 0) {
                start = omp_get_wtime();
                long long result = sum_critical(a, n);
                end = omp_get_wtime();
                cout << setw(17) << setprecision(2) << (end - start) * 1000
                     << (result == correct ? " (correct)" : " (WRONG)") << endl;
            } else {
                cout << setw(17) << "-" << endl;
            }
        }

        delete[] a;
    }

    return 0;
}
