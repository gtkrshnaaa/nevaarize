function printHeader()
    print("  -------------------------------------------------------------")
    print(string.format("  %-15s | %15s | %s", "Benchmark", "Performance", "Time"))
    print("  -------------------------------------------------------------")
end

-- Helper to format number with commas
function formatNum(num)
    local formatted = string.format("%.2f", num)
    local k
    while true do  
        formatted, k = string.gsub(formatted, "^(-?%d+)(%d%d%d)", '%1,%2')
        if (k==0) then
          break
        end
    end
    return formatted
end

function printResult(name, ops, sec)
    local opsStr = formatNum(ops)
    print(string.format("  %-15s | %15s OPS/sec | %.4fs", name, opsStr, sec))
end

function benchInt()
    local limit = 1000000000
    local start = os.clock()
    local i = 0
    while i < limit do
        i = i + 1
    end
    local finish = os.clock()
    local sec = finish - start
    local ops = limit / sec
    printResult("Integer Add", ops, sec)
end

function benchDouble()
    local limit = 100000000
    local val = 0.0
    local start = os.clock()
    local i = 0
    while i < limit do
        val = val + 1.1
        i = i + 1
    end
    local finish = os.clock()
    local sec = finish - start
    local ops = limit / sec
    printResult("Double Arith", ops, sec)
end

function benchString()
    local limit = 50000
    local start = os.clock()
    local s = ""
    local i = 0
    while i < limit do
        s = s .. "a"
        i = i + 1
    end
    local finish = os.clock()
    local sec = finish - start
    local ops = limit / sec
    printResult("String Concat", ops, sec)
end

function benchArray()
    local limit = 1000000
    local arr = {}
    local start = os.clock()
    local i = 0
    while i < limit do
        arr[i] = i
        i = i + 1
    end
    local finish = os.clock()
    local sec = finish - start
    local ops = limit / sec
    printResult("Array Push", ops, sec)
end

function benchStruct()
    local limit = 50000000
    local o = { val = 0 }
    local start = os.clock()
    local i = 0
    while i < limit do
        o.val = i
        local x = o.val
        i = i + 1
    end
    local finish = os.clock()
    local sec = finish - start
    local ops = limit / sec
    printResult("Struct Access", ops, sec)
end

print(">>> Lua 5.4 Benchmark Suite <<<")
printHeader()
benchInt()
benchDouble()
benchString()
benchArray()
benchStruct()
print("  -------------------------------------------------------------")
print("")
