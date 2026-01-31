import time

class Obj:
    def __init__(self):
        self.val = 0

def print_header():
    print("  -------------------------------------------------------------")
    print(f"  {'Benchmark':<15} | {'Performance':>15} | Time")
    print("  -------------------------------------------------------------")

def print_result(name, ops, sec):
    ops_str = f"{ops:,.2f}"
    print(f"  {name:<15} | {ops_str:>15} OPS/sec | {sec:.4f}s")

def bench_int():
    limit = 1000000000
    start = time.time()
    i = 0
    while i < limit:
        i += 1
    end = time.time()
    sec = end - start
    ops = limit / sec
    print_result("Integer Add", ops, sec)

def bench_double():
    limit = 100000000
    val = 0.0
    start = time.time()
    i = 0
    while i < limit:
        val += 1.1
        i += 1
    end = time.time()
    sec = end - start
    ops = limit / sec
    print_result("Double Arith", ops, sec)

def bench_string():
    limit = 50000
    start = time.time()
    s = ""
    i = 0
    while i < limit:
        s += "a"
        i += 1
    end = time.time()
    sec = end - start
    ops = limit / sec
    print_result("String Concat", ops, sec)

def bench_array():
    limit = 1000000
    start = time.time()
    arr = []
    i = 0
    while i < limit:
        arr.append(i)
        i += 1
    end = time.time()
    sec = end - start
    ops = limit / sec
    print_result("Array Push", ops, sec)

def bench_struct():
    limit = 50000000
    o = Obj()
    start = time.time()
    i = 0
    while i < limit:
        o.val = i
        x = o.val
        i += 1
    end = time.time()
    sec = end - start
    ops = limit / sec
    print_result("Struct Access", ops, sec)

if __name__ == "__main__":
    print(">>> Python 3 Benchmark Suite <<<")
    print_header()
    bench_int()
    bench_double()
    bench_string()
    bench_array()
    bench_struct()
    print("  -------------------------------------------------------------")
    print("")
