// CS527 Lab 9 - Task 3: Counting Even and Odd Elements
//
// Two counters updated in one parallel loop. Versions:
//   1. reduction(+:even, odd)          - two variables in one reduction clause
//   2. atomic on every element         - #pragma omp atomic on the shared counters
//   3. local counters + atomic once    - private counting, one atomic per thread
//
// Compile: g++ -fopenmp -O2 -Wall task3_evenodd.cpp -o task3
// Run:     ./task3

#include <omp.h>
#include <cstdlib>
#include <iomanip>
#include <iostream>
using namespace std;

// ---------------- serial version (used to check the answers) ----------------
// a[i] % 2 == 0 is used for "even". This also works for negative numbers:
// in C++ -3 % 2 is -1 (not 1), so a test like a[i] % 2 == 1 would miss
// negative odd numbers.

void count_serial(int *a, long n, long *even_count, long *odd_count) {
    long even = 0, odd = 0;
    for (long i = 0; i < n; i++) {
        if (a[i] % 2 == 0)
            even++;
        else
            odd++;
    }
    *even_count = even;
    *odd_count = odd;
}

// ---------------- 1. reduction with two variables ----------------
// Both variables are listed in one clause, separated by a comma. Each thread
// gets its own private even and odd (starting at 0), and at the end OpenMP
// adds all the private copies into the original variables.

void count_reduction(int *a, long n, long *even_count, long *odd_count) {
    long even = 0, odd = 0;

    #pragma omp parallel for reduction(+:even, odd)
    for (long i = 0; i < n; i++) {
        if (a[i] % 2 == 0)
            even++;
        else
            odd++;
    }
    *even_count = even;
    *odd_count = odd;
}

// ---------------- 2. atomic update on every element ----------------
// even and odd are shared; every increment is an atomic operation.

void count_atomic(int *a, long n, long *even_count, long *odd_count) {
    long even = 0, odd = 0;

    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        if (a[i] % 2 == 0) {
            #pragma omp atomic
            even++;
        } else {
            #pragma omp atomic
            odd++;
        }
    }
    *even_count = even;
    *odd_count = odd;
}

// ---------------- 3. local counters, atomic once per thread ----------------
// The same idea as the reduction, written by hand: count privately, then add
// the private counts into the shared ones with two atomics per thread.
// Shows that atomic itself is not the problem - doing it N times is.

void count_local_atomic(int *a, long n, long *even_count, long *odd_count) {
    long even = 0, odd = 0;

    #pragma omp parallel
    {
        long my_even = 0, my_odd = 0; // private: declared inside the region

        #pragma omp for
        for (long i = 0; i < n; i++) {
            if (a[i] % 2 == 0)
                my_even++;
            else
                my_odd++;
        }

        #pragma omp atomic
        even += my_even;
        #pragma omp atomic
        odd += my_odd;
    }
    *even_count = even;
    *odd_count = odd;
}

// ---------------- correctness test with a known answer ----------------
// a[i] = i - n/2 for i = 0..n-1 (negative and positive numbers). Every second
// number is even, so for an even n exactly n/2 are even and n/2 are odd.

bool known_answer_test() {
    long n = 1000000;
    int *a = new int[n];
    for (long i = 0; i < n; i++)
        a[i] = (int)(i - n / 2);

    long e, o;
    bool ok = true;
    omp_set_num_threads(8);

    count_serial(a, n, &e, &o);
    if (e != n / 2 || o != n / 2) ok = false;
    count_reduction(a, n, &e, &o);
    if (e != n / 2 || o != n / 2) ok = false;
    count_atomic(a, n, &e, &o);
    if (e != n / 2 || o != n / 2) ok = false;
    count_local_atomic(a, n, &e, &o);
    if (e != n / 2 || o != n / 2) ok = false;

    cout << "Known-answer test (a[i] = i - n/2, n = 10^6, values -500000..499999, 8 threads): "
         << "expected even = odd = " << n / 2 << " -> " << (ok ? "all versions correct" : "WRONG")
         << endl;
    delete[] a;
    return ok;
}

int main() {
    long sizes[4] = {100000, 1000000, 10000000, 100000000};
    int threads[4] = {1, 2, 4, 8}; // thread counts given in the handout

    cout << "Task 3 - Counting Even and Odd Elements" << endl << endl;
    known_answer_test();
    cout << fixed;

    for (int s = 0; s < 4; s++) {
        long n = sizes[s];
        int *a = new int[n];

        // random values from -1000000 to 1000000 (rand() only goes to 32767
        // on Windows, so two calls are combined)
        srand(1);
        for (long i = 0; i < n; i++)
            a[i] = (rand() * 32768 + rand()) % 2000001 - 1000000;

        // omp_get_wtime() only changes every 1 ms on my machine, so every
        // function is called `repeat` times and the average time is used.
        // atomic is much slower, so it is called fewer times.
        int repeat = 1000000000 / n;   // 10000, 1000, 100, 10
        int repeat_atomic = repeat / 100;
        if (repeat_atomic < 1)
            repeat_atomic = 1;         // 100, 10, 1, 1

        long even_s, odd_s;
        count_serial(a, n, &even_s, &odd_s); // warm-up call, not timed
        double start = omp_get_wtime();
        for (int r = 0; r < repeat; r++)
            count_serial(a, n, &even_s, &odd_s);
        double end = omp_get_wtime();
        double t_serial = (end - start) / repeat;

        cout << endl << "N = " << n << " (reduction / local: average of " << repeat
             << " calls, atomic: " << repeat_atomic << ")" << endl;
        cout << "Serial: even = " << even_s << ", odd = " << odd_s
             << ", even + odd = " << even_s + odd_s << (even_s + odd_s == n ? " (= N)" : " (WRONG)")
             << ", time = " << setprecision(4) << t_serial * 1000 << " ms" << endl;
        cout << "Threads   reduction (ms)  speedup  efficiency   local+atomic (ms)  speedup   atomic (ms)  vs serial   Correct?" << endl;

        for (int k = 0; k < 4; k++) {
            omp_set_num_threads(threads[k]);
            bool correct = true;
            long e = 0, o = 0;

            start = omp_get_wtime();
            for (int r = 0; r < repeat; r++) {
                count_reduction(a, n, &e, &o);
                if (e != even_s || o != odd_s || e + o != n)
                    correct = false;
            }
            end = omp_get_wtime();
            double t_red = (end - start) / repeat;

            start = omp_get_wtime();
            for (int r = 0; r < repeat; r++) {
                count_local_atomic(a, n, &e, &o);
                if (e != even_s || o != odd_s || e + o != n)
                    correct = false;
            }
            end = omp_get_wtime();
            double t_local = (end - start) / repeat;

            start = omp_get_wtime();
            for (int r = 0; r < repeat_atomic; r++) {
                count_atomic(a, n, &e, &o);
                if (e != even_s || o != odd_s || e + o != n)
                    correct = false;
            }
            end = omp_get_wtime();
            double t_atomic = (end - start) / repeat_atomic;

            cout << setw(7) << threads[k]
                 << setw(17) << setprecision(4) << t_red * 1000
                 << setw(9) << setprecision(2) << t_serial / t_red
                 << setw(12) << t_serial / t_red / threads[k]
                 << setw(20) << setprecision(4) << t_local * 1000
                 << setw(9) << setprecision(2) << t_serial / t_local
                 << setw(14) << setprecision(2) << t_atomic * 1000
                 << setw(10) << setprecision(1) << t_atomic / t_serial << "x"
                 << "   " << (correct ? "yes" : "NO") << endl;
        }

        delete[] a;
    }

    return 0;
}
