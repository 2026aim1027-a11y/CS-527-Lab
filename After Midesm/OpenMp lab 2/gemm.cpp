// task 1
#include <omp.h>
#include <iostream>
#include <vector>
#include <cstdlib>

// 1. Serial Implementation
long linear_search_serial(const std::vector<int> &a, long n, int target)
{
    for (long i = 0; i < n; i++)
    {
        if (a[i] == target)
        {
            return i; // Return first matching index
        }
    }
    return -1; // Not found
}

// 2. OpenMP Parallel Implementation
long linear_search_parallel(const std::vector<int> &a, long n, int target)
{
    long found_idx = -1;
    bool found = false;

#pragma omp parallel for shared(found, found_idx)
    for (long i = 0; i < n; i++)
    {
        // Early exit check: skip computation if another thread already found the target
        if (found)
        {
            continue;
        }

        if (a[i] == target)
        {
#pragma omp critical
            {
                if (!found || i < found_idx)
                {
                    found_idx = i;
                    found = true;
                }
            }
        }
    }

    return found_idx;
}

int main(int argc, char *argv[])
{
    long n = 10000000; // Array size N = 10,000,000
    if (argc > 1)
    {
        n = std::atol(argv[1]);
    }

    int target = 999999;

    std::cout << "========================================\n";
    std::cout << "Running Parallel Array Search (N = " << n << ")\n";
    std::cout << "Target element: " << target << "\n";
    std::cout << "========================================\n";

    // Allocate and initialize array
    std::vector<int> a(n);
    for (long i = 0; i < n; i++)
    {
        a[i] = static_cast<int>(i % 500000);
    }

    // Plant target element near the end of the array to demonstrate performance
    long target_actual_index = n - 5000;
    a[target_actual_index] = target;

    // --- Serial Search ---
    double t0 = omp_get_wtime();
    long serial_idx = linear_search_serial(a, n, target);
    double t1 = omp_get_wtime();
    double serial_time = t1 - t0;

    std::cout << "Serial Search   : Index = " << serial_idx
              << " | Time: " << serial_time << " seconds\n";

    // --- Parallel Search ---
    double t2 = omp_get_wtime();
    long parallel_idx = linear_search_parallel(a, n, target);
    double t3 = omp_get_wtime();
    double parallel_time = t3 - t2;

    std::cout << "Parallel Search : Index = " << parallel_idx
              << " | Time: " << parallel_time << " seconds\n";

    if (parallel_time > 0.0)
    {
        std::cout << "Speedup         : " << (serial_time / parallel_time) << "x\n";
    }

    // --- Verification ---
    if (serial_idx == parallel_idx && serial_idx != -1)
    {
        std::cout << "Verification    : SUCCESS (Target found at correct index)\n";
    }
    else
    {
        std::cout << "Verification    : FAILED!\n";
    }

    return 0;
}

// task 2
#include <omp.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdlib>

// 1. Serial Bubble Sort
void bubble_sort_serial(std::vector<int> &a, int n)
{
    for (int i = 0; i < n - 1; i++)
    {
        for (int j = 0; j < n - i - 1; j++)
        {
            if (a[j] > a[j + 1])
            {
                std::swap(a[j], a[j + 1]);
            }
        }
    }
}

// 2. Parallel Odd-Even Transposition Sort (Parallel Bubble Sort)
void odd_even_sort_parallel(std::vector<int> &a, int n)
{
    for (int phase = 0; phase < n; phase++)
    {
        if (phase % 2 == 0)
        {
// Even Phase: Compare (0,1), (2,3), (4,5)...
#pragma omp parallel for default(none) shared(a, n)
            for (int i = 0; i < n - 1; i += 2)
            {
                if (a[i] > a[i + 1])
                {
                    std::swap(a[i], a[i + 1]);
                }
            }
        }
        else
        {
// Odd Phase: Compare (1,2), (3,4), (5,6)...
#pragma omp parallel for default(none) shared(a, n)
            for (int i = 1; i < n - 1; i += 2)
            {
                if (a[i] > a[i + 1])
                {
                    std::swap(a[i], a[i + 1]);
                }
            }
        }
    }
}

int main(int argc, char *argv[])
{
    int n = 10000; // Array size N (Odd-Even sort is O(N^2), so test with ~10k items)
    if (argc > 1)
    {
        n = std::atoi(argv[1]);
    }

    std::cout << "========================================\n";
    std::cout << "Running Parallel Odd-Even Sort (N = " << n << ")\n";
    std::cout << "========================================\n";

    // Create random input arrays
    std::vector<int> a_serial(n);
    for (int i = 0; i < n; i++)
    {
        a_serial[i] = std::rand() % 100000;
    }
    std::vector<int> a_parallel = a_serial; // Duplicate for parallel sorting

    // --- Serial Sort Timing ---
    double t0 = omp_get_wtime();
    bubble_sort_serial(a_serial, n);
    double t1 = omp_get_wtime();
    double serial_time = t1 - t0;

    std::cout << "Serial Bubble Sort   : " << serial_time << " seconds\n";

    // --- Parallel Sort Timing ---
    double t2 = omp_get_wtime();
    odd_even_sort_parallel(a_parallel, n);
    double t3 = omp_get_wtime();
    double parallel_time = t3 - t2;

    std::cout << "Parallel Odd-Even    : " << parallel_time << " seconds\n";
    if (parallel_time > 0.0)
    {
        std::cout << "Speedup              : " << (serial_time / parallel_time) << "x\n";
    }

    // --- Verification ---
    bool is_sorted = std::is_sorted(a_parallel.begin(), a_parallel.end());
    bool matches_serial = (a_serial == a_parallel);

    if (is_sorted && matches_serial)
    {
        std::cout << "Verification         : SUCCESS (Array sorted correctly and matches serial)\n";
    }
    else
    {
        std::cout << "Verification         : FAILED!\n";
    }

    return 0;
}

// task 3
#include <omp.h>
#include <iostream>
#include <vector>
#include <cstdlib>

// 1. Serial Histogram Calculation
void histogram_serial(const std::vector<int> &a, long n, std::vector<int> &count)
{
    for (long i = 0; i < n; i++)
    {
        count[a[i]]++;
    }
}

// 2. Parallel Histogram using Atomic Updates
void histogram_atomic(const std::vector<int> &a, long n, std::vector<int> &count)
{
#pragma omp parallel for
    for (long i = 0; i < n; i++)
    {
#pragma omp atomic
        count[a[i]]++;
    }
}

// 3. Parallel Histogram using Reduction
void histogram_reduction(const std::vector<int> &a, long n, std::vector<int> &count)
{
    int local_count[10] = {0};

// OpenMP array reduction creates thread-private copies of local_count
#pragma omp parallel for reduction(+ : local_count)
    for (long i = 0; i < n; i++)
    {
        local_count[a[i]]++;
    }

    for (int i = 0; i < 10; i++)
    {
        count[i] = local_count[i];
    }
}

int main(int argc, char *argv[])
{
    long n = 100000000; // 100 million elements
    if (argc > 1)
    {
        n = std::atol(argv[1]);
    }

    std::cout << "========================================\n";
    std::cout << "Running Parallel Histogram (N = " << n << ")\n";
    std::cout << "========================================\n";

    // Initialize array with numbers between 0 and 9
    std::vector<int> a(n);
    for (long i = 0; i < n; i++)
    {
        a[i] = static_cast<int>(i % 10);
    }

    // --- Serial Version ---
    std::vector<int> count_serial(10, 0);
    double t0 = omp_get_wtime();
    histogram_serial(a, n, count_serial);
    double t1 = omp_get_wtime();
    double serial_time = t1 - t0;
    std::cout << "Serial Time     : " << serial_time << " seconds\n";

    // --- Atomic Parallel Version ---
    std::vector<int> count_atomic(10, 0);
    double t2 = omp_get_wtime();
    histogram_atomic(a, n, count_atomic);
    double t3 = omp_get_wtime();
    double atomic_time = t3 - t2;
    std::cout << "Atomic Time     : " << atomic_time << " seconds\n";

    // --- Reduction Parallel Version ---
    std::vector<int> count_reduction(10, 0);
    double t4 = omp_get_wtime();
    histogram_reduction(a, n, count_reduction);
    double t5 = omp_get_wtime();
    double reduction_time = t5 - t4;
    std::cout << "Reduction Time  : " << reduction_time << " seconds\n";

    // --- Print Results & Verification ---
    std::cout << "\nHistogram Output Counts:\n";
    for (int i = 0; i < 10; i++)
    {
        std::cout << "Bin [" << i << "]: " << count_serial[i] << "\n";
    }

    bool matches = (count_serial == count_atomic) && (count_serial == count_reduction);
    if (matches)
    {
        std::cout << "\nVerification    : SUCCESS (All counts match identically)\n";
    }
    else
    {
        std::cout << "\nVerification    : FAILED!\n";
    }

    return 0;
}

// task 4
#include <omp.h>
#include <iostream>
#include <vector>
#include <cstdlib>

// 1. Serial Matrix Transpose
void transpose_serial(const std::vector<std::vector<double>> &A,
                      std::vector<std::vector<double>> &B, int n)
{
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            B[j][i] = A[i][j];
        }
    }
}

// 2. Parallel Matrix Transpose using collapse(2)
void transpose_parallel(const std::vector<std::vector<double>> &A,
                        std::vector<std::vector<double>> &B, int n)
{
#pragma omp parallel for collapse(2)
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            B[j][i] = A[i][j];
        }
    }
}

int main(int argc, char *argv[])
{
    int n = 2048; // Default matrix size N x N
    if (argc > 1)
    {
        n = std::atoi(argv[1]);
    }

    std::cout << "========================================\n";
    std::cout << "Running Matrix Transpose (N = " << n << "x" << n << ")\n";
    std::cout << "========================================\n";

    // Allocate 2D vectors
    std::vector<std::vector<double>> A(n, std::vector<double>(n));
    std::vector<std::vector<double>> B_serial(n, std::vector<double>(n));
    std::vector<std::vector<double>> B_parallel(n, std::vector<double>(n));

    // Initialize matrix A
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            A[i][j] = static_cast<double>(i * n + j) * 1.5;
        }
    }

    // --- Serial Version ---
    double t0 = omp_get_wtime();
    transpose_serial(A, B_serial, n);
    double t1 = omp_get_wtime();
    double serial_time = t1 - t0;
    std::cout << "Serial Time     : " << serial_time << " seconds\n";

    // --- Parallel Version ---
    double t2 = omp_get_wtime();
    transpose_parallel(A, B_parallel, n);
    double t3 = omp_get_wtime();
    double parallel_time = t3 - t2;
    std::cout << "Parallel Time   : " << parallel_time << " seconds\n";

    if (parallel_time > 0.0)
    {
        std::cout << "Speedup         : " << (serial_time / parallel_time) << "x\n";
    }

    // --- Verification ---
    bool match = true;
    for (int i = 0; i < n && match; i++)
    {
        for (int j = 0; j < n; j++)
        {
            if (B_serial[i][j] != B_parallel[i][j])
            {
                match = false;
                break;
            }
        }
    }

    if (match)
    {
        std::cout << "Verification    : SUCCESS (Transposed matrices match identically)\n";
    }
    else
    {
        std::cout << "Verification    : FAILED!\n";
    }

    return 0;
}

// task 5
#include <omp.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>

// 1. Serial 2D Convolution
void convolution2d_serial(const std::vector<std::vector<double>> &image,
                          std::vector<std::vector<double>> &output,
                          int H, int W)
{
    int out_h = H - 2;
    int out_w = W - 2;

    for (int r = 0; r < out_h; r++)
    {
        for (int c = 0; c < out_w; c++)
        {
            double sum = 0.0;
            // Apply 3x3 averaging filter
            for (int dr = 0; dr < 3; dr++)
            {
                for (int dc = 0; dc < 3; dc++)
                {
                    sum += image[r + dr][c + dc];
                }
            }
            output[r][c] = sum / 9.0;
        }
    }
}

// 2. Parallel 2D Convolution using OpenMP collapse(2)
void convolution2d_parallel(const std::vector<std::vector<double>> &image,
                            std::vector<std::vector<double>> &output,
                            int H, int W)
{
    int out_h = H - 2;
    int out_w = W - 2;

#pragma omp parallel for collapse(2)
    for (int r = 0; r < out_h; r++)
    {
        for (int c = 0; c < out_w; c++)
        {
            double sum = 0.0;
            // Apply 3x3 averaging filter
            for (int dr = 0; dr < 3; dr++)
            {
                for (int dc = 0; dc < 3; dc++)
                {
                    sum += image[r + dr][c + dc];
                }
            }
            output[r][c] = sum / 9.0;
        }
    }
}

int main(int argc, char *argv[])
{
    // Default matrix size H x W = 2048 x 2048
    int H = 2048;
    int W = 2048;
    if (argc > 2)
    {
        H = std::atoi(argv[1]);
        W = std::atoi(argv[2]);
    }

    std::cout << "========================================\n";
    std::cout << "Running 2D Convolution (Image: " << H << "x" << W << ")\n";
    std::cout << "========================================\n";

    int out_h = H - 2;
    int out_w = W - 2;

    // Allocate input image and output matrices
    std::vector<std::vector<double>> image(H, std::vector<double>(W));
    std::vector<std::vector<double>> output_serial(out_h, std::vector<double>(out_w));
    std::vector<std::vector<double>> output_parallel(out_h, std::vector<double>(out_w));

    // Initialize image with synthetic pixel values
    for (int r = 0; r < H; r++)
    {
        for (int c = 0; c < W; c++)
        {
            image[r][c] = static_cast<double>((r * 10 + c) % 256);
        }
    }

    // --- Serial Convolution ---
    double t0 = omp_get_wtime();
    convolution2d_serial(image, output_serial, H, W);
    double t1 = omp_get_wtime();
    double serial_time = t1 - t0;
    std::cout << "Serial Time     : " << serial_time << " seconds\n";

    // --- Parallel Convolution ---
    double t2 = omp_get_wtime();
    convolution2d_parallel(image, output_parallel, H, W);
    double t3 = omp_get_wtime();
    double parallel_time = t3 - t2;
    std::cout << "Parallel Time   : " << parallel_time << " seconds\n";

    if (parallel_time > 0.0)
    {
        std::cout << "Speedup         : " << (serial_time / parallel_time) << "x\n";
    }

    // --- Verification (Tolerance check) ---
    double tol = 1e-6;
    bool match = true;
    for (int r = 0; r < out_h && match; r++)
    {
        for (int c = 0; c < out_w; c++)
        {
            if (std::abs(output_serial[r][c] - output_parallel[r][c]) > tol)
            {
                match = false;
                break;
            }
        }
    }

    if (match)
    {
        std::cout << "Verification    : SUCCESS (Serial and Parallel results match within tolerance)\n";
    }
    else
    {
        std::cout << "Verification    : FAILED!\n";
    }

    return 0;
}
