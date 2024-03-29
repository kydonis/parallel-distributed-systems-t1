#include <stdio.h> // fprintf, printf
#include <stdlib.h> // EXIT_FAILURE, EXIT_SUCCESS
#include <stdint.h>
#include <pthread.h>
#include "readmtx.h" // readMtxFile
#include "coo2csc.h" // coo2csc
#include "arrayutils.h" // binarySearch, zeroOutArray, printArray
#include "controller.h"


struct Arg {
    uint32_t *row;
    uint32_t *col;
    uint32_t *res;
    uint32_t nc;
    uint32_t *colSizes;
    uint32_t **symmetricRowItems;
    uint32_t executeFrom;
    uint32_t executeTo;
};

int NUM_THREADS = 12;
pthread_mutex_t mutex;

void *multiplyMatrices(void *argument) {
    struct Arg a = *(struct Arg *) argument;

    for (uint32_t i = a.executeFrom; i < a.executeTo; i++) {
        for (uint32_t j = a.col[i]; j < a.col[i + 1]; j++) {
            uint32_t curRow = i;
            uint32_t curCol = a.row[j];
            if (curRow == curCol)
                continue;
            uint32_t fullRowSize = a.col[curRow + 1] - a.col[curRow] + a.colSizes[curRow];
            uint32_t *fullRow = (uint32_t *)malloc(fullRowSize * sizeof(uint32_t));
            mergeArrays(a.row + a.col[curRow], a.symmetricRowItems[curRow], fullRow, a.col[curRow + 1] - a.col[curRow], a.colSizes[curRow]);
            uint32_t fullColSize = a.col[curCol + 1] - a.col[curCol] + a.colSizes[curCol];
            uint32_t *fullCol = (uint32_t *)malloc(fullColSize * sizeof(uint32_t));
            mergeArrays(a.row + a.col[curCol], a.symmetricRowItems[curCol], fullCol, a.col[curCol + 1] - a.col[curCol], a.colSizes[curCol]);
            uint32_t sum = countCommonElementsInSortedArrays(fullRow, fullCol, fullRowSize, fullColSize);
            pthread_mutex_lock(&mutex);
            a.res[curCol] += sum;
            a.res[curRow] += sum;
            pthread_mutex_unlock(&mutex);
            free(fullRow);
            free(fullCol);
        }
    }
}

// V4 Parallel with pthreads
void cscParallelV4Pthreads(uint32_t *row, uint32_t *col, uint32_t *res, uint32_t nc) {
    pthread_t threads[NUM_THREADS];
    pthread_mutex_init(&mutex,NULL);

    uint32_t *colSizes = (uint32_t *)malloc(nc * sizeof(uint32_t));
    zeroOutArray(colSizes, nc);
    for (uint32_t i = 0; i < nc; i++) {
        for (uint32_t j = col[i]; j < col[i + 1]; j++) {
            colSizes[row[j]]++;
        }
    }

    uint32_t *colIndexes = (uint32_t *)malloc(nc * sizeof(uint32_t));
    zeroOutArray(colIndexes, nc);
    uint32_t **symmetricRowItems = (uint32_t **)malloc(nc * sizeof(uint32_t *));
    for (uint32_t i = 0; i < nc; i++)
        symmetricRowItems[i] = (uint32_t *)malloc(colSizes[i] * sizeof(uint32_t));
    for (uint32_t i = 0; i < nc; i++) {
        for (uint32_t j = col[i]; j < col[i + 1]; j++) {
            symmetricRowItems[row[j]][colIndexes[row[j]]] = i;
            colIndexes[row[j]]++;
        }
    }

    // Multiply
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        uint32_t executeFrom = i * nc / NUM_THREADS;
        uint32_t executeTo = (i + 1) * nc / NUM_THREADS;
        struct Arg *a = malloc(sizeof(struct Arg));
        (*a).row = row;
        (*a).col = col;
        (*a).res = res;
        (*a).nc = nc;
        (*a).colSizes = colSizes;
        (*a).symmetricRowItems = symmetricRowItems;
        (*a).executeFrom = executeFrom;
        (*a).executeTo = executeTo;
        pthread_create(&threads[i], NULL, multiplyMatrices, (void *) a);
    }

    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    for (uint32_t i = 0; i < nc; i++) {
        res[i] /= 2;
    }

    pthread_mutex_destroy(&mutex);
    for (uint32_t i = 0; i < nc; i++)
        free(symmetricRowItems[i]);
    free(symmetricRowItems);
    free(colIndexes);
    free(colSizes);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [matrix-market-filename]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (argc >= 3) {
        NUM_THREADS = atoi(argv[2]);
    }

    uint32_t *rowsCoo, *colsCoo, nr, nc, nnz;

    readMtxFile(argv[1], &rowsCoo, &colsCoo, &nr, &nc, &nnz);

    uint32_t *rowsCsc = (uint32_t *)malloc(nnz * sizeof(uint32_t));
    uint32_t *colsCsc = (uint32_t *)malloc((nc + 1) * sizeof(uint32_t));
    coo2csc(rowsCsc, colsCsc, rowsCoo, colsCoo, nnz, nc, 0);

    runAndPresentResult(rowsCsc, colsCsc, nc, cscParallelV4Pthreads, "V4 Parallel pthreads", "./v4-pt.txt", "./v4-pt-results.txt");

    free(rowsCoo);
    free(colsCoo);
    free(rowsCsc);
    free(colsCsc);
    pthread_exit(NULL);
}
