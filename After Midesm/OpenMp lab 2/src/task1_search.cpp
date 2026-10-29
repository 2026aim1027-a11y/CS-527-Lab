// CS527 Lab 8 - Task 1: Array Search
//
// Searches for a key in an array and returns its index (-1 if not present).
// Three OpenMP versions:
//   1. reduction(min:...)  - scans the whole array, gives the FIRST index
//   2. early exit, any     - all threads stop as soon as anyone finds the key,
//                            gives SOME index where the key is
//   3. early exit, first   - stops early but still gives the FIRST index
//
// Compile: g++ -fopenmp -O2 -Wall task1_search.cpp -o task1
// Run:     ./task1

#include <omp.h>
#include <cstdlib>
#include <iomanip>
#include <iostream>
using namespace std;

// In the early-exit versions a thread looks at the shared flag once every
// BLOCK elements instead of after every element.
const long BLOCK = 4096;

// ---------------- serial version (used to check the answers) ----------------

long search_serial(int *a, long n, int key) {
    for (long i = 0; i < n; i++) {
        if (a[i] == key)
            return i;
    }
    return -1;
}

// ---------------- 1. reduction: first index, no early exit ----------------
// Every thread keeps the smallest matching index of its own part in a private
// copy of `first`, and the copies are combined with min at the end.
// The loop cannot `break` (not allowed inside an omp for), so the whole
// array is always scanned.

long search_reduction(int *a, long n, int key) {
    long first = n; // n means "not found"

    #pragma omp parallel for reduction(min:first)
    for (long i = 0; i < n; i++) {
        if (a[i] == key && i < first)
            first = i;
    }
    return (first == n) ? -1 : first;
}

// ---------------- 2. early exit: any index ----------------
// The array is split into one contiguous part per thread. Each thread scans
// its part and, every BLOCK elements, checks the shared `found_idx`. When any
// thread finds the key it writes the index there and all threads stop.
// The reads and writes of found_idx are atomic so there is no data race.

long search_any_early(int *a, long n, int key) {
    long found_idx = -1;

    #pragma omp parallel
    {
        int id = omp_get_thread_num();
        int p = omp_get_num_threads();
        long lo = (long long)n * id / p;
        long hi = (long long)n * (id + 1) / p;

        for (long start = lo; start < hi; start += BLOCK) {
            long cur;
            #pragma omp atomic read
            cur = found_idx;
            if (cur != -1)
                break; // someone already found it

            long end = (start + BLOCK < hi) ? start + BLOCK : hi;
            long my_idx = -1;
            for (long i = start; i < end; i++) {
                if (a[i] == key) {
                    my_idx = i;
                    break;
                }
            }
            if (my_idx != -1) {
                #pragma omp atomic write
                found_idx = my_idx;
                break;
            }
        }
    }
    return found_idx;
}

// ---------------- 3. early exit: first index ----------------
// Same split as above, but a thread may only stop when a thread with a LOWER
// part has already found the key (then nothing in my part can be first).
// Threads before the match still have to finish their part to make sure the
// key does not appear earlier.

long search_first_early(int *a, long n, int key) {
    long best = n; // smallest index found so far, n means "not found"

    #pragma omp parallel
    {
        int id = omp_get_thread_num();
        int p = omp_get_num_threads();
        long lo = (long long)n * id / p;
        long hi = (long long)n * (id + 1) / p;

        for (long start = lo; start < hi; start += BLOCK) {
            long cur;
            #pragma omp atomic read
            cur = best;
            if (cur < lo)
                break; // a lower part already has the key

            long end = (start + BLOCK < hi) ? start + BLOCK : hi;
            long my_idx = -1;
            for (long i = start; i < end; i++) {
                if (a[i] == key) {
                    my_idx = i;
                    break;
                }
            }
            if (my_idx != -1) {
                // happens at most once per thread, so a critical is cheap here
                #pragma omp critical
                {
                    if (my_idx < best) {
                        #pragma omp atomic write
                        best = my_idx;
                    }
                }
                break;
            }
        }
    }
    return (best == n) ? -1 : best;
}

// ---------------- correctness tests on many small random arrays ----------------

bool run_small_tests() {
    int threads[5] = {1, 2, 4, 8, 16};
    int tests = 0, failed = 0;

    srand(7);
    for (int t = 0; t < 2000; t++) {
        long n = 1 + rand() % 5000;
        int *a = new int[n];
        for (long i = 0; i < n; i++)
            a[i] = rand() % 50; // small range, so keys often appear several times
        int key = rand() % 60;  // 50..59 are never in the array

        long expected = search_serial(a, n, key);
        omp_set_num_threads(threads[t % 5]);

        long r1 = search_reduction(a, n, key);
        long r2 = search_any_early(a, n, key);
        long r3 = search_first_early(a, n, key);

        // the "any" version only has to return a valid position of the key
        bool ok2 = (expected == -1) ? (r2 == -1) : (r2 >= 0 && r2 < n && a[r2] == key);
        if (r1 != expected || !ok2 || r3 != expected)
            failed++;
        tests++;
        delete[] a;
    }

    cout << "Small random tests (n = 1..5000, keys repeat, some keys absent): "
         << tests - failed << " / " << tests << " passed" << endl;
    return failed == 0;
}

int main() {
    long sizes[2] = {10000000, 100000000};
    int threads[5] = {1, 2, 4, 8, 16}; // my laptop has 8 cores / 16 logical CPUs

    cout << "Task 1 - Array Search" << endl << endl;
    run_small_tests();
    cout << fixed; // print decimals normally (not in scientific form)

    for (int s = 0; s < 2; s++) {
        long n = sizes[s];
        int *a = new int[n];

        // Values 0..999999, so the keys -1 and -2 below never appear by chance.
        srand(1);
        for (long i = 0; i < n; i++)
            a[i] = rand() % 1000000;
        a[n / 3] = -1;     // the key appears twice; the first one is at n/3
        a[2 * (n / 3)] = -1;

        // omp_get_wtime() only changes every 1 ms on my machine, so every
        // function is called `repeat` times and the average time is used.
        int repeat = 1000000000 / n; // 100, 10

        // Case 0: key = -1 (present, first at n/3). Case 1: key = -2 (absent).
        int keys[2] = {-1, -2};
        const char *names[2] = {"present (first at N/3, again at 2N/3)", "absent"};

        for (int c = 0; c < 2; c++) {
            int key = keys[c];

            long idx_s = search_serial(a, n, key); // warm-up call, not timed
            double start = omp_get_wtime();
            for (int r = 0; r < repeat; r++)
                idx_s = search_serial(a, n, key);
            double end = omp_get_wtime();
            double t_serial = (end - start) / repeat;

            cout << endl << "N = " << n << ", key " << names[c]
                 << " (average of " << repeat << " calls)" << endl;
            cout << "Serial: index = " << idx_s << ", time = "
                 << setprecision(3) << t_serial * 1000 << " ms" << endl;
            cout << "Threads   reduction (ms)  speedup   any-early (ms)  speedup   first-early (ms)  speedup   Correct?" << endl;

            for (int k = 0; k < 5; k++) {
                omp_set_num_threads(threads[k]);
                bool correct = true;
                long idx = 0;

                start = omp_get_wtime();
                for (int r = 0; r < repeat; r++) {
                    idx = search_reduction(a, n, key);
                    if (idx != idx_s)
                        correct = false;
                }
                end = omp_get_wtime();
                double t_red = (end - start) / repeat;

                start = omp_get_wtime();
                for (int r = 0; r < repeat; r++) {
                    idx = search_any_early(a, n, key);
                    bool ok = (idx_s == -1) ? (idx == -1) : (idx >= 0 && a[idx] == key);
                    if (!ok)
                        correct = false;
                }
                end = omp_get_wtime();
                double t_any = (end - start) / repeat;

                start = omp_get_wtime();
                for (int r = 0; r < repeat; r++) {
                    idx = search_first_early(a, n, key);
                    if (idx != idx_s)
                        correct = false;
                }
                end = omp_get_wtime();
                double t_first = (end - start) / repeat;

                cout << setw(7) << threads[k]
                     << setw(17) << setprecision(3) << t_red * 1000
                     << setw(9) << setprecision(2) << t_serial / t_red
                     << setw(17) << setprecision(3) << t_any * 1000
                     << setw(9) << setprecision(2) << t_serial / t_any
                     << setw(19) << setprecision(3) << t_first * 1000
                     << setw(9) << setprecision(2) << t_serial / t_first
                     << "   " << (correct ? "yes" : "NO") << endl;
            }
        }

        delete[] a;
    }

    return 0;
}
