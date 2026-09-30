/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * GC.hpp - Nevaarize Garbage Collector
 *
 * Generational garbage collector with bump-pointer allocation,
 * root set tracking, and mark-sweep collection.
 */

#ifndef NEVAARIZE_GC_HPP
#define NEVAARIZE_GC_HPP

#include <cstdint>
#include <cstddef>
#include <vector>
#include <memory>
#include <cstring>
#include <algorithm>
#include <csetjmp>
#include <mutex>

namespace nevaarize {

/**
 * Memory region for allocation.
 */
class MemoryRegion {
public:
    explicit MemoryRegion(size_t size)
        : data(new uint8_t[size])
        , capacity(size)
        , used(0) {}

    ~MemoryRegion() {
        delete[] data;
    }

    MemoryRegion(const MemoryRegion&) = delete;
    MemoryRegion& operator=(const MemoryRegion&) = delete;

    MemoryRegion(MemoryRegion&& other) noexcept
        : data(other.data)
        , capacity(other.capacity)
        , used(other.used) {
        other.data = nullptr;
        other.capacity = 0;
        other.used = 0;
    }

    void* allocate(size_t size, size_t alignment = 8) {
        size_t aligned = (used + alignment - 1) & ~(alignment - 1);
        if (aligned + size > capacity) {
            return nullptr;
        }
        void* ptr = data + aligned;
        used = aligned + size;
        return ptr;
    }

    /**
     * Attempts to expand the most recently allocated block in place.
     * Returns true if successful, false otherwise.
     */
    bool expand(void* ptr, size_t oldSize, size_t newSize, size_t alignment = 8) {
        size_t oldAlignedSearch = ((used - oldSize) + alignment - 1) & ~(alignment - 1);
        if (data + oldAlignedSearch == ptr) {
            size_t newUsed = oldAlignedSearch + newSize;
            if (newUsed <= capacity) {
                used = newUsed;
                return true;
            }
        }
        return false;
    }

    void reset() {
        used = 0;
    }

    size_t getUsed() const { return used; }
    size_t getCapacity() const { return capacity; }
    uint8_t* getData() const { return data; }

    bool contains(const void* ptr) const {
        if (!data || !ptr) return false;
        const uint8_t* p = static_cast<const uint8_t*>(ptr);
        return p >= data && p < (data + used);
    }

private:
    uint8_t* data;
    size_t capacity;
    size_t used;
};

/**
 * Object header for GC tracking.
 */
struct GCHeader {
    uint32_t size;
    uint8_t type;
    uint8_t marked : 1;
    uint8_t generation : 2;
    uint8_t reserved : 5;
    GCHeader* next;   // Intrusive linked list for allocated objects
};

/**
 * Thread-safe shared old-generation memory region for cross-thread async task payloads.
 */
class SharedMemoryRegion {
public:
    static SharedMemoryRegion& instance() {
        static SharedMemoryRegion shared(64 * 1024 * 1024);
        return shared;
    }

    explicit SharedMemoryRegion(size_t cap)
        : data(static_cast<uint8_t*>(malloc(cap)))
        , capacity(cap)
        , used(0) {}

    ~SharedMemoryRegion() {
        std::lock_guard<std::mutex> lock(mutex);
        if (data) free(data);
        for (void* p : overflow) {
            free(p);
        }
        overflow.clear();
    }

    void* allocate(size_t bytes, size_t alignment = 8) {
        std::lock_guard<std::mutex> lock(mutex);
        size_t alignedUsed = (used + alignment - 1) & ~(alignment - 1);
        if (alignedUsed + bytes <= capacity) {
            void* ptr = data + alignedUsed;
            used = alignedUsed + bytes;
            return ptr;
        }
        void* ptr = malloc(bytes);
        if (ptr) {
            overflow.push_back(ptr);
        }
        return ptr;
    }

    bool contains(const void* ptr) const {
        if (!ptr) return false;
        std::lock_guard<std::mutex> lock(mutex);
        if (data && ptr >= data && ptr < (data + used)) return true;
        for (void* p : overflow) {
            if (p == ptr) return true;
        }
        return false;
    }

private:
    uint8_t* data;
    size_t capacity;
    size_t used;
    mutable std::mutex mutex;
    std::vector<void*> overflow;
};

/**
 * Allocate shared memory for cross-thread objects with an immortal GCHeader.
 */
inline void* allocateShared(size_t size) {
    size_t total = sizeof(GCHeader) + size;
    void* mem = SharedMemoryRegion::instance().allocate(total);
    if (!mem) return nullptr;
    GCHeader* hdr = static_cast<GCHeader*>(mem);
    hdr->size = static_cast<uint32_t>(size);
    hdr->type = 0;
    hdr->marked = 1;      // Permanently marked so thread-local GC never sweeps
    hdr->generation = 2;  // Old/shared generation
    hdr->reserved = 0;
    hdr->next = nullptr;
    return static_cast<uint8_t*>(mem) + sizeof(GCHeader);
}

/**
 * Generational garbage collector with mark-sweep.
 */
class GarbageCollector {
public:
    static constexpr size_t YOUNG_SIZE = 1024 * 1024;
    static constexpr size_t OLD_SIZE = 16 * 1024 * 1024;

    GarbageCollector()
        : youngGen(YOUNG_SIZE)
        , oldGen(OLD_SIZE)
        , totalAllocated(0)
        , allocsSinceCollect(0)
        , collectCount(0)
        , objectList(nullptr)
        , stackTop(nullptr)
        , gcInhibitCount(0) {}

    void setStackTop(void* top) { stackTop = top; }
    void* getStackTop() const { return stackTop; }

    void inhibitGC() { gcInhibitCount++; }
    void resumeGC() { if (gcInhibitCount > 0) gcInhibitCount--; }
    bool isGCLocked() const { return gcInhibitCount > 0; }

    /**
     * Allocate memory from young generation or fallback to old generation/overflow.
     */
    void* allocate(size_t size) {
        size_t totalSize = sizeof(GCHeader) + size;

        void* ptr = youngGen.allocate(totalSize);
        if (!ptr) {
            collectYoung();
            ptr = youngGen.allocate(totalSize);
            if (!ptr) {
                ptr = oldGen.allocate(totalSize);
                if (!ptr) {
                    collectFull();
                    ptr = oldGen.allocate(totalSize);
                    if (!ptr) {
                        if (!overflowRegions.empty()) {
                            ptr = overflowRegions.back()->allocate(totalSize);
                        }
                        if (!ptr) {
                            size_t overflowSize = totalSize > YOUNG_SIZE ? totalSize * 2 : YOUNG_SIZE;
                            overflowRegions.push_back(std::make_unique<MemoryRegion>(overflowSize));
                            ptr = overflowRegions.back()->allocate(totalSize);
                            if (!ptr) {
                                return nullptr;
                            }
                        }
                    }
                }
            }
        }

        GCHeader* header = static_cast<GCHeader*>(ptr);
        header->size = static_cast<uint32_t>(size);
        header->type = 0;
        header->marked = 0;
        header->generation = 0;
        header->next = objectList;
        objectList = header;

        totalAllocated += size;
        allocsSinceCollect++;

        // Adaptive collection: trigger when young generation is 80% full and not inhibited
        if (!isGCLocked() && youngGen.getUsed() >= (youngGen.getCapacity() * 4) / 5) {
            collectYoung();
        }

        return static_cast<uint8_t*>(ptr) + sizeof(GCHeader);
    }

    /**
     * Expand an existing allocation.
     * Returns true if expanded in-place, false if reallocation is needed.
     */
    bool expand(void* ptr, size_t newSize) {
        if (!ptr) return false;

        GCHeader* header = reinterpret_cast<GCHeader*>(static_cast<uint8_t*>(ptr) - sizeof(GCHeader));
        size_t oldTotal = sizeof(GCHeader) + header->size;
        size_t newTotal = sizeof(GCHeader) + newSize;

        if (youngGen.expand(header, oldTotal, newTotal)) {
            totalAllocated += (newSize - header->size);
            header->size = static_cast<uint32_t>(newSize);
            return true;
        }
        return false;
    }

    /**
     * Register a root pointer for GC scanning.
     */
    void addRoot(void** rootPtr) {
        roots.push_back(rootPtr);
    }

    /**
     * Remove a root pointer from GC scanning.
     */
    void removeRoot(void** rootPtr) {
        roots.erase(
            std::remove(roots.begin(), roots.end(), rootPtr),
            roots.end()
        );
    }

    /**
     * Mark phase: trace from roots and mark reachable objects.
     */
    void markFromRoots() {
        for (void** root : roots) {
            if (*root) {
                scanCandidatePointer(*root);
            }
        }
    }

    /**
     * Conservative stack scanning across active JIT execution stack.
     * Inspects active stack memory from current RSP to stackTop and marks live objects.
     */
    void scanStackRoots() {
        if (!stackTop) return;

        jmp_buf cpuRegs;
        setjmp(cpuRegs);

        void* currentSp = nullptr;
#if defined(__x86_64__) || defined(_M_X64)
        asm volatile("mov %%rsp, %0" : "=r"(currentSp));
#else
        currentSp = __builtin_frame_address(0);
#endif

        if (!currentSp) return;

        uintptr_t low = reinterpret_cast<uintptr_t>(currentSp);
        uintptr_t high = reinterpret_cast<uintptr_t>(stackTop);
        if (low > high) {
            std::swap(low, high);
        }

        if (high - low > 2 * 1024 * 1024) {
            low = high - (2 * 1024 * 1024);
        }

        low &= ~static_cast<uintptr_t>(sizeof(void*) - 1);

        for (uintptr_t p = low; p < high; p += sizeof(void*)) {
            void* candidate = *reinterpret_cast<void**>(p);
            if (!candidate) continue;

            scanCandidatePointer(candidate);
        }
    }

    /**
     * Evaluates a candidate pointer against GC memory regions and marks enclosing object.
     */
    void scanCandidatePointer(void* candidate) {
        if (!candidate) return;
        uintptr_t uaddr = reinterpret_cast<uintptr_t>(candidate);
        if (uaddr < 0x1000 || (uaddr & 0x7) != 0) return;

        bool inGC = youngGen.contains(candidate) || oldGen.contains(candidate);
        if (!inGC) {
            for (const auto& reg : overflowRegions) {
                if (reg->contains(candidate)) {
                    inGC = true;
                    break;
                }
            }
        }
        if (!inGC) return;

        const uint8_t* p = static_cast<const uint8_t*>(candidate);
        for (GCHeader* cur = objectList; cur != nullptr; cur = cur->next) {
            if (cur->marked) continue;
            const uint8_t* start = reinterpret_cast<const uint8_t*>(cur);
            const uint8_t* end = start + sizeof(GCHeader) + cur->size;
            if (p >= start && p < end) {
                markObject(cur);
                break;
            }
        }
    }

    /**
     * Mark an object and transitively trace interior GC pointers.
     */
    void markObject(GCHeader* header) {
        if (!header || header->marked) return;
        header->marked = 1;

        uint8_t* payload = reinterpret_cast<uint8_t*>(header) + sizeof(GCHeader);
        size_t words = header->size / sizeof(void*);
        void** slots = reinterpret_cast<void**>(payload);
        for (size_t i = 0; i < words; ++i) {
            void* slotVal = slots[i];
            if (slotVal) {
                scanCandidatePointer(slotVal);
            }
        }
    }

    /**
     * Sweep phase: unmark live objects, reclaim dead blocks.
     */
    size_t sweep(bool isFull = false) {
        size_t freedBytes = 0;
        GCHeader** prev = &objectList;
        GCHeader* current = objectList;
        bool hasLiveInYoung = false;
        bool hasLiveInOld = false;

        while (current) {
            if (current->marked) {
                current->marked = 0;
                if (youngGen.contains(current)) {
                    hasLiveInYoung = true;
                } else if (oldGen.contains(current)) {
                    hasLiveInOld = true;
                }
                prev = &current->next;
                current = current->next;
            } else {
                freedBytes += current->size;
                *prev = current->next;
                current = current->next;
            }
        }

        if (!hasLiveInYoung) {
            youngGen.reset();
        }
        if (isFull && !hasLiveInOld) {
            oldGen.reset();
        }
        return freedBytes;
    }

    /**
     * Collect young generation with conservative stack mark-sweep.
     */
    void collectYoung() {
        if (isGCLocked()) return;
        collectCount++;
        markFromRoots();
        scanStackRoots();
        sweep(false);
        allocsSinceCollect = 0;
    }

    /**
     * Full garbage collection across young and old generations.
     */
    void collectFull() {
        if (isGCLocked()) return;
        collectCount++;
        markFromRoots();
        scanStackRoots();
        sweep(true);
        allocsSinceCollect = 0;
    }

    size_t getTotalAllocated() const { return totalAllocated; }
    size_t getCollectCount() const { return collectCount; }
    size_t getRootCount() const { return roots.size(); }

private:
    MemoryRegion youngGen;
    MemoryRegion oldGen;
    std::vector<std::unique_ptr<MemoryRegion>> overflowRegions;
    size_t totalAllocated;
    size_t allocsSinceCollect;
    size_t collectCount;
    GCHeader* objectList;
    std::vector<void**> roots;
    void* stackTop;
    size_t gcInhibitCount;
};

/**
 * RAII guard to inhibit garbage collection during critical native expression evaluations.
 */
struct ScopedGCLock {
    GarbageCollector& gc;
    explicit ScopedGCLock(GarbageCollector& collector) : gc(collector) {
        gc.inhibitGC();
    }
    ~ScopedGCLock() {
        gc.resumeGC();
    }
};

} // namespace nevaarize

#endif // NEVAARIZE_GC_HPP
