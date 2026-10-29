// CS527 Lab 8 - Task 2: Array Sorting
//
// Part A: parallel bubble sort, done as odd-even transposition sort.
// Part B: parallel merge sort using OpenMP tasks.
//
// Compile: g++ -fopenmp -O2 -Wall task2_sort.cpp -o task2
// Run:     ./task2

#include <omp.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
using namespace std;

// ======================= Part A: bubble / odd-even sort =======================

// Classic bubble sort (stops early when a pass makes no swap).
void bubble_sort_serial(int *a, long n) {
    for (long pass = 0; pass < n - 1; pass++) {
        bool swapped = false;
        for (long i = 0; i < n - 1 - pass; i++) {
            if (a[i] > a[i + 1]) {
                int t = a[i];
                a[i] = a[i + 1];
                a[i + 1] = t;
                swapped = true;
            }
        }
        if (!swapped)
            break;
    }
}

// Odd-even transposition sort, serial.
// Even phase compares the pairs (0,1), (2,3), (4,5), ...
// Odd  phase compares the pairs (1,2), (3,4), (5,6), ...
// After n phases the array is sorted.
void odd_even_sort_serial(int *a, long n) {
    for (long phase = 0; phase < n; phase++) {
        for (long i = phase % 2; i < n - 1; i += 2) {
            if (a[i] > a[i + 1]) {
                int t = a[i];
                a[i] = a[i + 1];
                a[i + 1] = t;
            }
        }
    }
}

// Odd-even transposition sort, parallel.
// Inside one phase the pairs do not overlap (each element is in at most one
// pair), so the pairs can be compared in parallel without any lock.
// Between two phases we DO need a barrier: the odd phase uses elements that
// the even phase just changed. The barrier at the end of `omp for` does this.
// The parallel region is opened only once (outside the phase loop), so the
// threads are not created again for each of the n phases.
void odd_even_sort_parallel(int *a, long n) {
    #pragma omp parallel
    for (long phase = 0; phase < n; phase++) {
        #pragma omp for
        for (long i = phase % 2; i < n - 1; i += 2) {
            if (a[i] > a[i + 1]) {
                int t = a[i];
                a[i] = a[i + 1];
                a[i + 1] = t;
            }
        }
        // implicit barrier here: no thread starts the next phase early
    }
}

// ======================= Part B: merge sort =======================

const long INSERTION_CUTOFF = 32;  // small pieces are sorted with insertion sort
const long TASK_CUTOFF = 10000;    // below this size no new tasks are made

void insertion_sort(int *a, long lo, long hi) {
    for (long i = lo + 1; i < hi; i++) {
        int x = a[i];
        long j = i - 1;
        while (j >= lo && a[j] > x) {
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = x;
    }
}

// merges the sorted halves a[lo..mid) and a[mid..hi) using tmp
void merge(int *a, int *tmp, long lo, long mid, long hi) {
    long i = lo, j = mid, k = lo;
    while (i < mid && j < hi) {
        if (a[i] <= a[j])
            tmp[k++] = a[i++];
        else
            tmp[k++] = a[j++];
    }
    while (i < mid)
        tmp[k++] = a[i++];
    while (j < hi)
        tmp[k++] = a[j++];
    memcpy(a + lo, tmp + lo, (hi - lo) * sizeof(int));
}

// sorts a[lo..hi)
void merge_sort_serial(int *a, int *tmp, long lo, long hi) {
    if (hi - lo <= INSERTION_CUTOFF) {
        insertion_sort(a, lo, hi);
        return;
    }
    long mid = lo + (hi - lo) / 2;
    merge_sort_serial(a, tmp, lo, mid);
    merge_sort_serial(a, tmp, mid, hi);
    merge(a, tmp, lo, mid, hi);
}

// The two halves are independent (different parts of a and tmp), so each
// half is sorted as a separate task. Before merging we must wait until both
// halves are finished: that is what `taskwait` does.
void merge_sort_tasks(int *a, int *tmp, long lo, long hi) {
    if (hi - lo <= TASK_CUTOFF) {
        merge_sort_serial(a, tmp, lo, hi); // too small to be worth a task
        return;
    }
    long mid = lo + (hi - lo) / 2;

    #pragma omp task
    merge_sort_tasks(a, tmp, lo, mid);

    #pragma omp task
    merge_sort_tasks(a, tmp, mid, hi);

    #pragma omp taskwait
    merge(a, tmp, lo, mid, hi);
}

void merge_sort_parallel(int *a, int *tmp, long n) {
    #pragma omp parallel
    {
        #pragma omp single // one thread starts the recursion, all threads run tasks
        merge_sort_tasks(a, tmp, 0, n);
    }
}

// ======================= helpers =======================

void fill_random(int *a, long n, unsigned seed) {
    srand(seed);
    for (long i = 0; i < n; i++)
        a[i] = rand() * 32768 + rand(); // rand() only goes to 32767 on Windows
}

// true if a is exactly equal to the reference (sorted with std::sort)
bool same(int *a, int *ref, long n) {
    for (long i = 0; i < n; i++) {
        if (a[i] != ref[i])
            return false;
    }
    return true;
}

int main() {
    int threads[5] = {1, 2, 4, 8, 16}; // my laptop has 8 cores / 16 logical CPUs
    cout << "Task 2 - Array Sorting" << endl;
    cout << fixed;

    // ---------------- Part A ----------------
    cout << endl << "===== Part A: bubble sort vs odd-even transposition sort =====" << endl;
    long sizes_a[3] = {10000, 50000, 100000};

    for (int s = 0; s < 3; s++) {
        long n = sizes_a[s];
        int *orig = new int[n];
        int *ref = new int[n];
        int *a = new int[n];

        fill_random(orig, n, 1);
        memcpy(ref, orig, n * sizeof(int));
        sort(ref, ref + n); // reference answer

        // these sorts are slow for large n, so each one is run only once
        memcpy(a, orig, n * sizeof(int));
        double start = omp_get_wtime();
        bubble_sort_serial(a, n);
        double end = omp_get_wtime();
        double t_bubble = end - start;
        bool ok_bubble = same(a, ref, n);

        memcpy(a, orig, n * sizeof(int));
        start = omp_get_wtime();
        odd_even_sort_serial(a, n);
        end = omp_get_wtime();
        double t_oe = end - start;
        bool ok_oe = same(a, ref, n);

        cout << endl << "N = " << n << " (one run each)" << endl;
        cout << "Serial bubble sort:   " << setprecision(1) << t_bubble * 1000 << " ms"
             << (ok_bubble ? "  (correct)" : "  (WRONG)") << endl;
        cout << "Serial odd-even sort: " << t_oe * 1000 << " ms"
             << (ok_oe ? "  (correct)" : "  (WRONG)") << endl;
        cout << "Threads   odd-even parallel (ms)   speedup vs serial odd-even   speedup vs bubble   Correct?" << endl;

        for (int k = 0; k < 5; k++) {
            omp_set_num_threads(threads[k]);
            memcpy(a, orig, n * sizeof(int));
            start = omp_get_wtime();
            odd_even_sort_parallel(a, n);
            end = omp_get_wtime();
            double t = end - start;
            bool ok = same(a, ref, n);

            cout << setw(7) << threads[k]
                 << setw(25) << setprecision(1) << t * 1000
                 << setw(29) << setprecision(2) << t_oe / t
                 << setw(20) << t_bubble / t
                 << "   " << (ok ? "yes" : "NO") << endl;
        }

        delete[] orig;
        delete[] ref;
        delete[] a;
    }

    // ---------------- Part B ----------------
    cout << endl << "===== Part B: merge sort with OpenMP tasks =====" << endl;
    long sizes_b[2] = {1000000, 10000000};

    for (int s = 0; s < 2; s++) {
        long n = sizes_b[s];
        int *orig = new int[n];
        int *ref = new int[n];
        int *a = new int[n];
        int *tmp = new int[n];

        fill_random(orig, n, 2);
        memcpy(ref, orig, n * sizeof(int));
        double start = omp_get_wtime();
        sort(ref, ref + n);
        double end = omp_get_wtime();
        double t_std = end - start;

        int repeat = 50000000 / n; // 50, 5 (the copy back to `a` is not timed)

        double t_serial = 0;
        bool ok_serial = true;
        for (int r = 0; r < repeat; r++) {
            memcpy(a, orig, n * sizeof(int));
            start = omp_get_wtime();
            merge_sort_serial(a, tmp, 0, n);
            end = omp_get_wtime();
            t_serial += end - start;
            if (!same(a, ref, n))
                ok_serial = false;
        }
        t_serial /= repeat;

        cout << endl << "N = " << n << " (average of " << repeat << " sorts)" << endl;
        cout << "std::sort (one run, for reference): " << setprecision(2) << t_std * 1000 << " ms" << endl;
        cout << "Serial merge sort: " << t_serial * 1000 << " ms"
             << (ok_serial ? "  (correct)" : "  (WRONG)") << endl;
        cout << "Threads   task merge sort (ms)   speedup   efficiency   Correct?" << endl;

        for (int k = 0; k < 5; k++) {
            omp_set_num_threads(threads[k]);
            double t = 0;
            bool ok = true;
            for (int r = 0; r < repeat; r++) {
                memcpy(a, orig, n * sizeof(int));
                start = omp_get_wtime();
                merge_sort_parallel(a, tmp, n);
                end = omp_get_wtime();
                t += end - start;
                if (!same(a, ref, n))
                    ok = false;
            }
            t /= repeat;

            cout << setw(7) << threads[k]
                 << setw(23) << setprecision(2) << t * 1000
                 << setw(10) << t_serial / t
                 << setw(13) << t_serial / t / threads[k]
                 << "   " << (ok ? "yes" : "NO") << endl;
        }

        delete[] orig;
        delete[] ref;
        delete[] a;
        delete[] tmp;
    }

    return 0;
}
