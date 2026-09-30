#!/usr/bin/env bash
# ==============================================================================
# Nevaarize Isolated Test Runner
# Executes test suites and benchmarks inside a resource-constrained container.
# Enforces a hard 50% cap on host CPU cores and physical RAM to prevent
# host lockups, swap thrashing, or thermal reboots.
# ==============================================================================

set -euo pipefail

IMAGE_NAME="nevaarize-test:latest"
WORKSPACE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ------------------------------------------------------------------------------
# Resource Discovery & 50% Boundary Calculation
# ------------------------------------------------------------------------------
HOST_CORES=$(nproc)
LIMIT_CORES=$(( HOST_CORES / 2 ))
if [ "$LIMIT_CORES" -lt 1 ]; then
    LIMIT_CORES=1
fi

TOTAL_MEM_KB=$(awk '/MemTotal/ {print $2}' /proc/meminfo)
LIMIT_MEM_MB=$(( TOTAL_MEM_KB / 1024 / 2 ))

echo "================================================================================"
echo "[*] Nevaarize Isolated Container Sandbox"
echo "--------------------------------------------------------------------------------"
echo "[*] Host CPU Cores   : ${HOST_CORES} -> Limit (50%): ${LIMIT_CORES} cores"
echo "[*] Host Total RAM   : $(( TOTAL_MEM_KB / 1024 )) MB -> Limit (50%): ${LIMIT_MEM_MB} MB"
echo "[*] Swap Thrash Guard: Enforced (Swap Limit = Memory Limit)"
echo "================================================================================"

# ------------------------------------------------------------------------------
# Pre-flight Image Check
# ------------------------------------------------------------------------------
if ! docker image inspect "${IMAGE_NAME}" >/dev/null 2>&1; then
    echo "[*] Docker image ${IMAGE_NAME} not found. Building..."
    docker build -t "${IMAGE_NAME}" -f "${WORKSPACE_DIR}/Dockerfile" "${WORKSPACE_DIR}"
    echo "[OK] Image build completed."
fi

# ------------------------------------------------------------------------------
# Execution Helper
# ------------------------------------------------------------------------------
run_sandbox() {
    local tty_arg=""
    if [ -t 0 ] && [ -t 1 ]; then
        tty_arg="-it"
    else
        tty_arg="-i"
    fi

    set +e
    docker run --rm ${tty_arg} \
        --memory="${LIMIT_MEM_MB}m" \
        --memory-swap="${LIMIT_MEM_MB}m" \
        --cpus="${LIMIT_CORES}.0" \
        -v "${WORKSPACE_DIR}:/workspace" \
        -w /workspace \
        "${IMAGE_NAME}" \
        "$@"
    local exit_code=$?
    set -e

    if [ $exit_code -eq 137 ]; then
        echo "--------------------------------------------------------------------------------"
        echo "[!] Container terminated with exit code 137 (OOMKilled)."
        echo "[!] Memory exceeded the 50% host threshold (${LIMIT_MEM_MB} MB)."
        echo "[!] Host system remains fully protected and responsive."
        echo "--------------------------------------------------------------------------------"
        return 137
    elif [ $exit_code -ne 0 ]; then
        echo "[x] Command failed with exit code ${exit_code}."
        return $exit_code
    fi
    return 0
}

# ------------------------------------------------------------------------------
# Subcommand Dispatch
# ------------------------------------------------------------------------------
MODE="${1:-all}"

case "$MODE" in
    all)
        echo "[*] Running default test suite: clean build and smoke tests..."
        run_sandbox bash -c "make clean && make release && ./bin/nevaarize examples/basics.nva"
        echo "[OK] Test suite execution finished successfully."
        ;;

    build)
        echo "[*] Compiling release binary in sandbox..."
        run_sandbox make release
        echo "[OK] Build completed."
        ;;

    build-asan)
        echo "[*] Compiling AddressSanitizer/LeakSanitizer binary in sandbox..."
        run_sandbox bash -c "make clean && make debug CXXFLAGS_DEBUG='-std=c++23 -Wall -Wextra -g -O1 -fsanitize=address,leak -fno-omit-frame-pointer -DDEBUG -MMD -MP'"
        echo "[OK] ASan build completed (bin/nevaarize-debug)."
        ;;

    run)
        if [ -z "${2:-}" ]; then
            echo "[x] Error: specify script path. Usage: ./test.sh run <path/to/script.nva>"
            exit 1
        fi
        run_sandbox bash -c "test -f bin/nevaarize || make release"
        echo "[*] Executing script: $2"
        run_sandbox ./bin/nevaarize "$2"
        ;;

    asan)
        if [ -z "${2:-}" ]; then
            echo "[x] Error: specify script path. Usage: ./test.sh asan <path/to/script.nva>"
            exit 1
        fi
        echo "[*] Ensuring ASan debug binary is built..."
        run_sandbox bash -c "test -f bin/nevaarize-debug || make debug CXXFLAGS_DEBUG='-std=c++23 -Wall -Wextra -g -O1 -fsanitize=address,leak -fno-omit-frame-pointer -DDEBUG -MMD -MP'"
        echo "[*] Running script with AddressSanitizer & LeakSanitizer: $2"
        run_sandbox ./bin/nevaarize-debug "$2"
        ;;

    valgrind)
        if [ -z "${2:-}" ]; then
            echo "[x] Error: specify script path. Usage: ./test.sh valgrind <path/to/script.nva>"
            exit 1
        fi
        echo "[*] Ensuring debug binary without AVX-512 is built for Valgrind..."
        run_sandbox bash -c "test -f bin/nevaarize-debug || make debug"
        echo "[*] Running Valgrind leak check on: $2"
        run_sandbox valgrind \
            --leak-check=full \
            --show-leak-kinds=all \
            --track-origins=yes \
            ./bin/nevaarize-debug "$2"
        ;;

    shell)
        echo "[*] Launching interactive shell inside resource-constrained container..."
        run_sandbox bash
        ;;

    stats)
        echo "[*] Monitoring running Docker container resource stats..."
        docker stats
        ;;

    help|--help|-h)
        echo "Usage: ./test.sh [command] [args]"
        echo ""
        echo "Commands:"
        echo "  all                  Clean build and run smoke tests (default)"
        echo "  build                Compile release binary inside container"
        echo "  build-asan           Compile binary with AddressSanitizer/LeakSanitizer"
        echo "  run <script.nva>     Execute script inside container"
        echo "  asan <script.nva>    Execute script under AddressSanitizer & LeakSanitizer"
        echo "  valgrind <script>    Run Valgrind memcheck on script"
        echo "  shell                Spawn interactive bash shell inside container"
        echo "  stats                Stream live container CPU/RAM stats"
        echo "  help                 Display this help reference"
        ;;

    *)
        if [ -f "$MODE" ]; then
            echo "[*] Executing script: $MODE"
            run_sandbox ./bin/nevaarize "$MODE"
        else
            echo "[x] Unknown command or file: $MODE"
            echo "Run './test.sh help' for usage."
            exit 1
        fi
        ;;
esac
