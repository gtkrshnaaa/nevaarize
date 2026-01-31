#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>

using namespace std;
using namespace std::chrono;

struct Obj {
    double val;
};

void printResult(string name, double ops, double sec) {
    cout << "  " << left << setw(15) << name << " | " 
         << right << setw(15) << fixed << setprecision(2) << ops << " OPS/sec | " 
         << fixed << setprecision(4) << sec << "s" << endl;
}

void printHeader() {
    cout << "  -------------------------------------------------------------" << endl;
    cout << "  Benchmark       |     Performance     | Time" << endl;
    cout << "  -------------------------------------------------------------" << endl;
}

void benchInt() {
    long long limit = 1000000000;
    auto start = high_resolution_clock::now();
    
    // Volatile to prevent compiler from optimizing away the loop completely
    volatile long long i = 0;
    while (i < limit) {
        i++;
    }
    
    auto end = high_resolution_clock::now();
    double sec = duration_cast<nanoseconds>(end - start).count() / 1e9;
    double ops = limit / sec;
    printResult("Integer Add", ops, sec);
}

void benchDouble() {
    long long limit = 100000000;
    auto start = high_resolution_clock::now();
    
    // Prevent optimization
    volatile double v = 0.0;
    long long i = 0;
    while (i < limit) {
        v += 1.1;
        i++;
    }
    
    auto end = high_resolution_clock::now();
    double sec = duration_cast<nanoseconds>(end - start).count() / 1e9;
    double ops = limit / sec;
    printResult("Double Arith", ops, sec);
}

void benchString() {
    long long limit = 50000;
    string s = "";
    auto start = high_resolution_clock::now();
    
    long long i = 0;
    while (i < limit) {
        s += "a";
        i++;
    }
    
    auto end = high_resolution_clock::now();
    double sec = duration_cast<nanoseconds>(end - start).count() / 1e9;
    double ops = limit / sec;
    printResult("String Concat", ops, sec);
}

void benchArray() {
    long long limit = 1000000;
    vector<double> arr;
    arr.reserve(limit); // Optional optimization, but standard defaults usually grow. Let's not reserve to be fair with dynamic langs? 
    // Actually dynamic langs usually realloc. Let's reserve to see C++ peak, or not to be fair?
    // User wants to see "speed mereka". C++ vectors usually imply good memory management.
    // Let's stick to standard push_back without reserve to emulate "growing array" logic similar to Java ArrayList/JS Array.
    
    auto start = high_resolution_clock::now();
    
    long long i = 0;
    while (i < limit) {
        arr.push_back((double)i);
        i++;
    }
    
    auto end = high_resolution_clock::now();
    double sec = duration_cast<nanoseconds>(end - start).count() / 1e9;
    double ops = limit / sec;
    printResult("Array Push", ops, sec);
}

void benchStruct() {
    long long limit = 50000000;
    Obj o = {0};
    auto start = high_resolution_clock::now();
    
    volatile double x;
    long long i = 0;
    while (i < limit) {
        o.val = (double)i;
        x = o.val;
        i++;
    }
    (void)x;
    
    auto end = high_resolution_clock::now();
    double sec = duration_cast<nanoseconds>(end - start).count() / 1e9;
    double ops = limit / sec;
    printResult("Struct Access", ops, sec);
}

int main() {
    // Disable sync for speed
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    cout << ">>> C++23 Benchmark Suite <<<" << endl;
    printHeader();
    benchInt();
    benchDouble();
    benchString();
    benchArray();
    benchStruct();
    cout << "  -------------------------------------------------------------" << endl;
    cout << endl;
    return 0;
}
