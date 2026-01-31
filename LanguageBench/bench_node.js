function printHeader() {
    console.log("  -------------------------------------------------------------");
    console.log("  Benchmark       |     Performance     | Time");
    console.log("  -------------------------------------------------------------");
}

function printResult(name, ops, sec) {
    // Basic padding implementation since native padding is complex across versions
    const padName = name.padEnd(15);
    const opsStr = ops.toFixed(2).replace(/\B(?=(\d{3})+(?!\d))/g, ",");
    const padOps = opsStr.padStart(15) + " OPS/sec";
    console.log(`  ${padName} | ${padOps} | ${sec.toFixed(4)}s`);
}

function benchInt() {
    const limit = 1000000000;
    const start = process.hrtime.bigint();
    let i = 0;
    while (i < limit) {
        i++;
    }
    const end = process.hrtime.bigint();
    const sec = Number(end - start) / 1e9;
    const ops = limit / sec;
    printResult("Integer Add", ops, sec);
}

function benchDouble() {
    const limit = 100000000;
    let val = 0.0;
    const start = process.hrtime.bigint();
    let i = 0;
    while (i < limit) {
        val += 1.1;
        i++;
    }
    const end = process.hrtime.bigint();
    const sec = Number(end - start) / 1e9;
    const ops = limit / sec;
    printResult("Double Arith", ops, sec);
}

function benchString() {
    const limit = 50000;
    const start = process.hrtime.bigint();
    let s = "";
    let i = 0;
    while (i < limit) {
        s += "a";
        i++;
    }
    const end = process.hrtime.bigint();
    const sec = Number(end - start) / 1e9;
    const ops = limit / sec;
    printResult("String Concat", ops, sec);
}

function benchArray() {
    const limit = 1000000;
    const arr = [];
    const start = process.hrtime.bigint();
    let i = 0;
    while (i < limit) {
        arr.push(i);
        i++;
    }
    const end = process.hrtime.bigint();
    const sec = Number(end - start) / 1e9;
    const ops = limit / sec;
    printResult("Array Push", ops, sec);
}

function benchStruct() {
    const limit = 50000000;
    const o = { val: 0 };
    const start = process.hrtime.bigint();
    let i = 0;
    while (i < limit) {
        o.val = i;
        let x = o.val;
        i++;
    }
    const end = process.hrtime.bigint();
    const sec = Number(end - start) / 1e9;
    const ops = limit / sec;
    printResult("Struct Access", ops, sec);
}

console.log(">>> Node.js Benchmark Suite <<<");
printHeader();
benchInt();
benchDouble();
benchString();
benchArray();
benchStruct();
console.log("  -------------------------------------------------------------");
console.log("");
