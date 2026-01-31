public class BenchJava {
    static class Obj {
        public double val;
    }

    private static void printResult(String name, double ops, double sec) {
        System.out.printf("  %-15s | %15.2f OPS/sec | %.4fs%n", name, ops, sec);
    }
    
    private static void printHeader() {
         System.out.println("  -------------------------------------------------------------");
         System.out.printf("  %-15s | %15s | %s%n", "Benchmark", "Performance", "Time");
         System.out.println("  -------------------------------------------------------------");
    }

    public static void benchInt() {
        long limit = 1000000000;
        long start = System.nanoTime();
        long i = 0;
        while (i < limit) {
            i++;
        }
        long end = System.nanoTime();
        double sec = (end - start) / 1e9;
        double ops = limit / sec;
        printResult("Integer Add", ops, sec);
    }

    public static void benchDouble() {
        long limit = 100000000;
        double val = 0.0;
        long start = System.nanoTime();
        long i = 0;
        while (i < limit) {
            val += 1.1;
            i++;
        }
        long end = System.nanoTime();
        double sec = (end - start) / 1e9;
        double ops = limit / sec;
        printResult("Double Arith", ops, sec);
    }

    public static void benchString() {
        long limit = 50000;
        String s = "";
        long start = System.nanoTime();
        long i = 0;
        while (i < limit) {
            s += "a";
            i++;
        }
        long end = System.nanoTime();
        double sec = (end - start) / 1e9;
        double ops = limit / sec;
        printResult("String Concat", ops, sec);
    }

    public static void benchArray() {
        int limit = 1000000;
        java.util.ArrayList<Double> arr = new java.util.ArrayList<>();
        long start = System.nanoTime();
        int i = 0;
        while (i < limit) {
            arr.add((double)i);
            i++;
        }
        long end = System.nanoTime();
        double sec = (end - start) / 1e9;
        double ops = limit / sec;
        printResult("Array Push", ops, sec);
    }

    public static void benchStruct() {
        long limit = 50000000;
        Obj o = new Obj();
        long start = System.nanoTime();
        long i = 0;
        while (i < limit) {
            o.val = i;
            double x = o.val;
            i++;
        }
        long end = System.nanoTime();
        double sec = (end - start) / 1e9;
        double ops = limit / sec;
        printResult("Struct Access", ops, sec);
    }

    public static void main(String[] args) {
        System.out.println(">>> Java 21 Benchmark Suite <<<");
        printHeader();
        benchInt();
        benchDouble();
        benchString();
        benchArray();
        benchStruct();
        System.out.println("  -------------------------------------------------------------");
        System.out.println("");
    }
}
