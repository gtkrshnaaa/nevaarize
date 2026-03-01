#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    double val;
} Obj;

void printResult(const char* name, double ops, double sec) {
    printf("  %-15s | %15.2f OPS/sec | %.4fs\n", name, ops, sec);
}

void printHeader() {
    printf("  -------------------------------------------------------------\n");
    printf("  Benchmark       |     Performance     | Time\n");
    printf("  -------------------------------------------------------------\n");
}

double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

void benchInt() {
    long long limit = 1000000000;
    double start = get_time();
    
    volatile long long i = 0;
    while (i < limit) {
        i++;
    }
    
    double end = get_time();
    double sec = end - start;
    double ops = limit / sec;
    printResult("Integer Add", ops, sec);
}

void benchDouble() {
    long long limit = 100000000;
    double start = get_time();
    
    volatile double val = 0.0;
    long long i = 0;
    while (i < limit) {
        val += 1.1;
        i++;
    }
    
    double end = get_time();
    double sec = end - start;
    double ops = limit / sec;
    printResult("Double Arith", ops, sec);
}

void benchString() {
    long long limit = 500000;
    double start = get_time();
    
    // C strings (malloc/realloc simulation or simple strcat to big buffer?)
    // High-level languages utilize dynamic strings. We should simulate that somewhat.
    // Naive repeated strcat is O(N^2). This matches what "s += 'a'" does in truly immutable envs or simple dynamic arrays.
    
    char* s = (char*)malloc(1);
    s[0] = '\0';
    size_t len = 0;
    size_t cap = 1;
    
    long long i = 0;
    while (i < limit) {
        // Append 'a'
        if (len + 1 >= cap) {
            cap *= 2; // Basic growth strategy
            s = (char*)realloc(s, cap);
        }
        s[len] = 'a';
        s[len+1] = '\0';
        len++;
        i++;
    }
    
    double end = get_time();
    double sec = end - start;
    double ops = limit / sec;
    printResult("String Concat", ops, sec);
    free(s);
}

void benchArray() {
    long long limit = 1000000;
    double start = get_time();
    
    // Dynamic array simulation
    double* arr = (double*)malloc(sizeof(double));
    size_t size = 0;
    size_t cap = 1;
    
    long long i = 0;
    while (i < limit) {
        if (size >= cap) {
            cap *= 2;
            arr = (double*)realloc(arr, cap * sizeof(double));
        }
        arr[size++] = (double)i;
        i++;
    }
    
    double end = get_time();
    double sec = end - start;
    double ops = limit / sec;
    printResult("Array Push", ops, sec);
    free(arr);
}

void benchStruct() {
    long long limit = 50000000;
    Obj o = {0};
    double start = get_time();
    
    volatile double x;
    long long i = 0;
    while (i < limit) {
        o.val = (double)i;
        x = o.val;
        i++;
    }
    
    double end = get_time();
    double sec = end - start;
    double ops = limit / sec;
    printResult("Struct Access", ops, sec);
}

int main() {
    printf(">>> C (GCC optimized) Benchmark Suite <<<\n");
    printHeader();
    benchInt();
    benchDouble();
    benchString();
    benchArray();
    benchStruct();
    printf("  -------------------------------------------------------------\n");
    printf("\n");
    return 0;
}
