//task 1#include <omp.h>
#include <iostream>
#include <vector>
#include <cstdlib>

// Serial implementation of vector addition
void vec_add_serial(const std::vector<double>& a, const std::vector<double>& b, std::vector<double>& c, long n) {
    for (long i = 0; i < n; i++) {
        c[i] = a[i] + b[i];
    }
}

// Parallel implementation using OpenMP parallel for
void vec_add_parallel(const std::vector<double>& a, const std::vector<double>& b, std::vector<double>& c, long n) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        c[i] = a[i] + b[i];
    }
}

int main(int argc, char *argv[]) {
    // Default size N = 10,000,000 (can override via command line argument)
    long n = 10000000; 
    if (argc > 1) {
        n = std::atol(argv[1]);
    }

    std::cout << "========================================\n";
    std::cout << "Running Vector Addition with N = " << n << "\n";
    std::cout << "========================================\n";

    // Allocate memory using std::vector
    std::vector<double> a(n);
    std::vector<double> b(n);
    std::vector<double> c_serial(n);
    std::vector<double> c_parallel(n);

    // Initialize vectors a[] and b[]
    for (long i = 0; i < n; i++) {
        a[i] = static_cast<double>(i) * 1.5;
        b[i] = static_cast<double>(i) * 2.5;
    }

    // 1. Measure Serial Execution Time
    double t0 = omp_get_wtime();
    vec_add_serial(a, b, c_serial, n);
    double t1 = omp_get_wtime();
    double serial_time = t1 - t0;

    // 2. Measure Parallel Execution Time
    double t2 = omp_get_wtime();
    vec_add_parallel(a, b, c_parallel, n);
    double t3 = omp_get_wtime();
    double parallel_time = t3 - t2;

    // 3. Print Timings & Speedup
    std::cout << "Serial Time   : " << serial_time << " seconds\n";
    std::cout << "Parallel Time : " << parallel_time << " seconds\n";
    if (parallel_time > 0.0) {
        std::cout << "Speedup       : " << (serial_time / parallel_time) << "\n";
    }

    // 4. Verify results (exact match required)[cite: 2]
    bool match = true;
    for (long i = 0; i < n; i++) {
        if (c_serial[i] != c_parallel[i]) {
            match = false;
            std::cout << "Mismatch at index " << i << ": serial=" << c_serial[i] 
                      << ", parallel=" << c_parallel[i] << "\n";
            break;
        }
    }

    if (match) {
        std::cout << "Verification  : SUCCESS (All elements match identically)\n";
    } else {
        std::cout << "Verification  : FAILED!\n";
    }

    return 0;
}

//task 2
#include <omp.h>
#include <iostream>
#include <vector>
#include <cstdlib>

// 1. Serial implementation of sum
long sum_serial(const std::vector<long>& a, long n) {
    long sum = 0;
    for (long i = 0; i < n; i++) {
        sum += a[i];
    }
    return sum;
}

// 2. Broken parallel implementation (Intentional Data Race)
long sum_naive_parallel_BROKEN(const std::vector<long>& a, long n) {
    long sum = 0;
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        sum += a[i]; // Data race: multiple threads read/write 'sum' simultaneously without synchronization
    }
    return sum;
}

// 3. Correct parallel implementation using reduction
long sum_reduction(const std::vector<long>& a, long n) {
    long sum = 0;
    #pragma omp parallel for reduction(+:sum)
    for (long i = 0; i < n; i++) {
        sum += a[i];
    }
    return sum;
}

int main(int argc, char *argv[]) {
    // Default size N = 10,000,000 (can be overridden via command line)
    long n = 10000000; 
    if (argc > 1) {
        n = std::atol(argv[1]);
    }

    std::cout << "========================================\n";
    std::cout << "Running Array Sum with N = " << n << "\n";
    std::cout << "========================================\n";

    // Allocate and initialize vector with values (e.g., all 1s or simple sequence)
    std::vector<long> a(n);
    for (long i = 0; i < n; i++) {
        a[i] = 1; // Using 1 makes it easy to verify: total sum should equal N
    }

    // Compute expected serial sum
    double t0 = omp_get_wtime();
    long expected_sum = sum_serial(a, n);
    double t1 = omp_get_wtime();
    std::cout << "Serial Sum     : " << expected_sum << " (Time: " << (t1 - t0) << "s)\n";

    // Demonstrate the broken naive parallel sum
    double t2 = omp_get_wtime();
    long broken_sum = sum_naive_parallel_BROKEN(a, n);
    double t3 = omp_get_wtime();
    std::cout << "Broken Sum     : " << broken_sum << " (Time: " << (t3 - t2) << "s) -> Notice how this is incorrect/inconsistent!\n";

    // Measure correct reduction-based parallel sum
    double t4 = omp_get_wtime();
    long reduction_sum = sum_reduction(a, n);
    double t5 = omp_get_wtime();
    std::cout << "Reduction Sum  : " << reduction_sum << " (Time: " << (t5 - t4) << "s)\n";

    // Verify correctness
    if (reduction_sum == expected_sum) {
        std::cout << "Verification   : SUCCESS (Reduction matches serial sum)\n";
    } else {
        std::cout << "Verification   : FAILED!\n";
    }

    return 0;
}

//task 3
#include <omp.h>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <algorithm>

// 1. Serial implementation to find max value and its index
void find_max_serial(const std::vector<int>& a, long n, int& max_val, long& max_idx) {
    max_val = a[0];
    max_idx = 0;
    for (long i = 1; i < n; i++) {
        if (a[i] > max_val) {
            max_val = a[i];
            max_idx = i;
        }
    }
}

// 2. Parallel max value using reduction clause
int find_max_val_reduction(const std::vector<int>& a, long n) {
    int max_val = a[0];
    #pragma omp parallel for reduction(max:max_val)
    for (long i = 0; i < n; i++) {
        if (a[i] > max_val) {
            max_val = a[i];
        }
    }
    return max_val;
}

// 3. Parallel max value and index using a critical section
void find_max_idx_critical(const std::vector<int>& a, long n, int& max_val, long& max_idx) {
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

int main(int argc, char *argv[]) {
    // Default size N = 10,000,000 (can be overridden via command line)
    long n = 10000000; 
    if (argc > 1) {
        n = std::atol(argv[1]);
    }

    std::cout << "========================================\n";
    std::cout << "Running Max/Min Array Search with N = " << n << "\n";
    std::cout << "========================================\n";

    // Allocate and initialize vector with pseudo-random or patterned values
    std::vector<int> a(n);
    for (long i = 0; i < n; i++) {
        a[i] = static_cast<int>(i % 999999); 
    }
    // Plant a unique maximum at a known location to verify correctness
    a[n / 2] = 99999999; 

    // --- Serial Baseline ---
    int serial_max_val;
    long serial_max_idx;
    double t0 = omp_get_wtime();
    find_max_serial(a, n, serial_max_val, serial_max_idx);
    double t1 = omp_get_wtime();
    std::cout << "Serial Max Val : " << serial_max_val << " at Index " << serial_max_idx 
              << " (Time: " << (t1 - t0) << "s)\n";

    // --- Reduction Max Value ---
    double t2 = omp_get_wtime();
    int red_max_val = find_max_val_reduction(a, n);
    double t3 = omp_get_wtime();
    std::cout << "Reduction Val  : " << red_max_val << " (Time: " << (t3 - t2) << "s)\n";

    // --- Critical Section Max Val & Index ---
    int crit_max_val;
    long crit_max_idx;
    double t4 = omp_get_wtime();
    find_max_idx_critical(a, n, crit_max_val, crit_max_idx);
    double t5 = omp_get_wtime();
    std::cout << "Critical Max   : " << crit_max_val << " at Index " << crit_max_idx 
              << " (Time: " << (t5 - t4) << "s)\n";

    // --- Verification ---
    if (serial_max_val == red_max_val && serial_max_val == crit_max_val && serial_max_idx == crit_max_idx) {
        std::cout << "Verification   : SUCCESS (All results match serial baseline)\n";
    } else {
        std::cout << "Verification   : FAILED!\n";
    }

    return 0;
}

//task 4
#include <omp.h>
#include <iostream>
#include <vector>
#include <cstdlib>

// 1. Serial Matrix Addition
void mat_add_serial(const std::vector<std::vector<double>>& A, 
                    const std::vector<std::vector<double>>& B, 
                    std::vector<std::vector<double>>& C, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            C[i][j] = A[i][j] + B[i][j];
        }
    }
}

// 2. Parallel Matrix Addition (Outer loop only)
void mat_add_outer_parallel(const std::vector<std::vector<double>>& A, 
                            const std::vector<std::vector<double>>& B, 
                            std::vector<std::vector<double>>& C, int n) {
    #pragma omp parallel for
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            C[i][j] = A[i][j] + B[i][j];
        }
    }
}

// 3. Parallel Matrix Addition using collapse(2)
void mat_add_collapse_parallel(const std::vector<std::vector<double>>& A, 
                               const std::vector<std::vector<double>>& B, 
                               std::vector<std::vector<double>>& C, int n) {
    #pragma omp parallel for collapse(2)
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            C[i][j] = A[i][j] + B[i][j];
        }
    }
}

int main(int argc, char *argv[]) {
    // Default matrix size N (can override via command line)
    int n = 1024; 
    if (argc > 1) {
        n = std::atoi(argv[1]);
    }

    std::cout << "========================================\n";
    std::cout << "Running Matrix Addition with N = " << n << "x" << n << "\n";
    std::cout << "========================================\n";

    // Allocate 2D vectors
    std::vector<std::vector<double>> A(n, std::vector<double>(n));
    std::vector<std::vector<double>> B(n, std::vector<double>(n));
    std::vector<std::vector<double>> C_serial(n, std::vector<double>(n));
    std::vector<std::vector<double>> C_outer(n, std::vector<double>(n));
    std::vector<std::vector<double>> C_collapse(n, std::vector<double>(n));

    // Initialize matrices A and B
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            A[i][j] = static_cast<double>(i + j) * 1.1;
            B[i][j] = static_cast<double>(i - j) * 2.2;
        }
    }

    // --- Serial Timing ---
    double t0 = omp_get_wtime();
    mat_add_serial(A, B, C_serial, n);
    double t1 = omp_get_wtime();
    std::cout << "Serial Time     : " << (t1 - t0) << " seconds\n";

    // --- Outer Loop Parallel Timing ---
    double t2 = omp_get_wtime();
    mat_add_outer_parallel(A, B, C_outer, n);
    double t3 = omp_get_wtime();
    std::cout << "Outer-Only Time : " << (t3 - t2) << " seconds\n";

    // --- Collapse(2) Parallel Timing ---
    double t4 = omp_get_wtime();
    mat_add_collapse_parallel(A, B, C_collapse, n);
    double t5 = omp_get_wtime();
    std::cout << "Collapse(2) Time: " << (t5 - t4) << " seconds\n";

    // --- Verification ---
    bool match = true;
    for (int i = 0; i < n && match; i++) {
        for (int j = 0; j < n; j++) {
            if (C_serial[i][j] != C_outer[i][j] || C_serial[i][j] != C_collapse[i][j]) {
                match = false;
                break;
            }
        }
    }

    if (match) {
        std::cout << "Verification    : SUCCESS (All matrices match identically)\n";
    } else {
        std::cout << "Verification    : FAILED!\n";
    }

    return 0;
}

//task 5
#include <omp.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>

// 1. Serial Matrix Multiplication
void matmul_serial(const std::vector<std::vector<double>>& A, 
                  const std::vector<std::vector<double>>& B, 
                  std::vector<std::vector<double>>& C, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            for (int k = 0; k < n; k++) {
                sum += A[i][k] * B[k][j];
            }
            C[i][j] = sum;
        }
    }
}

// 2. Parallel Matrix Multiplication (Outer 'i' loop only)[cite: 5]
void matmul_outer_parallel(const std::vector<std::vector<double>>& A, 
                           const std::vector<std::vector<double>>& B, 
                           std::vector<std::vector<double>>& C, int n) {
    #pragma omp parallel for
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            for (int k = 0; k < n; k++) {
                sum += A[i][k] * B[k][j];
            }
            C[i][j] = sum;
        }
    }
}

// 3. Parallel Matrix Multiplication using collapse(2) on (i, j) loops
void matmul_collapse_parallel(const std::vector<std::vector<double>>& A, 
                             const std::vector<std::vector<double>>& B, 
                             std::vector<std::vector<double>>& C, int n) {
    #pragma omp parallel for collapse(2)
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            for (int k = 0; k < n; k++) {
                sum += A[i][k] * B[k][j];
            }
            C[i][j] = sum;
        }
    }
}

int main(int argc, char *argv[]) {
    // Default size N = 512 (can override via command line)
    int n = 512; 
    if (argc > 1) {
        n = std::atoi(argv[1]);
    }

    int num_threads = omp_get_max_threads();
    std::cout << "========================================\n";
    std::cout << "Running MatMul with N = " << n << "x" << n << " (" << num_threads << " threads)\n";
    std::cout << "========================================\n";

    // Allocate 2D vectors
    std::vector<std::vector<double>> A(n, std::vector<double>(n));
    std::vector<std::vector<double>> B(n, std::vector<double>(n));
    std::vector<std::vector<double>> C_serial(n, std::vector<double>(n, 0.0));
    std::vector<std::vector<double>> C_outer(n, std::vector<double>(n, 0.0));
    std::vector<std::vector<double>> C_collapse(n, std::vector<double>(n, 0.0));

    // Initialize inputs A and B
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            A[i][j] = static_cast<double>(i + 1) * 0.01;
            B[i][j] = static_cast<double>(j + 1) * 0.02;
        }
    }

    // --- Serial Version ---
    double t0 = omp_get_wtime();
    matmul_serial(A, B, C_serial, n);
    double t1 = omp_get_wtime();
    double t_serial = t1 - t0;
    std::cout << "Serial Time     : " << t_serial << " s\n";

    // --- Outer Loop Parallel Version ---
    double t2 = omp_get_wtime();
    matmul_outer_parallel(A, B, C_outer, n);
    double t3 = omp_get_wtime();
    double t_outer = t3 - t2;
    std::cout << "Outer-Only Time : " << t_outer << " s"
              << " | Speedup: " << (t_serial / t_outer)
              << " | Efficiency: " << (t_serial / t_outer) / num_threads << "\n";

    // --- Collapse(2) Parallel Version ---
    double t4 = omp_get_wtime();
    matmul_collapse_parallel(A, B, C_collapse, n);
    double t5 = omp_get_wtime();
    double t_collapse = t5 - t4;
    std::cout << "Collapse(2) Time: " << t_collapse << " s"
              << " | Speedup: " << (t_serial / t_collapse)
              << " | Efficiency: " << (t_serial / t_collapse) / num_threads << "\n";

    // --- Verification (Tolerance-based comparison) ---
    double tol = 1e-6;
    bool match = true;
    for (int i = 0; i < n && match; i++) {
        for (int j = 0; j < n; j++) {
            if (std::abs(C_serial[i][j] - C_outer[i][j]) > tol || 
                std::abs(C_serial[i][j] - C_collapse[i][j]) > tol) {
                match = false;
                break;
            }
        }
    }

    if (match) {
        std::cout << "Verification    : SUCCESS (Results match serial within tolerance)\n";
    } else {
        std::cout << "Verification    : FAILED!\n";
    }

    return 0;
}