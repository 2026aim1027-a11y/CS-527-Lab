// CS527 Lab 9 - Task 1: Parallel Merge Sort with OpenMP Tasks
//
// Serial merge sort and a parallel merge sort where the two halves are
// sorted as separate OpenMP tasks. Small subarrays (below a cutoff) are
// sorted serially instead of making more tasks.
//
// Compile: g++ -fopenmp -O2 -Wall task1_mergesort.cpp -o task1
// Run:     ./task1

#include <omp.h>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
using namespace std;

// Subarrays with right - left < cutoff are sorted serially. 2048 as in the
// handout; main() changes it for the cutoff experiment.
int cutoff = 2048;

// ---------------- helpers ----------------
// All ranges are inclusive: a[left..right].

// merges the sorted halves a[left..mid] and a[mid+1..right]
void merge(int *a, int *tmp, int left, int mid, int right) {
    int i = left, j = mid + 1, k = left;
    while (i <= mid && j <= right) {
        if (a[i] <= a[j])
            tmp[k++] = a[i++];
        else
            tmp[k++] = a[j++];
    }
    while (i <= mid)
        tmp[k++] = a[i++];
    while (j <= right)
        tmp[k++] = a[j++];
    memcpy(a + left, tmp + left, (right - left + 1) * sizeof(int));
}

// ---------------- serial merge sort ----------------

void merge_sort_serial(int *a, int *tmp, int left, int right) {
    if (left >= right)
        return;
    int mid = left + (right - left) / 2;
    merge_sort_serial(a, tmp, left, mid);
    merge_sort_serial(a, tmp, mid + 1, right);
    merge(a, tmp, left, mid, right);
}

// ---------------- parallel merge sort with tasks ----------------
// The two halves are disjoint parts of a (and of tmp), so the two tasks
// never write the same memory. a and tmp are shared pointers; left, mid and
// right are copied into each task (firstprivate) because the parent's
// variables may be gone or changed by the time the task runs.
// taskwait: the merge reads both halves, so it must wait until both child
// tasks have finished.

void merge_sort_parallel(int *a, int *tmp, int left, int right) {
    if (left >= right)
        return;

    if (right - left < cutoff) {
        merge_sort_serial(a, tmp, left, right);
        return;
    }

    int mid = left + (right - left) / 2;

    #pragma omp task shared(a, tmp) firstprivate(left, mid)
    merge_sort_parallel(a, tmp, left, mid);

    #pragma omp task shared(a, tmp) firstprivate(mid, right)
    merge_sort_parallel(a, tmp, mid + 1, right);

    #pragma omp taskwait
    merge(a, tmp, left, mid, right);
}

void sort_parallel(int *a, int *tmp, int n) {
    #pragma omp parallel
    {
        #pragma omp single // one thread starts the recursion, the team runs the tasks
        merge_sort_parallel(a, tmp, 0, n - 1);
    }
}

// number of tasks merge_sort_parallel creates for a range of this size
long count_tasks(int left, int right) {
    if (left >= right || right - left < cutoff)
        return 0;
    int mid = left + (right - left) / 2;
    return 2 + count_tasks(left, mid) + count_tasks(mid + 1, right);
}

// ---------------- checking ----------------

bool is_sorted_asc(int *a, int n) {
    for (int i = 1; i < n; i++) {
        if (a[i - 1] > a[i])
            return false;
    }
    return true;
}

bool same(int *a, int *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

// ---------------- timing ----------------
// Every sort starts from a fresh copy of the same unsorted input.
// omp_get_wtime() only changes every 1 ms on my machine, and a small sort is
// much shorter than that, so single sorts cannot be timed. Instead the whole
// loop (copy + sort, `repeat` times) is timed, the time of the copies alone
// is subtracted, and the result is divided by `repeat`.

double time_copies(int *orig, int *a, int n, int repeat) {
    double start = omp_get_wtime();
    for (int r = 0; r < repeat; r++)
        memcpy(a, orig, n * sizeof(int));
    double end = omp_get_wtime();
    return end - start;
}

double time_serial(int *orig, int *a, int *tmp, int n, int repeat) {
    double t_copy = time_copies(orig, a, n, repeat);
    double start = omp_get_wtime();
    for (int r = 0; r < repeat; r++) {
        memcpy(a, orig, n * sizeof(int));
        merge_sort_serial(a, tmp, 0, n - 1);
    }
    double end = omp_get_wtime();
    return (end - start - t_copy) / repeat;
}

// After the timed loop the output of the last sort is checked to be sorted
// and equal to the serial output. In the main table every thread count is
// additionally checked on 5 separate untimed runs (check_every_run).
double time_parallel(int *orig, int *a, int *tmp, int n, int repeat, int *ref, bool &ok) {
    double t_copy = time_copies(orig, a, n, repeat);
    double start = omp_get_wtime();
    for (int r = 0; r < repeat; r++) {
        memcpy(a, orig, n * sizeof(int));
        sort_parallel(a, tmp, n);
    }
    double end = omp_get_wtime();
    if (!is_sorted_asc(a, n) || !same(a, ref, n))
        ok = false;
    return (end - start - t_copy) / repeat;
}

// Sorts the input `times` times with the current thread count and checks
// every single result (not timed).
bool check_every_run(int *orig, int *a, int *tmp, int n, int times, int *ref) {
    bool ok = true;
    for (int r = 0; r < times; r++) {
        memcpy(a, orig, n * sizeof(int));
        sort_parallel(a, tmp, n);
        if (!is_sorted_asc(a, n) || !same(a, ref, n))
            ok = false;
    }
    return ok;
}

void fill_random(int *a, int n, unsigned seed) {
    srand(seed);
    for (int i = 0; i < n; i++)
        a[i] = rand() * 32768 + rand(); // rand() only goes to 32767 on Windows
}

int main() {
    int threads[4] = {1, 2, 4, 8}; // thread counts given in the handout
    cout << "Task 1 - Parallel Merge Sort with OpenMP Tasks" << endl;
    cout << fixed;

    // ================= main table: N = 10^5, 10^6, 10^7, cutoff 2048 =================
    int sizes[3] = {100000, 1000000, 10000000};
    cutoff = 2048;

    for (int s = 0; s < 3; s++) {
        int n = sizes[s];
        int *orig = new int[n];
        int *ref = new int[n];
        int *a = new int[n];
        int *tmp = new int[n]; // allocated once, before sorting, shared by all tasks
        fill_random(orig, n, 1);

        // serial result = reference answer
        memcpy(ref, orig, n * sizeof(int));
        merge_sort_serial(ref, tmp, 0, n - 1);
        bool serial_sorted = is_sorted_asc(ref, n);

        // the sorts are repeated and the average time is used
        int repeat = 20000000 / n; // 200, 20, 2
        if (repeat < 3)
            repeat = 3;

        double t_serial = time_serial(orig, a, tmp, n, repeat);

        cout << endl << "N = " << n << ", cutoff = " << cutoff << " (" << count_tasks(0, n - 1)
             << " tasks per sort, average of " << repeat << " sorts)" << endl;
        cout << "Serial: " << setprecision(3) << t_serial * 1000 << " ms, output sorted: "
             << (serial_sorted ? "yes" : "NO") << endl;
        cout << "Threads   time (ms)   speedup   efficiency   sorted and == serial?" << endl;

        for (int k = 0; k < 4; k++) {
            omp_set_num_threads(threads[k]);
            bool ok = check_every_run(orig, a, tmp, n, 5, ref);
            double t = time_parallel(orig, a, tmp, n, repeat, ref, ok);
            cout << setw(7) << threads[k]
                 << setw(12) << setprecision(3) << t * 1000
                 << setw(10) << setprecision(2) << t_serial / t
                 << setw(13) << t_serial / t / threads[k]
                 << "   " << (ok ? "yes" : "NO") << endl;
        }

        delete[] orig;
        delete[] ref;
        delete[] a;
        delete[] tmp;
    }

    // ================= small arrays =================
    cout << endl << "===== Small arrays (cutoff = 2048), time in microseconds =====" << endl;
    int small[3] = {1000, 10000, 50000};
    for (int s = 0; s < 3; s++) {
        int n = small[s];
        int *orig = new int[n];
        int *ref = new int[n];
        int *a = new int[n];
        int *tmp = new int[n];
        fill_random(orig, n, 2);
        memcpy(ref, orig, n * sizeof(int));
        merge_sort_serial(ref, tmp, 0, n - 1);

        int repeat = 20000000 / n; // 20000, 2000, 400
        double t_serial = time_serial(orig, a, tmp, n, repeat);

        cout << endl << "N = " << n << " (" << count_tasks(0, n - 1) << " tasks per sort, average of "
             << repeat << " sorts)" << endl;
        cout << "Serial: " << setprecision(1) << t_serial * 1e6 << " us" << endl;
        cout << "Threads   time (us)   speedup   sorted and == serial?" << endl;
        for (int k = 0; k < 4; k++) {
            omp_set_num_threads(threads[k]);
            bool ok = true;
            double t = time_parallel(orig, a, tmp, n, repeat, ref, ok);
            cout << setw(7) << threads[k]
                 << setw(12) << setprecision(1) << t * 1e6
                 << setw(10) << setprecision(2) << t_serial / t
                 << "   " << (ok ? "yes" : "NO") << endl;
        }

        delete[] orig;
        delete[] ref;
        delete[] a;
        delete[] tmp;
    }

    // ================= effect of the cutoff =================
    cout << endl << "===== Effect of the cutoff (N = 10^6) =====" << endl;
    {
        int n = 1000000;
        int *orig = new int[n];
        int *ref = new int[n];
        int *a = new int[n];
        int *tmp = new int[n];
        fill_random(orig, n, 3);
        memcpy(ref, orig, n * sizeof(int));
        merge_sort_serial(ref, tmp, 0, n - 1);

        int repeat = 10;
        double t_serial = time_serial(orig, a, tmp, n, repeat);
        cout << "Serial: " << setprecision(2) << t_serial * 1000 << " ms (average of " << repeat << " sorts)" << endl;
        cout << "   cutoff        tasks   1 thread (ms)   8 threads (ms)   speedup (8 thr)   correct?" << endl;

        int cutoffs[8] = {1, 16, 256, 2048, 16384, 131072, 524288, 1000000};
        for (int c = 0; c < 8; c++) {
            cutoff = cutoffs[c];
            bool ok = true;
            int rep = (cutoff < 256) ? 3 : repeat; // tiny cutoffs are very slow
            omp_set_num_threads(1);
            double t1 = time_parallel(orig, a, tmp, n, rep, ref, ok);
            omp_set_num_threads(8);
            double t8 = time_parallel(orig, a, tmp, n, rep, ref, ok);
            cout << setw(9) << cutoff
                 << setw(13) << count_tasks(0, n - 1)
                 << setw(16) << setprecision(2) << t1 * 1000
                 << setw(17) << t8 * 1000
                 << setw(18) << t_serial / t8
                 << "   " << (ok ? "yes" : "NO") << endl;
        }

        delete[] orig;
        delete[] ref;
        delete[] a;
        delete[] tmp;
    }

    return 0;
}
