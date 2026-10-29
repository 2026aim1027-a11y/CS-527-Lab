// CS527 OpenMP Lab - Task 5: Matrix-Matrix Multiplication (Naive)
// C = A x B with the triple loop.
//
// Matrices are stored as 2D arrays (an array of row pointers), so an element
// is written as A[i][j].
//
// Compile: g++ -fopenmp -O2 -Wall task5_matmul.cpp -o task5
// Run:     ./task5

#include <omp.h>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
using namespace std;

double **alloc_matrix(int n) {
    double **M = new double *[n];
    for (int i = 0; i < n; i++)
        M[i] = new double[n];
    return M;
}

void free_matrix(double **M, int n) {
    for (int i = 0; i < n; i++)
        delete[] M[i];
    delete[] M;
}

// ---------------- loop order i, j, k ----------------
// The sum for one element is collected in a local variable and written to
// C[i][j] once, so C does not depend on what was stored in it before.

void matmul_serial(double **A, double **B, double **C, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            for (int k = 0; k < n; k++)
                sum += A[i][k] * B[k][j];
            C[i][j] = sum;
        }
    }
}

// Only the i loop is parallel: each thread computes whole rows of C.
void matmul_parallel_i(double **A, double **B, double **C, int n) {
    #pragma omp parallel for
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            for (int k = 0; k < n; k++)
                sum += A[i][k] * B[k][j];
            C[i][j] = sum;
        }
    }
}

// collapse(2) on i and j: the n*n elements of C are divided among the threads.
// The k loop is NOT collapsed, it stays a normal loop inside each element.
void matmul_parallel_collapse(double **A, double **B, double **C, int n) {
    #pragma omp parallel for collapse(2)
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            for (int k = 0; k < n; k++)
                sum += A[i][k] * B[k][j];
            C[i][j] = sum;
        }
    }
}

// ---------------- bonus: loop order i, k, j ----------------
// Now the inner loop moves along a row of B and a row of C (row-wise access).

void matmul_serial_ikj(double **A, double **B, double **C, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++)
            C[i][j] = 0.0;
        for (int k = 0; k < n; k++)
            for (int j = 0; j < n; j++)
                C[i][j] += A[i][k] * B[k][j];
    }
}

void matmul_parallel_ikj(double **A, double **B, double **C, int n) {
    #pragma omp parallel for
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++)
            C[i][j] = 0.0;
        for (int k = 0; k < n; k++)
            for (int j = 0; j < n; j++)
                C[i][j] += A[i][k] * B[k][j];
    }
}

// returns true if the two matrices are equal within a small tolerance
bool same_matrix(double **X, double **Y, int n) {
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (fabs(X[i][j] - Y[i][j]) > 1e-6)
                return false;
    return true;
}

int main() {
    int sizes[3] = {256, 512, 1024};
    // omp_get_wtime() only changes every 1 ms on my machine, so every function
    // is called `repeat` times and the average time is used (the small sizes
    // are fast, so they are repeated more often).
    int repeats[3] = {20, 5, 2};
    int threads[5] = {1, 2, 4, 8, 16}; // my laptop has 8 cores / 16 logical CPUs

    cout << "Task 5 - Matrix Multiplication" << endl;
    cout << "Efficiency = speedup / number of threads" << endl;
    cout << fixed << setprecision(2); // print decimals normally, 2 digits

    for (int s = 0; s < 3; s++) {
        int n = sizes[s];
        int repeat = repeats[s];

        double **A = alloc_matrix(n);
        double **B = alloc_matrix(n);
        double **C_serial = alloc_matrix(n);
        double **C_par = alloc_matrix(n);

        srand(1);
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                A[i][j] = (double)rand() / RAND_MAX;
                B[i][j] = (double)rand() / RAND_MAX;
                C_serial[i][j] = 0.0; // C is zero-initialized
                C_par[i][j] = 0.0;
            }
        }

        // ---- serial i, j, k ----
        double start = omp_get_wtime();
        for (int r = 0; r < repeat; r++)
            matmul_serial(A, B, C_serial, n);
        double end = omp_get_wtime();
        double t_serial = (end - start) / repeat;

        cout << endl << "N = " << n << " (average of " << repeat << " calls), loop order i,j,k"
             << ", serial time = " << t_serial * 1000 << " ms" << endl;
        cout << "Threads    i loop (ms)    Speedup    Efficiency    collapse(2) (ms)    Speedup    Efficiency    Correct?" << endl;

        for (int k = 0; k < 5; k++) {
            int T = threads[k];
            omp_set_num_threads(T);

            // ---- parallel i loop ----
            start = omp_get_wtime();
            for (int r = 0; r < repeat; r++)
                matmul_parallel_i(A, B, C_par, n);
            end = omp_get_wtime();
            double t_i = (end - start) / repeat;
            bool correct = same_matrix(C_serial, C_par, n);

            // ---- parallel collapse(2) ----
            start = omp_get_wtime();
            for (int r = 0; r < repeat; r++)
                matmul_parallel_collapse(A, B, C_par, n);
            end = omp_get_wtime();
            double t_c = (end - start) / repeat;
            if (!same_matrix(C_serial, C_par, n))
                correct = false;

            cout << setw(7) << T
                 << setw(15) << t_i * 1000 << setw(11) << t_serial / t_i
                 << setw(14) << t_serial / t_i / T
                 << setw(20) << t_c * 1000 << setw(11) << t_serial / t_c
                 << setw(14) << t_serial / t_c / T
                 << "    " << (correct ? "yes" : "NO") << endl;
        }

        // ---- bonus: serial i, k, j ----
        start = omp_get_wtime();
        for (int r = 0; r < repeat; r++)
            matmul_serial_ikj(A, B, C_par, n);
        end = omp_get_wtime();
        double t_serial_ikj = (end - start) / repeat;

        cout << endl << "N = " << n << ", loop order i,k,j (bonus)"
             << ", serial time = " << t_serial_ikj * 1000 << " ms, same result as i,j,k: "
             << (same_matrix(C_serial, C_par, n) ? "yes" : "NO") << endl;
        cout << "Threads    i loop (ms)    Speedup    Efficiency    Correct?" << endl;

        for (int k = 0; k < 5; k++) {
            int T = threads[k];
            omp_set_num_threads(T);

            start = omp_get_wtime();
            for (int r = 0; r < repeat; r++)
                matmul_parallel_ikj(A, B, C_par, n);
            end = omp_get_wtime();
            double t_ikj = (end - start) / repeat;

            cout << setw(7) << T
                 << setw(15) << t_ikj * 1000 << setw(11) << t_serial_ikj / t_ikj
                 << setw(14) << t_serial_ikj / t_ikj / T
                 << "    " << (same_matrix(C_serial, C_par, n) ? "yes" : "NO") << endl;
        }

        free_matrix(A, n);
        free_matrix(B, n);
        free_matrix(C_serial, n);
        free_matrix(C_par, n);
    }

    return 0;
}
