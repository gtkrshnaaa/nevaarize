/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * RuntimeGuard.hpp - Shared Runtime Protection System
 *
 * Lightweight, header-only guard providing four protection layers:
 *   1. Signal trap (SIGSEGV, SIGFPE, SIGBUS, SIGILL) with recovery
 *   2. Execution timeout via POSIX timer (kernel-managed, zero threads)
 *   3. Virtual memory cap via setrlimit (kernel-enforced, zero overhead)
 *   4. Recursion depth limiter (RAII counter, ~4 bytes per thread)
 *
 * Total memory footprint: ~32KB (alternate signal stack) + 52 bytes.
 * Runtime cost: negligible for well-behaved programs.
 */

#ifndef RUNTIME_GUARD_HPP
#define RUNTIME_GUARD_HPP

#include <csignal>
#include <csetjmp>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <ctime>
#include <sys/resource.h>
#include <stdexcept>

// ============================================================================
// Configuration Defaults
// ============================================================================

#ifndef GUARD_TIMEOUT_SECONDS
#define GUARD_TIMEOUT_SECONDS 30
#endif

#ifndef GUARD_MEMORY_LIMIT_MB
#define GUARD_MEMORY_LIMIT_MB 512
#endif

#ifndef GUARD_MAX_RECURSION_DEPTH
#define GUARD_MAX_RECURSION_DEPTH 10000
#endif

#ifndef GUARD_LOOP_CHECK_INTERVAL
#define GUARD_LOOP_CHECK_INTERVAL 100000
#endif

// ============================================================================
// Signal Recovery State (thread-local for async safety)
// ============================================================================

namespace runtime_guard {

/**
 * Thread-local jump buffer for signal-based crash recovery.
 * Used by the signal handler to longjmp back to a safe recovery point.
 */
inline thread_local sigjmp_buf recovery_env;

/**
 * Indicates whether the current thread is inside a guarded execution region.
 * When true, signal handler will attempt recovery via siglongjmp.
 * When false, signal handler performs a clean abort with diagnostics.
 */
inline thread_local bool in_guarded_execution = false;

/**
 * Stores the signal number that triggered recovery, for diagnostic output.
 */
inline thread_local int last_trapped_signal = 0;

/**
 * Recursion depth counter per thread.
 */
inline thread_local uint32_t recursion_depth = 0;

/**
 * Execution deadline timestamp (seconds since epoch via CLOCK_MONOTONIC).
 * Set to 0 when no timeout is active.
 */
inline thread_local uint64_t execution_deadline = 0;

/**
 * Flag set by SIGALRM handler to indicate timeout expiry.
 */
inline thread_local volatile sig_atomic_t timeout_fired = 0;

// ============================================================================
// Signal Names (diagnostic output)
// ============================================================================

inline const char* signalName(int sig) {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV (Segmentation Fault)";
        case SIGFPE:  return "SIGFPE (Floating Point Exception)";
        case SIGBUS:  return "SIGBUS (Bus Error)";
        case SIGILL:  return "SIGILL (Illegal Instruction)";
        case SIGALRM: return "SIGALRM (Timeout)";
        default:      return "Unknown Signal";
    }
}

// ============================================================================
// Signal Handlers
// ============================================================================

/**
 * Hardware fault handler for SIGSEGV, SIGFPE, SIGBUS, SIGILL.
 * Recovers via siglongjmp when inside a guarded region.
 * Performs clean abort with diagnostics when outside guarded region.
 */
inline void crashHandler(int sig, siginfo_t* si, void* /* unused */) {
    last_trapped_signal = sig;
    if (in_guarded_execution) {
        siglongjmp(recovery_env, sig);
    }
    fprintf(stderr,
            "\n[RuntimeGuard] Fatal %s at address %p\n",
            signalName(sig), si ? si->si_addr : nullptr);
    _exit(128 + sig);
}

/**
 * Timeout handler for SIGALRM.
 * Sets the timeout flag. Recovery is handled cooperatively
 * via checkTimeout() calls in loops and recursive evaluations.
 */
inline void timeoutHandler(int /* sig */, siginfo_t* /* si */, void* /* unused */) {
    timeout_fired = 1;
    // For JIT or uncooperative code, force recovery via longjmp
    if (in_guarded_execution) {
        last_trapped_signal = SIGALRM;
        siglongjmp(recovery_env, SIGALRM);
    }
}

// ============================================================================
// Core Guard API
// ============================================================================

/**
 * Initialize all four protection layers.
 * Call once at the start of main().
 *
 * @param timeoutSec  Execution timeout in seconds (0 = no timeout)
 * @param memLimitMB  Virtual memory cap in megabytes (0 = no limit)
 */
inline void initialize(
    uint32_t timeoutSec = GUARD_TIMEOUT_SECONDS,
    uint32_t memLimitMB = GUARD_MEMORY_LIMIT_MB
) {
    // ---- Layer 1: Alternate Signal Stack ----
    // Allocates a dedicated stack for signal handlers so that stack overflow
    // in user code doesn't prevent the handler from executing.
    stack_t ss;
    ss.ss_sp = malloc(SIGSTKSZ * 4);
    if (ss.ss_sp) {
        ss.ss_size = SIGSTKSZ * 4;
        ss.ss_flags = 0;
        sigaltstack(&ss, nullptr);
    }

    // ---- Layer 2: Hardware Fault Handlers ----
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = crashHandler;
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGFPE,  &sa, nullptr);
    sigaction(SIGBUS,  &sa, nullptr);
    sigaction(SIGILL,  &sa, nullptr);

    // ---- Layer 3: Execution Timeout (POSIX timer → SIGALRM) ----
    if (timeoutSec > 0) {
        struct sigaction ta;
        ta.sa_flags = SA_SIGINFO | SA_ONSTACK;
        sigemptyset(&ta.sa_mask);
        ta.sa_sigaction = timeoutHandler;
        sigaction(SIGALRM, &ta, nullptr);

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        execution_deadline = now.tv_sec + timeoutSec;

        timer_t timerId;
        struct sigevent sev;
        sev.sigev_notify = SIGEV_SIGNAL;
        sev.sigev_signo = SIGALRM;
        sev.sigev_value.sival_ptr = nullptr;
        if (timer_create(CLOCK_MONOTONIC, &sev, &timerId) == 0) {
            struct itimerspec its;
            its.it_value.tv_sec = timeoutSec;
            its.it_value.tv_nsec = 0;
            its.it_interval.tv_sec = 0;
            its.it_interval.tv_nsec = 0;
            timer_settime(timerId, 0, &its, nullptr);
        }
    }

    // ---- Layer 4: Virtual Memory Cap ----
    if (memLimitMB > 0) {
        struct rlimit rl;
        rl.rlim_cur = static_cast<rlim_t>(memLimitMB) * 1024 * 1024;
        rl.rlim_max = rl.rlim_cur;
        setrlimit(RLIMIT_AS, &rl);
    }
}

/**
 * Cooperative timeout check for interpreter loops.
 * Call periodically from hot loops (while, for) to detect timeout.
 * Uses a counter to avoid syscall overhead on every iteration.
 */
inline bool checkTimeout() {
    if (timeout_fired) return true;
    if (execution_deadline == 0) return false;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (static_cast<uint64_t>(now.tv_sec) >= execution_deadline) {
        timeout_fired = 1;
        return true;
    }
    return false;
}

// ============================================================================
// RAII Guards
// ============================================================================

/**
 * RAII guard for entering/exiting a guarded execution region.
 * When active, hardware faults trigger recovery instead of abort.
 */
struct ExecutionRegionGuard {
    bool was_guarded;
    ExecutionRegionGuard() : was_guarded(in_guarded_execution) {
        in_guarded_execution = true;
    }
    ~ExecutionRegionGuard() {
        in_guarded_execution = was_guarded;
    }
};

/**
 * RAII recursion depth guard.
 * Increments depth on construction, decrements on destruction.
 * Throws std::runtime_error if depth exceeds the configured maximum.
 */
struct RecursionGuard {
    RecursionGuard() {
        if (++recursion_depth > GUARD_MAX_RECURSION_DEPTH) {
            --recursion_depth;
            throw std::runtime_error(
                "Maximum recursion depth exceeded (" +
                std::to_string(GUARD_MAX_RECURSION_DEPTH) + ")");
        }
    }
    ~RecursionGuard() {
        --recursion_depth;
    }
};

} // namespace runtime_guard

#endif // RUNTIME_GUARD_HPP
