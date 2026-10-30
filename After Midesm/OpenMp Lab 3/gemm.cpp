//task 1
#include <omp.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdlib>

// Helper: Merge two sorted halves into a temporary array and copy back
void merge(int *a, int *tmp, int left, int mid, int right) {
    int i = left;
    int j = mid + 1;
    int k = left;

    while (i <= mid && j <= right) {
        if (a[i] <= a[j]) {
            tmp[k++] = a[i++];
        } else {
            tmp[k++] = a[j++];
        }
    }

    while (i <= mid) {
        tmp[k++] = a[i++];
    }

    while (j <= right) {
        tmp[k++] = a[j++];
    }

    for (i = left; i <= right; i++) {
        a[i] = tmp[i];
    }
}

// 1. Serial Merge Sort Baseline
void merge_sort_serial(int *a, int *tmp, int left, int right) {
    if (left >= right) return;
    int mid = left + (right - left) / 2;
    merge_sort_serial(a, tmp, left, mid);
    merge_sort_serial(a, tmp, mid + 1, right);
    merge(a, tmp, left, mid, right);
}

// 2. Parallel Merge Sort using OpenMP Tasks
void merge_sort_parallel(int *a, int *tmp, int left, int right) {
    if (left >= right) {
        return;
    }

    // 3. Cutoff threshold to prevent excessive task creation overhead
    if ((right - left) < 2048) {
        merge_sort_serial(a, tmp, left, right);
        return;
    }

    int mid = left + (right - left) / 2;

    // Spawn task for the left half
    #pragma omp task shared(a, tmp) firstprivate(left, mid)
    merge_sort_parallel(a, tmp, left, mid);

    // Spawn task for the right half
    #pragma omp task shared(a, tmp) firstprivate(mid, right)
    merge_sort_parallel(a, tmp, mid + 1, right);

    // Synchronize: Wait for both child tasks to finish before merging[cite: 3, 4]
    #pragma omp taskwait

    merge(a, tmp, left, mid, right);
}

int main(int argc, char* argv[]) {
    int n = 1000000; // Array size N = 10^6
    if (argc > 1) {
        n = std::atoi(argv[1]);
    }

    std::cout << "========================================\n";
    std::cout << "Running Parallel Merge Sort (N = " << n << ")\n";
    std::cout << "========================================\n";

    std::vector<int> a_serial(n);
    for (int i = 0; i < n; i++) {
        a_serial[i] = std::rand() % 1000000;
    }
    std::vector<int> a_parallel = a_serial;
    std::vector<int> tmp(n);
    std::vector<int> tmp_serial(n);

    // --- Serial Timing ---
    double t0 = omp_get_wtime();
    merge_sort_serial(a_serial.data(), tmp_serial.data(), 0, n - 1);
    double t1 = omp_get_wtime();
    double serial_time = t1 - t0;
    std::cout << "Serial Merge Sort Time : " << serial_time << " seconds\n";

    // --- Parallel Timing ---
    double t2 = omp_get_wtime();
    #pragma omp parallel
    {
        #pragma omp single
        {
            merge_sort_parallel(a_parallel.data(), tmp.data(), 0, n - 1);
        }
    }
    double t3 = omp_get_wtime();
    double parallel_time = t3 - t2;
    std::cout << "Parallel Task Sort Time: " << parallel_time << " seconds\n";

    if (parallel_time > 0.0) {
        std::cout << "Speedup                : " << (serial_time / parallel_time) << "x\n";
    }

    // --- Verification ---
    bool is_sorted = std::is_sorted(a_parallel.begin(), a_parallel.end());
    bool matches = (a_serial == a_parallel);

    if (is_sorted && matches) {
        std::cout << "Verification           : SUCCESS (Array sorted correctly)\n";
    } else {
        std::cout << "Verification           : FAILED!\n";
    }

    return 0;
}

//task 2
#include <omp.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>

// Function to run the uneven workload loop with a specific schedule
double run_parallel_schedule(const std::vector<double>& a_in, std::vector<double>& a_out, int n, omp_sched_t sched_type, int chunk_size) {
    a_out = a_in;

    // Set schedule dynamically if needed or rely on clause
    if (sched_type == omp_sched_static) {
        omp_set_schedule(omp_sched_static, chunk_size);
    } else if (sched_type == omp_sched_dynamic) {
        omp_set_schedule(omp_sched_dynamic, chunk_size);
    } else if (sched_type == omp_sched_guided) {
        omp_set_schedule(omp_sched_guided, chunk_size);
    }

    double start = omp_get_wtime();

    // Using runtime schedule reads from omp_set_schedule
    #pragma omp parallel for schedule(runtime)
    for (int i = 0; i < n; i++) {
        double x = a_out[i];
        // Simulate an uneven computational workload
        int work = (i % 1000) * 100;
        for (int j = 0; j < work; j++) {
            x = x * 1.000001 + 0.0000001;
        }
        a_out[i] = x;
    }

    double end = omp_get_wtime();
    return end - start;
}

int main(int argc, char* argv[]) {
    int n = 1000000; // Default N = 10^6
    if (argc > 1) {
        n = std::atoi(argv[1]);
    }

    std::cout << "========================================\n";
    std::cout << "Running Loop Scheduling Test (N = " << n << ")\n";
    std::cout << "========================================\n";

    std::vector<double> a(n);
    for (int i = 0; i < n; i++) {
        a[i] = i * 0.001; //
    }

    // --- 1. Serial Baseline ---
    std::vector<double> a_serial = a;
    double t_start = omp_get_wtime();
    for (int i = 0; i < n; i++) {
        double x = a_serial[i];
        int work = (i % 1000) * 100; //[cite: 5]
        for (int j = 0; j < work; j++) {
            x = x * 1.000001 + 0.0000001; //[cite: 5]
        }
        a_serial[i] = x;
    }
    double serial_time = omp_get_wtime() - t_start;
    std::cout << "Serial Execution Time : " << serial_time << " seconds\n";

    // --- 2. Parallel Static Schedule ---
    std::vector<double> a_static;
    double static_time = run_parallel_schedule(a, a_static, n, omp_sched_static, 0);
    std::cout << "Static Schedule Time  : " << static_time << " seconds (Speedup: " << (serial_time / static_time) << "x)\n";

    // --- 3. Parallel Dynamic Schedule (Chunk size = 100) ---
    std::vector<double> a_dynamic;
    double dynamic_time = run_parallel_schedule(a, a_dynamic, n, omp_sched_dynamic, 100);
    std::cout << "Dynamic Schedule Time : " << dynamic_time << " seconds (Speedup: " << (serial_time / dynamic_time) << "x)\n";

    // --- 4. Parallel Guided Schedule ---
    std::vector<double> a_guided;
    double guided_time = run_parallel_schedule(a, a_guided, n, omp_sched_guided, 0);
    std::cout << "Guided Schedule Time  : " << guided_time << " seconds (Speedup: " << (serial_time / guided_time) << "x)\n";

    return 0;
}

//task 3
#include <omp.h>
#include <iostream>
#include <vector>
#include <cstdlib>

// 1. Serial Solution
void count_even_odd_serial(const std::vector<int>& a, long n, long& even_count, long& odd_count) {
    long even = 0, odd = 0;
    for (long i = 0; i < n; i++) {
        if (a[i] % 2 == 0) {
            even++;
        } else {
            odd++;
        }
    }
    even_count = even;
    odd_count = odd;
}

// 2. Parallel Reduction Solution
void count_even_odd_reduction(const std::vector<int>& a, long n, long& even_count, long& odd_count) {
    long even = 0, odd = 0;

    // OpenMP supports multiple variables in a single reduction clause
    #pragma omp parallel for reduction(+:even, odd)
    for (long i = 0; i < n; i++) {
        if (a[i] % 2 == 0) {
            even++;
        } else {
            odd++;
        }
    }

    even_count = even;
    odd_count = odd;
}

// 3. Parallel Atomic Solution (for performance comparison)
void count_even_odd_atomic(const std::vector<int>& a, long n, long& even_count, long& odd_count) {
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

    even_count = even;
    odd_count = odd;
}

int main(int argc, char* argv[]) {
    long n = 100000000; // 100 million elements
    if (argc > 1) {
        n = std::atol(argv[1]);
    }

    std::cout << "========================================\n";
    std::cout << "Running Multiple Reductions Test (N = " << n << ")\n";
    std::cout << "========================================\n";

    std::vector<int> a(n);
    for (long i = 0; i < n; i++) {
        a[i] = static_cast<int>(i);
    }

    long serial_even = 0, serial_odd = 0;
    long red_even = 0, red_odd = 0;
    long atomic_even = 0, atomic_odd = 0;

    // --- Serial Baseline ---
    double t0 = omp_get_wtime();
    count_even_odd_serial(a, n, serial_even, serial_odd);
    double t1 = omp_get_wtime();
    double serial_time = t1 - t0;
    std::cout << "Serial Time     : " << serial_time << " seconds\n";

    // --- Reduction Version ---
    double t2 = omp_get_wtime();
    count_even_odd_reduction(a, n, red_even, red_odd);
    double t3 = omp_get_wtime();
    double reduction_time = t3 - t2;
    std::cout << "Reduction Time  : " << reduction_time << " seconds (Speedup: " << (serial_time / reduction_time) << "x)\n";

    // --- Atomic Version ---
    double t4 = omp_get_wtime();
    count_even_odd_atomic(a, n, atomic_even, atomic_odd);
    double t5 = omp_get_wtime();
    double atomic_time = t5 - t4;
    std::cout << "Atomic Time     : " << atomic_time << " seconds (Speedup: " << (serial_time / atomic_time) << "x)\n";

    // --- Verification ---
    bool matches = (serial_even == red_even && serial_odd == red_odd &&
                    serial_even == atomic_even && serial_odd == atomic_odd &&
                    (serial_even + serial_odd == n));

    if (matches) {
        std::cout << "\nVerification    : SUCCESS (All counts match identically, Even + Odd = N)\n";
    } else {
        std::cout << "\nVerification    : FAILED!\n";
    }

    return 0;
}
