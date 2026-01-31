package main

import (
	"fmt"
	"time"
)

type Obj struct {
	val float64
}

func printHeader() {
	fmt.Println("  -------------------------------------------------------------")
	fmt.Println("  Benchmark       |     Performance     | Time")
	fmt.Println("  -------------------------------------------------------------")
}

func printResult(name string, ops float64, sec float64) {
	fmt.Printf("  %-15s | %15.2f OPS/sec | %.4fs\n", name, ops, sec)
}

func benchInt() {
	limit := 1000000000
	start := time.Now()
	i := 0
	for i < limit {
		i++
	}
	sec := time.Since(start).Seconds()
	ops := float64(limit) / sec
	printResult("Integer Add", ops, sec)
}

func benchDouble() {
	limit := 100000000
	val := 0.0
	start := time.Now()
	i := 0
	for i < limit {
		val += 1.1
		i++
	}
	sec := time.Since(start).Seconds()
	ops := float64(limit) / sec
	printResult("Double Arith", ops, sec)
}

func benchString() {
	limit := 50000
	s := ""
	start := time.Now()
	i := 0
	for i < limit {
		s += "a"
		i++
	}
	sec := time.Since(start).Seconds()
	ops := float64(limit) / sec
	printResult("String Concat", ops, sec)
}

func benchArray() {
	limit := 1000000
	var arr []float64
	start := time.Now()
	i := 0
	for i < limit {
		arr = append(arr, float64(i))
		i++
	}
	sec := time.Since(start).Seconds()
	ops := float64(limit) / sec
	printResult("Array Push", ops, sec)
}

func benchStruct() {
	limit := 50000000
	o := Obj{val: 0}
	start := time.Now()
	i := 0
	for i < limit {
		o.val = float64(i)
		_ = o.val
		i++
	}
	sec := time.Since(start).Seconds()
	ops := float64(limit) / sec
	printResult("Struct Access", ops, sec)
}

func main() {
	fmt.Println(">>> Go 1.21 Benchmark Suite <<<")
	printHeader()
	benchInt()
	benchDouble()
	benchString()
	benchArray()
	benchStruct()
	fmt.Println("  -------------------------------------------------------------")
	fmt.Println("")
}
