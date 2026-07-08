#include <stdio.h>
#include <stdlib.h>

#define N_SMALL  8
#define N_MEDIUM 64
#define N_LARGE  1024

void tiny_unroll_candidate(int* a) {
    for (int i = 0; i < N_SMALL; ++i) {
        a[i] = a[i] * 2 + 1;
    }
}

void vector_add(const float* x, const float* y, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        out[i] = x[i] + y[i];
    }
}

void licm_candidate(float* a, int n, float scale, float bias) {
    for (int i = 0; i < n; ++i) {
        float factor = scale * bias * 3.14159f; // loop-invariant
        a[i] = a[i] * factor;
    }
}

void nested_matrix_scale(float mat[][N_MEDIUM], int rows, float scale) {
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < N_MEDIUM; ++j) {
            mat[i][j] *= scale;
        }
    }
}

void naive_matmul(const float A[][N_SMALL], const float B[][N_SMALL],
                   float C[][N_SMALL]) {
    for (int i = 0; i < N_SMALL; ++i) {
        for (int j = 0; j < N_SMALL; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < N_SMALL; ++k) {
                sum += A[i][k] * B[k][j];
            }
            C[i][j] = sum;
        }
    }
}

long long sum_reduction(const int* a, int n) {
    long long total = 0;
    for (int i = 0; i < n; ++i) {
        total += a[i];
    }
    return total;
}

void conditional_loop(int* a, int n) {
    for (int i = 0; i < n; ++i) {
        if (a[i] % 2 == 0) {
            a[i] += 1;
        } else {
            a[i] -= 1;
        }
    }
}

void nested_small_inner(float* a, int outer_n) {
    for (int i = 0; i < outer_n; ++i) {
        for (int j = 0; j < 4; ++j) {
            a[i * 4 + j] += (float)j;
        }
    }
}

void fill_pattern(int* a, int n, int value) {
    for (int i = 0; i < n; ++i) {
        a[i] = value;
    }
}

int main(void) {
    int* ivec = malloc(N_LARGE * sizeof(int));
    float* fvec1 = malloc(N_LARGE * sizeof(float));
    float* fvec2 = malloc(N_LARGE * sizeof(float));
    float* fvec3 = malloc(N_LARGE * sizeof(float));
    static float matA[N_SMALL][N_SMALL];
    static float matB[N_SMALL][N_SMALL];
    static float matC[N_SMALL][N_SMALL];
    static float bigMat[N_MEDIUM][N_MEDIUM];

    if (!ivec || !fvec1 || !fvec2 || !fvec3) {
        fprintf(stderr, "allocation failed\n");
        return 1;
    }

    for (int i = 0; i < N_LARGE; ++i) {
        ivec[i] = i % 100;
        fvec1[i] = (float)i * 0.5f;
        fvec2[i] = (float)i * 0.25f;
    }
    for (int i = 0; i < N_SMALL; ++i) {
        for (int j = 0; j < N_SMALL; ++j) {
            matA[i][j] = (float)(i + j);
            matB[i][j] = (float)(i - j);
        }
    }
    for (int i = 0; i < N_MEDIUM; ++i)
        for (int j = 0; j < N_MEDIUM; ++j)
            bigMat[i][j] = 1.0f;

    tiny_unroll_candidate(ivec);
    vector_add(fvec1, fvec2, fvec3, N_LARGE);
    licm_candidate(fvec3, N_LARGE, 1.5f, 2.0f);
    nested_matrix_scale(bigMat, N_MEDIUM, 3.0f);
    naive_matmul(matA, matB, matC);
    long long total = sum_reduction(ivec, N_LARGE);
    conditional_loop(ivec, N_LARGE);
    nested_small_inner(fvec1, N_LARGE / 4);
    fill_pattern(ivec, N_LARGE, 7);

    // print a few values so nothing gets optimized away as dead code
    printf("sum_reduction total = %lld\n", total);
    printf("matC[0][0] = %f\n", matC[0][0]);
    printf("fvec3[10]  = %f\n", fvec3[10]);
    printf("bigMat[5][5] = %f\n", bigMat[5][5]);
    printf("ivec[0] = %d, ivec[1] = %d\n", ivec[0], ivec[1]);

    free(ivec);
    free(fvec1);
    free(fvec2);
    free(fvec3);

    return 0;
}