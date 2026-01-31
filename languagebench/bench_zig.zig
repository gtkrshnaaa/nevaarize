// Zig Benchmark Suite
// Comparable to C++/Nevaarize benchmarks

const std = @import("std");

fn benchInt() !void {
    const limit: i64 = 1_000_000_000;
    var timer = try std.time.Timer.start();
    
    var i: i64 = 0;
    while (i < limit) : (i += 1) {
        std.mem.doNotOptimizeAway(i);
    }
    
    const elapsed_ns = timer.read();
    const elapsed = @as(f64, @floatFromInt(elapsed_ns)) / 1_000_000_000.0;
    const ops = @as(f64, @floatFromInt(limit)) / elapsed;
    std.debug.print("  Integer Add     | {d:>15.2} OPS/sec | {d:.4}s\n", .{ ops, elapsed });
}

fn benchDouble() !void {
    const limit: i64 = 100_000_000;
    var timer = try std.time.Timer.start();
    
    var val: f64 = 0.0;
    var j: i64 = 0;
    while (j < limit) : (j += 1) {
        val += 1.1;
    }
    std.mem.doNotOptimizeAway(val);
    
    const elapsed_ns = timer.read();
    const elapsed = @as(f64, @floatFromInt(elapsed_ns)) / 1_000_000_000.0;
    const ops = @as(f64, @floatFromInt(limit)) / elapsed;
    std.debug.print("  Double Arith    | {d:>15.2} OPS/sec | {d:.4}s\n", .{ ops, elapsed });
}

fn benchString() !void {
    const limit: i64 = 50_000;
    const allocator = std.heap.page_allocator;
    var timer = try std.time.Timer.start();
    
    var list = std.ArrayList(u8){};
    defer list.deinit(allocator);
    
    var i: i64 = 0;
    while (i < limit) : (i += 1) {
        try list.append(allocator, 'a');
    }
    std.mem.doNotOptimizeAway(list.items.len);
    
    const elapsed_ns = timer.read();
    const elapsed = @as(f64, @floatFromInt(elapsed_ns)) / 1_000_000_000.0;
    const ops = @as(f64, @floatFromInt(limit)) / elapsed;
    std.debug.print("  String Concat   | {d:>15.2} OPS/sec | {d:.4}s\n", .{ ops, elapsed });
}

fn benchArray() !void {
    const limit: i64 = 1_000_000;
    const allocator = std.heap.page_allocator;
    var timer = try std.time.Timer.start();
    
    var list = std.ArrayList(i64){};
    defer list.deinit(allocator);
    
    var k: i64 = 0;
    while (k < limit) : (k += 1) {
        try list.append(allocator, k);
    }
    std.mem.doNotOptimizeAway(list.items.len);
    
    const elapsed_ns = timer.read();
    const elapsed = @as(f64, @floatFromInt(elapsed_ns)) / 1_000_000_000.0;
    const ops = @as(f64, @floatFromInt(limit)) / elapsed;
    std.debug.print("  Array Push      | {d:>15.2} OPS/sec | {d:.4}s\n", .{ ops, elapsed });
}

const Obj = struct {
    val: i64,
};

fn benchStruct() !void {
    const limit: i64 = 50_000_000;
    var timer = try std.time.Timer.start();
    
    var o = Obj{ .val = 0 };
    var i: i64 = 0;
    while (i < limit) : (i += 1) {
        o.val = i;
        const x = o.val;
        std.mem.doNotOptimizeAway(x);
    }
    
    const elapsed_ns = timer.read();
    const elapsed = @as(f64, @floatFromInt(elapsed_ns)) / 1_000_000_000.0;
    const ops = @as(f64, @floatFromInt(limit)) / elapsed;
    std.debug.print("  Struct Access   | {d:>15.2} OPS/sec | {d:.4}s\n", .{ ops, elapsed });
}

pub fn main() !void {
    std.debug.print(">>> Zig Benchmark Suite <<<\n", .{});
    std.debug.print("  -------------------------------------------------------------\n", .{});
    std.debug.print("  Benchmark       |     Performance | Time\n", .{});
    std.debug.print("  -------------------------------------------------------------\n", .{});
    try benchInt();
    try benchDouble();
    try benchString();
    try benchArray();
    try benchStruct();
    std.debug.print("  -------------------------------------------------------------\n", .{});
    std.debug.print("\n", .{});
}
