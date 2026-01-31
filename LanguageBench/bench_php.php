<?php

class Obj {
    public $val = 0;
}

function printHeader() {
    echo "  -------------------------------------------------------------\n";
    echo sprintf("  %-15s | %15s | %s\n", "Benchmark", "Performance", "Time");
    echo "  -------------------------------------------------------------\n";
}

function printResult($name, $ops, $sec) {
    echo sprintf("  %-15s | %15s OPS/sec | %.4fs\n", 
        $name, 
        number_format($ops, 2), 
        $sec
    );
}

function benchInt() {
    $limit = 1000000000;
    $start = microtime(true);
    $i = 0;
    while ($i < $limit) {
        $i++;
    }
    $end = microtime(true);
    $sec = $end - $start;
    $ops = $limit / $sec;
    printResult("Integer Add", $ops, $sec);
}

function benchDouble() {
    $limit = 100000000;
    $val = 0.0;
    $start = microtime(true);
    $i = 0;
    while ($i < $limit) {
        $val += 1.1;
        $i++;
    }
    $end = microtime(true);
    $sec = $end - $start;
    $ops = $limit / $sec;
    printResult("Double Arith", $ops, $sec);
}

function benchString() {
    $limit = 50000;
    $start = microtime(true);
    $s = "";
    $i = 0;
    while ($i < $limit) {
        $s .= "a";
        $i++;
    }
    $end = microtime(true);
    $sec = $end - $start;
    $ops = $limit / $sec;
    printResult("String Concat", $ops, $sec);
}

function benchArray() {
    $limit = 1000000;
    $arr = [];
    $start = microtime(true);
    $i = 0;
    while ($i < $limit) {
        $arr[] = $i;
        $i++;
    }
    $end = microtime(true);
    $sec = $end - $start;
    $ops = $limit / $sec;
    printResult("Array Push", $ops, $sec);
}

function benchStruct() {
    $limit = 50000000;
    $o = new Obj();
    $start = microtime(true);
    $i = 0;
    while ($i < $limit) {
        $o->val = $i;
        $x = $o->val;
        $i++;
    }
    $end = microtime(true);
    $sec = $end - $start;
    $ops = $limit / $sec;
    printResult("Struct Access", $ops, $sec);
}

echo ">>> PHP 8 Benchmark Suite <<<\n";
printHeader();
benchInt();
benchDouble();
benchString();
benchArray();
benchStruct();
echo "  -------------------------------------------------------------\n";
echo "\n";
?>
