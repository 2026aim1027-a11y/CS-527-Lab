// CS527 Lab 8 - Task 5: 2D Convolution with a 3x3 filter
//
// Applies a 3x3 kernel to an N x N image using only valid positions (no
// padding), so the output is (N-2) x (N-2). out[i][j] is the value centred
// on image pixel (i+1, j+1), i.e. out[0][0] is O[1][1] in the handout.
//
// Compile: g++ -fopenmp -O2 -Wall task5_conv2d.cpp -o task5
// Run:     ./task5

#include <omp.h>
#include <cstdlib>
#include <iomanip>
#include <iostream>
using namespace std;

double **alloc_matrix(int rows, int cols) {
    double **M = new double *[rows];
    for (int i = 0; i < rows; i++)
        M[i] = new double[cols];
    return M;
}

void free_matrix(double **M, int rows) {
    for (int i = 0; i < rows; i++)
        delete[] M[i];
    delete[] M;
}

// ---------------- serial version ----------------
// For every output position: multiply the 3x3 window of the image by the
// kernel element by element and add up the 9 products.

void conv2d_serial(double **img, int n, double K[3][3], double **out) {
    int m = n - 2;
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < m; j++) {
            double sum = 0;
            for (int ki = 0; ki < 3; ki++)
                for (int kj = 0; kj < 3; kj++)
                    sum += img[i + ki][j + kj] * K[ki][kj];
            out[i][j] = sum;
        }
    }
}

// ---------------- 1. parallel over the output rows ----------------
// Every output element depends only on the input image (which is only read)
// and is written by exactly one thread, so no synchronization is needed.
// `sum` is declared inside the loop, so it is private to each thread.

void conv2d_parallel(double **img, int n, double K[3][3], double **out) {
    int m = n - 2;
    #pragma omp parallel for
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < m; j++) {
            double sum = 0;
            for (int ki = 0; ki < 3; ki++)
                for (int kj = 0; kj < 3; kj++)
                    sum += img[i + ki][j + kj] * K[ki][kj];
            out[i][j] = sum;
        }
    }
}

// ---------------- 2. parallel with collapse(2) ----------------

void conv2d_collapse(double **img, int n, double K[3][3], double **out) {
    int m = n - 2;
    #pragma omp parallel for collapse(2)
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < m; j++) {
            double sum = 0;
            for (int ki = 0; ki < 3; ki++)
                for (int kj = 0; kj < 3; kj++)
                    sum += img[i + ki][j + kj] * K[ki][kj];
            out[i][j] = sum;
        }
    }
}

// The parallel versions do the same additions in the same order as the
// serial one, so the results must be bit-for-bit equal (== is used).
bool same_matrix(double **X, double **Y, int m) {
    for (int i = 0; i < m; i++)
        for (int j = 0; j < m; j++)
            if (X[i][j] != Y[i][j])
                return false;
    return true;
}

void print_matrix(double **M, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++)
            cout << setw(8) << setprecision(2) << M[i][j];
        cout << endl;
    }
}

int main() {
    int sizes[3] = {1024, 2048, 4096};
    int threads[5] = {1, 2, 4, 8, 16}; // my laptop has 8 cores / 16 logical CPUs

    // 3x3 averaging kernel: every weight is 1/9
    double K[3][3];
    for (int a = 0; a < 3; a++)
        for (int b = 0; b < 3; b++)
            K[a][b] = 1.0 / 9.0;

    cout << "Task 5 - 2D Convolution (3x3 averaging filter)" << endl;
    cout << fixed;

    // ---------------- the 5x5 example from the handout ----------------
    {
        int n = 5, m = 3;
        double **img = alloc_matrix(n, n);
        double **out_s = alloc_matrix(m, m);
        double **out_p = alloc_matrix(m, m);
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                img[i][j] = 10 * (i * n + j + 1); // 10, 20, ..., 250

        conv2d_serial(img, n, K, out_s);
        omp_set_num_threads(4);
        conv2d_parallel(img, n, K, out_p);

        // expected answer worked out by hand: each output is the average of
        // its 3x3 window, e.g. (10+20+30+60+70+80+110+120+130)/9 = 70
        double expected[3][3] = {{70, 80, 90}, {120, 130, 140}, {170, 180, 190}};
        bool ok = true;
        for (int i = 0; i < m; i++)
            for (int j = 0; j < m; j++)
                if (out_p[i][j] < expected[i][j] - 1e-9 || out_p[i][j] > expected[i][j] + 1e-9)
                    ok = false;

        cout << endl << "Handout example: input image (5 x 5)" << endl;
        print_matrix(img, n, n);
        cout << "Output (3 x 3), parallel with 4 threads:" << endl;
        print_matrix(out_p, m, m);
        cout << "Matches the hand-computed answer (70 80 90 / 120 130 140 / 170 180 190): "
             << (ok ? "yes" : "NO") << endl;
        cout << "Parallel == serial: " << (same_matrix(out_s, out_p, m) ? "yes" : "NO") << endl;

        free_matrix(img, n);
        free_matrix(out_s, m);
        free_matrix(out_p, m);
    }

    // ---------------- timing ----------------
    for (int s = 0; s < 3; s++) {
        int n = sizes[s];
        int m = n - 2;
        double **img = alloc_matrix(n, n);
        double **out_s = alloc_matrix(m, m);
        double **out_p = alloc_matrix(m, m);

        // grayscale values 0..255
        srand(1);
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                img[i][j] = rand() % 256;

        // omp_get_wtime() only changes every 1 ms on my machine, so every
        // function is called `repeat` times and the average time is used.
        int repeat = (int)(400000000LL / ((long long)n * n)); // 381, 95, 23

        conv2d_serial(img, n, K, out_s); // warm-up call, not timed
        double start = omp_get_wtime();
        for (int r = 0; r < repeat; r++)
            conv2d_serial(img, n, K, out_s);
        double end = omp_get_wtime();
        double t_serial = (end - start) / repeat;

        cout << endl << "N = " << n << " (output " << m << " x " << m
             << ", average of " << repeat << " calls)" << endl;
        cout << "Serial: " << setprecision(3) << t_serial * 1000 << " ms" << endl;
        cout << "Threads   rows (ms)  speedup  efficiency   collapse(2) (ms)  speedup  efficiency   Correct?" << endl;

        for (int k = 0; k < 5; k++) {
            omp_set_num_threads(threads[k]);
            bool correct = true;

            start = omp_get_wtime();
            for (int r = 0; r < repeat; r++)
                conv2d_parallel(img, n, K, out_p);
            end = omp_get_wtime();
            double t_rows = (end - start) / repeat;
            if (!same_matrix(out_s, out_p, m))
                correct = false;

            start = omp_get_wtime();
            for (int r = 0; r < repeat; r++)
                conv2d_collapse(img, n, K, out_p);
            end = omp_get_wtime();
            double t_col = (end - start) / repeat;
            if (!same_matrix(out_s, out_p, m))
                correct = false;

            cout << setw(7) << threads[k]
                 << setw(12) << setprecision(3) << t_rows * 1000
                 << setw(9) << setprecision(2) << t_serial / t_rows
                 << setw(12) << t_serial / t_rows / threads[k]
                 << setw(19) << setprecision(3) << t_col * 1000
                 << setw(9) << setprecision(2) << t_serial / t_col
                 << setw(12) << t_serial / t_col / threads[k]
                 << "   " << (correct ? "yes" : "NO") << endl;
        }

        free_matrix(img, n);
        free_matrix(out_s, m);
        free_matrix(out_p, m);
    }

    return 0;
}
