/**
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
 * Generational garbage collector with mark-sweep.
 */
class GarbageCollector {
public:
    static constexpr size_t YOUNG_SIZE = 1024 * 1024;
    static constexpr size_t OLD_SIZE = 16 * 1024 * 1024;
    static constexpr size_t COLLECT_THRESHOLD = 512;

    GarbageCollector()
        : youngGen(YOUNG_SIZE)
        , oldGen(OLD_SIZE)
        , totalAllocated(0)
        , allocsSinceCollect(0)
        , collectCount(0)
        , objectList(nullptr) {}

    /**
     * Allocate memory from young generation.
     */
    void* allocate(size_t size) {
        size_t totalSize = sizeof(GCHeader) + size;

        void* ptr = youngGen.allocate(totalSize);
        if (!ptr) {
            collectYoung();
            ptr = youngGen.allocate(totalSize);
            if (!ptr) {
                collectFull();
                ptr = youngGen.allocate(totalSize);
                if (!ptr) {
                    return nullptr;
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

        // Adaptive collection: trigger when allocation count exceeds threshold
        if (allocsSinceCollect >= COLLECT_THRESHOLD) {
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
     * Root pointers are locations that reference GC-managed objects.
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
                GCHeader* header = reinterpret_cast<GCHeader*>(
                    static_cast<uint8_t*>(*root) - sizeof(GCHeader));
                markObject(header);
            }
        }
    }

    /**
     * Sweep phase: unmark live objects, count dead bytes.
     */
    size_t sweep() {
        size_t freedBytes = 0;
        GCHeader* current = objectList;

        while (current) {
            if (current->marked) {
                current->marked = 0;
            } else {
                freedBytes += current->size;
            }
            current = current->next;
        }

        return freedBytes;
    }

    /**
     * Collect young generation with mark-sweep.
     */
    void collectYoung() {
        collectCount++;
        markFromRoots();
        sweep();
        youngGen.reset();
        objectList = nullptr;
        allocsSinceCollect = 0;
    }

    /**
     * Full garbage collection.
     */
    void collectFull() {
        collectCount++;
        markFromRoots();
        sweep();
        youngGen.reset();
        objectList = nullptr;
        allocsSinceCollect = 0;
    }

    size_t getTotalAllocated() const { return totalAllocated; }
    size_t getCollectCount() const { return collectCount; }
    size_t getRootCount() const { return roots.size(); }

private:
    MemoryRegion youngGen;
    MemoryRegion oldGen;
    size_t totalAllocated;
    size_t allocsSinceCollect;
    size_t collectCount;
    GCHeader* objectList;
    std::vector<void**> roots;

    /**
     * Mark a single object as reachable.
     */
    void markObject(GCHeader* header) {
        if (!header || header->marked) return;
        header->marked = 1;
    }
};

} // namespace nevaarize

#endif // NEVAARIZE_GC_HPP
