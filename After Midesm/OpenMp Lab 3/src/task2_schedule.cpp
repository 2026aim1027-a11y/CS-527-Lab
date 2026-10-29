// CS527 Lab 9 - Task 2: Loop Scheduling (static, dynamic, guided)
//
// Every element gets an expensive operation whose cost depends on its index.
// Three workloads:
//   uniform    - every element costs the same
//   periodic   - cost = (i % 1000), the pattern from the handout
//   increasing - cost grows with i, so the last elements are the most expensive
// All three have the same average cost (about 100 inner iterations per element).
//
// Compile: g++ -fopenmp -O2 -Wall task2_schedule.cpp -o task2
// Run:     ./task2

#include <omp.h>
#include <iomanip>
#include <iostream>
using namespace std;

const int UNIFORM = 0, PERIODIC = 1, INCREASING = 2;
const char *WORKLOAD_NAME[3] = {"uniform", "periodic (i % 1000)", "increasing (grows with i)"};

// How many inner iterations element i needs.
// The handout uses (i % 1000) * 100, i.e. 50000 iterations on average; with
// N = 10^7 that would take minutes per run, so the costs here are scaled
// down to an average of about 100.
inline int work_amount(int kind, long i, long n) {
    if (kind == UNIFORM)
        return 100;
    if (kind == PERIODIC)
        return (int)(i % 1000) / 5;          // 0 .. 199
    return (int)(200.0 * i / n);             // 0 .. 199, growing with i
}

// the expensive operation from the handout
inline double compute(double x, int work) {
    for (int j = 0; j < work; j++)
        x = x * 1.0000001 + 0.0000001;
    return x;
}

// ---------------- serial ----------------

void run_serial(double *a, double *b, long n, int kind) {
    for (long i = 0; i < n; i++)
        b[i] = compute(a[i], work_amount(kind, i, n));
}

// ---------------- schedule(static) ----------------
// The iterations are cut into one equal block per thread before the loop
// starts: thread 0 gets the first N/p, thread 1 the next N/p, ...

void run_static(double *a, double *b, long n, int kind) {
    #pragma omp parallel for schedule(static)
    for (long i = 0; i < n; i++)
        b[i] = compute(a[i], work_amount(kind, i, n));
}

// ---------------- schedule(dynamic, 100) ----------------
// Chunks of 100 iterations are handed out while the loop runs: a thread that
// finishes its chunk asks for the next one.

void run_dynamic(double *a, double *b, long n, int kind) {
    #pragma omp parallel for schedule(dynamic, 100)
    for (long i = 0; i < n; i++)
        b[i] = compute(a[i], work_amount(kind, i, n));
}

// ---------------- schedule(guided) ----------------
// Like dynamic, but the chunks start big (remaining / threads) and get
// smaller as the loop goes on.

void run_guided(double *a, double *b, long n, int kind) {
    #pragma omp parallel for schedule(guided)
    for (long i = 0; i < n; i++)
        b[i] = compute(a[i], work_amount(kind, i, n));
}

// ---------------- schedule(runtime) for the chunk size experiment ----------------
// The schedule and chunk size are chosen with omp_set_schedule() before the
// call, so one function can try many chunk sizes.

void run_runtime(double *a, double *b, long n, int kind) {
    #pragma omp parallel for schedule(runtime)
    for (long i = 0; i < n; i++)
        b[i] = compute(a[i], work_amount(kind, i, n));
}

// The parallel versions do exactly the same operations on each element as
// the serial one, so the results must be bit-for-bit equal (== is used).
bool same(double *x, double *y, long n) {
    for (long i = 0; i < n; i++) {
        if (x[i] != y[i])
            return false;
    }
    return true;
}

typedef void (*RunFn)(double *, double *, long, int);

double time_run(RunFn fn, double *a, double *b, long n, int kind, int repeat) {
    double start = omp_get_wtime();
    for (int r = 0; r < repeat; r++)
        fn(a, b, n, kind);
    double end = omp_get_wtime();
    return (end - start) / repeat;
}

int main() {
    long sizes[3] = {100000, 1000000, 10000000};
    int threads[4] = {1, 2, 4, 8}; // thread counts given in the handout

    cout << "Task 2 - Loop Scheduling" << endl;
    cout << fixed;

    for (int kind = 0; kind < 3; kind++) {
        cout << endl << "==================== Workload: " << WORKLOAD_NAME[kind]
             << " ====================" << endl;

        for (int s = 0; s < 3; s++) {
            long n = sizes[s];
            double *a = new double[n];
            double *b_serial = new double[n];
            double *b = new double[n];
            for (long i = 0; i < n; i++)
                a[i] = i * 0.001;

            // omp_get_wtime() only changes every 1 ms on my machine, so the
            // smaller sizes are repeated and the average time is used.
            int repeat = (int)(5000000 / n); // 50, 5, 0 -> 1
            if (repeat < 1)
                repeat = 1;

            run_serial(a, b_serial, n, kind); // warm-up call, also the reference result
            double t_serial = time_run(run_serial, a, b_serial, n, kind, repeat);

            cout << endl << "N = " << n << " (average of " << repeat << " runs)" << endl;
            cout << "Serial: " << setprecision(2) << t_serial * 1000 << " ms" << endl;
            cout << "Threads    static (ms)  S     E      dynamic,100 (ms)  S     E      guided (ms)  S     E     Correct?" << endl;

            for (int k = 0; k < 4; k++) {
                omp_set_num_threads(threads[k]);
                bool ok = true;

                double t_st = time_run(run_static, a, b, n, kind, repeat);
                if (!same(b, b_serial, n)) ok = false;
                double t_dy = time_run(run_dynamic, a, b, n, kind, repeat);
                if (!same(b, b_serial, n)) ok = false;
                double t_gu = time_run(run_guided, a, b, n, kind, repeat);
                if (!same(b, b_serial, n)) ok = false;

                int p = threads[k];
                cout << setw(7) << p
                     << setw(15) << setprecision(2) << t_st * 1000
                     << setw(6) << t_serial / t_st << setw(7) << t_serial / t_st / p
                     << setw(20) << t_dy * 1000
                     << setw(6) << t_serial / t_dy << setw(7) << t_serial / t_dy / p
                     << setw(15) << t_gu * 1000
                     << setw(6) << t_serial / t_gu << setw(7) << t_serial / t_gu / p
                     << "   " << (ok ? "yes" : "NO") << endl;
            }

            delete[] a;
            delete[] b_serial;
            delete[] b;
        }
    }

    // ================= chunk size experiment =================
    // N = 10^6, 8 threads, uniform and increasing workloads.
    cout << endl << "==================== Chunk size (N = 10^6, 8 threads) ====================" << endl;
    {
        long n = 1000000;
        double *a = new double[n];
        double *b_ref = new double[n];
        double *b = new double[n];
        for (long i = 0; i < n; i++)
            a[i] = i * 0.001;
        int repeat = 5;
        int chunks[6] = {1, 10, 100, 1000, 10000, 100000};

        omp_set_num_threads(8);
        for (int kind = 0; kind < 3; kind += 2) { // uniform and increasing
            run_serial(a, b_ref, n, kind);
            double t_serial = time_run(run_serial, a, b_ref, n, kind, repeat);
            cout << endl << "Workload: " << WORKLOAD_NAME[kind] << ", serial "
                 << setprecision(2) << t_serial * 1000 << " ms" << endl;
            cout << "    chunk    static,chunk (ms)  speedup   dynamic,chunk (ms)  speedup   guided,chunk (ms)  speedup   Correct?" << endl;

            for (int c = 0; c < 6; c++) {
                bool ok = true;
                omp_set_schedule(omp_sched_static, chunks[c]);
                double t_st = time_run(run_runtime, a, b, n, kind, repeat);
                if (!same(b, b_ref, n)) ok = false;
                omp_set_schedule(omp_sched_dynamic, chunks[c]);
                double t_dy = time_run(run_runtime, a, b, n, kind, repeat);
                if (!same(b, b_ref, n)) ok = false;
                omp_set_schedule(omp_sched_guided, chunks[c]);
                double t_gu = time_run(run_runtime, a, b, n, kind, repeat);
                if (!same(b, b_ref, n)) ok = false;

                cout << setw(9) << chunks[c]
                     << setw(21) << t_st * 1000 << setw(9) << t_serial / t_st
                     << setw(21) << t_dy * 1000 << setw(9) << t_serial / t_dy
                     << setw(20) << t_gu * 1000 << setw(9) << t_serial / t_gu
                     << "   " << (ok ? "yes" : "NO") << endl;
            }
        }

        delete[] a;
        delete[] b_ref;
        delete[] b;
    }

    return 0;
}
