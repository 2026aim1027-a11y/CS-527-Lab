// CS527 Lab 8 - Task 4: Matrix Transpose
//
// B = transpose(A), i.e. B[j][i] = A[i][j], for an N x N matrix.
// Versions: naive (parallel outer loop) and tiled / blocked (collapse(2)
// over the tiles).
//
// Compile: g++ -fopenmp -O2 -Wall task4_transpose.cpp -o task4
// Run:     ./task4

#include <omp.h>
#include <iomanip>
#include <iostream>
using namespace std;

const int TILE = 32; // 32 x 32 doubles = 8 KB per tile

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

// ---------------- serial versions ----------------

void transpose_serial(double **A, double **B, int n) {
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            B[j][i] = A[i][j];
}

void transpose_tiled_serial(double **A, double **B, int n) {
    for (int ii = 0; ii < n; ii += TILE)
        for (int jj = 0; jj < n; jj += TILE)
            for (int i = ii; i < ii + TILE && i < n; i++)
                for (int j = jj; j < jj + TILE && j < n; j++)
                    B[j][i] = A[i][j];
}

// ---------------- 1. naive: parallel outer loop ----------------
// Each thread gets a set of rows i of A. Every (i, j) writes a different
// B[j][i], so no two threads ever write the same element: no lock needed.

void transpose_parallel(double **A, double **B, int n) {
    #pragma omp parallel for
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            B[j][i] = A[i][j];
}

// ---------------- 2. tiled: parallel over the tiles ----------------
// The matrix is cut into TILE x TILE blocks. One block of A and the matching
// block of B both fit in the L1 cache, so the column-wise writes into B
// reuse cache lines instead of missing on every element.
// collapse(2) turns the two tile loops into one loop, so there are
// (n/TILE)^2 pieces of work to share out.

void transpose_tiled_parallel(double **A, double **B, int n) {
    #pragma omp parallel for collapse(2)
    for (int ii = 0; ii < n; ii += TILE)
        for (int jj = 0; jj < n; jj += TILE)
            for (int i = ii; i < ii + TILE && i < n; i++)
                for (int j = jj; j < jj + TILE && j < n; j++)
                    B[j][i] = A[i][j];
}

// B must be exactly the transpose of A
bool check_transpose(double **A, double **B, int n) {
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (B[j][i] != A[i][j])
                return false;
    return true;
}

void clear(double **M, int n) {
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            M[i][j] = -1;
}

int main() {
    int sizes[4] = {5, 1024, 2048, 4096};
    int threads[5] = {1, 2, 4, 8, 16}; // my laptop has 8 cores / 16 logical CPUs

    cout << "Task 4 - Matrix Transpose" << endl;
    cout << fixed;

    // ---------------- small example, printed ----------------
    {
        int n = 3;
        double **A = alloc_matrix(n);
        double **B = alloc_matrix(n);
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                A[i][j] = i * n + j + 1;
        omp_set_num_threads(4);
        transpose_parallel(A, B, n);
        cout << endl << "Example (3 x 3, 4 threads)" << endl << "A:              B = A^T:" << endl;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++)
                cout << setw(4) << (int)A[i][j];
            cout << "    ";
            for (int j = 0; j < n; j++)
                cout << setw(4) << (int)B[i][j];
            cout << endl;
        }
        free_matrix(A, n);
        free_matrix(B, n);
    }

    for (int s = 0; s < 4; s++) {
        int n = sizes[s];
        double **A = alloc_matrix(n);
        double **B = alloc_matrix(n);

        // every element has a different whole-number value, so == can be used
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                A[i][j] = (double)i * n + j;

        // omp_get_wtime() only changes every 1 ms on my machine, so every
        // function is called `repeat` times and the average time is used.
        int repeat;
        if (n <= 5)
            repeat = 100000;
        else
            repeat = (int)(1000000000LL / ((long long)n * n)); // 953, 238, 59
        if (repeat < 5)
            repeat = 5;

        clear(B, n);
        transpose_serial(A, B, n); // warm-up call, not timed
        double start = omp_get_wtime();
        for (int r = 0; r < repeat; r++)
            transpose_serial(A, B, n);
        double end = omp_get_wtime();
        double t_serial = (end - start) / repeat;
        bool ok_serial = check_transpose(A, B, n);

        clear(B, n);
        start = omp_get_wtime();
        for (int r = 0; r < repeat; r++)
            transpose_tiled_serial(A, B, n);
        end = omp_get_wtime();
        double t_tiled_serial = (end - start) / repeat;
        bool ok_tiled_serial = check_transpose(A, B, n);

        // small n: time in microseconds, otherwise in milliseconds
        double scale = (n <= 5) ? 1e6 : 1e3;
        const char *unit = (n <= 5) ? "us" : "ms";

        cout << endl << "N = " << n << " (average of " << repeat << " calls)" << endl;
        cout << "Serial naive: " << setprecision(3) << t_serial * scale << " " << unit
             << (ok_serial ? " (correct)" : " (WRONG)")
             << ",  serial tiled: " << t_tiled_serial * scale << " " << unit
             << (ok_tiled_serial ? " (correct)" : " (WRONG)") << endl;
        cout << "Threads   naive (" << unit << ")  speedup   tiled (" << unit
             << ")  speedup vs serial naive   Correct?" << endl;

        for (int k = 0; k < 5; k++) {
            omp_set_num_threads(threads[k]);

            clear(B, n);
            start = omp_get_wtime();
            for (int r = 0; r < repeat; r++)
                transpose_parallel(A, B, n);
            end = omp_get_wtime();
            double t_naive = (end - start) / repeat;
            bool ok1 = check_transpose(A, B, n);

            clear(B, n);
            start = omp_get_wtime();
            for (int r = 0; r < repeat; r++)
                transpose_tiled_parallel(A, B, n);
            end = omp_get_wtime();
            double t_tiled = (end - start) / repeat;
            bool ok2 = check_transpose(A, B, n);

            cout << setw(7) << threads[k]
                 << setw(13) << setprecision(3) << t_naive * scale
                 << setw(9) << setprecision(2) << t_serial / t_naive
                 << setw(13) << setprecision(3) << t_tiled * scale
                 << setw(25) << setprecision(2) << t_serial / t_tiled
                 << "   " << (ok1 && ok2 ? "yes" : "NO") << endl;
        }

        free_matrix(A, n);
        free_matrix(B, n);
    }

    return 0;
}
