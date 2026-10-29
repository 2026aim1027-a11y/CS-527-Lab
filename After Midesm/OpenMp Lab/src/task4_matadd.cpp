// CS527 OpenMP Lab - Task 4: Matrix Addition
// C[i][j] = A[i][j] + B[i][j]
//
// Matrices are stored as 2D arrays (an array of row pointers), so an element
// is written as A[i][j].
//
// Compile: g++ -fopenmp -O2 -Wall task4_matadd.cpp -o task4
// Run:     ./task4

#include <omp.h>
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

void mat_add_serial(double **A, double **B, double **C, int n) {
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            C[i][j] = A[i][j] + B[i][j];
}

// Only the outer loop is parallel: the n rows are divided among the threads.
void mat_add_outer(double **A, double **B, double **C, int n) {
    #pragma omp parallel for
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            C[i][j] = A[i][j] + B[i][j];
}

// collapse(2): the two loops become one loop of n*n iterations,
// and those n*n elements are divided among the threads.
void mat_add_collapse(double **A, double **B, double **C, int n) {
    #pragma omp parallel for collapse(2)
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            C[i][j] = A[i][j] + B[i][j];
}

// returns true if the two matrices are exactly equal
bool same_matrix(double **X, double **Y, int n) {
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (X[i][j] != Y[i][j])
                return false;
    return true;
}

int main() {
    int sizes[2] = {8, 4096};
    int threads[5] = {1, 2, 4, 8, 16}; // my laptop has 8 cores / 16 logical CPUs

    cout << "Task 4 - Matrix Addition" << endl;
    cout << fixed << setprecision(3); // print decimals normally, 3 digits

    for (int s = 0; s < 2; s++) {
        int n = sizes[s];

        // One addition of an 8x8 matrix is far too quick to time, so every
        // function is called `repeat` times and the average time is reported.
        int repeat = (n == 8) ? 100000 : 10;

        double **A = alloc_matrix(n);
        double **B = alloc_matrix(n);
        double **C_serial = alloc_matrix(n);
        double **C_outer = alloc_matrix(n);
        double **C_collapse = alloc_matrix(n);

        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                A[i][j] = i + j;
                B[i][j] = i - j;
                C_serial[i][j] = 0;
                C_outer[i][j] = 0;
                C_collapse[i][j] = 0;
            }
        }

        // ---- serial ----
        double start = omp_get_wtime();
        for (int r = 0; r < repeat; r++)
            mat_add_serial(A, B, C_serial, n);
        double t_serial = (omp_get_wtime() - start) / repeat;

        cout << endl << "N = " << n << " (average of " << repeat
             << " calls, time in microseconds)" << endl;
        cout << "Threads    outer only (us)    Speedup    collapse(2) (us)    Speedup    Correct?" << endl;
        cout << setw(7) << "serial" << setw(19) << t_serial * 1e6 << setw(11) << 1.0
             << setw(20) << "-" << setw(11) << "-" << "    -" << endl;

        for (int k = 0; k < 5; k++) {
            omp_set_num_threads(threads[k]);

            // ---- parallel outer loop only ----
            start = omp_get_wtime();
            for (int r = 0; r < repeat; r++)
                mat_add_outer(A, B, C_outer, n);
            double t_outer = (omp_get_wtime() - start) / repeat;

            // ---- parallel with collapse(2) ----
            start = omp_get_wtime();
            for (int r = 0; r < repeat; r++)
                mat_add_collapse(A, B, C_collapse, n);
            double t_collapse = (omp_get_wtime() - start) / repeat;

            bool correct = same_matrix(C_serial, C_outer, n) && same_matrix(C_serial, C_collapse, n);

            cout << setw(7) << threads[k]
                 << setw(19) << t_outer * 1e6 << setw(11) << t_serial / t_outer
                 << setw(20) << t_collapse * 1e6 << setw(11) << t_serial / t_collapse
                 << "    " << (correct ? "yes" : "NO") << endl;
        }

        free_matrix(A, n);
        free_matrix(B, n);
        free_matrix(C_serial, n);
        free_matrix(C_outer, n);
        free_matrix(C_collapse, n);
    }

    return 0;
}
