//
// Created by noams on 7/23/2024.
//

#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <cmath>

#define SUCCESS 1
#define FAILURE 0
#define INVALID (-1)
#define ROOT_FRAME 0

class FrameManager {
public:
    static word_t findAvailableFrame(const word_t parentFrames[TABLES_DEPTH]);
    static word_t findUnusedFrame();
    static word_t findFrameToUse(const word_t parentFrames[TABLES_DEPTH], uint64_t page);
    static word_t getMaxFrameValue(word_t frame, unsigned int depth);
    static word_t getMaxCyclicalDistance(const word_t parentFrames[TABLES_DEPTH], word_t startFrame, unsigned int currentDepth, uint64_t page, word_t currentPage, word_t *optimalPage);
    static bool isFrameInUseRecEng(word_t startFrame, word_t targetFrame, unsigned int currentDepth);
    static void removeFrameConnection(word_t startFrame, word_t targetFrame, unsigned int currentDepth);
    static void clearFrame(word_t frame);
    static bool checkIfFrameIsEmpty(word_t frame);
    static bool didComeFromTheSamePath(const word_t parentFrames[TABLES_DEPTH], word_t frame);
    static bool isLeaf(unsigned int currentDepth);

private:
    static void findMaxFrameValue(word_t frame, word_t &maxFrameValue, unsigned int depth);
};

class PageTableManager {
public:
    static word_t manageMemory(uint64_t virtualAddress, unsigned int depth);
    static void traversePageTable(uint64_t virtualAddress, word_t &address, word_t parentFrames[TABLES_DEPTH], unsigned int depth);
    static word_t handlePageFault(word_t prevAddress, uint64_t innerOffset, uint64_t currPage, word_t parentFrames[TABLES_DEPTH], unsigned int depth);
};

// Utility functions
static word_t readFrame(word_t address, uint64_t offset);
static void writeFrame(word_t address, uint64_t offset, word_t value);
static uint64_t extractPageIndex(uint64_t page, unsigned int depth, unsigned int level);
static uint64_t calculateInnerOffset(uint64_t pageIndex);
static word_t resolveFrameAddress(word_t page);
static word_t fetchNextFrameAddress(word_t currentAddress, unsigned int level, word_t page);
static word_t computeDistance(const word_t parentFrames[TABLES_DEPTH], word_t currentPage, word_t swappedPage, word_t *optimalPage);


// -------------------------- Virtual Memory Functions ----------------------------- //

/**
 * Initialize the virtual memory
 */
void VMinitialize() {
    FrameManager::clearFrame(ROOT_FRAME);
}

/**
 * Read a value from the virtual memory
 * @param virtualAddress the virtual address to read from
 * @param value the value to read into
 * @return 1 if the read was successful, 0 otherwise
 */
word_t VMread(uint64_t virtualAddress, word_t *value) {
    if (value == nullptr) { return FAILURE; }

    word_t address = PageTableManager::manageMemory(virtualAddress, TABLES_DEPTH);
    if (address == INVALID) { return FAILURE; }

    uint64_t offset = calculateInnerOffset(virtualAddress);
    *value = readFrame(address, offset);
    return SUCCESS;
}

/**
 * Write a value to the virtual memory
 * @param virtualAddress the virtual address to write to
 * @param value the value to write
 * @return 1 if the write was successful, 0 otherwise
 */
word_t VMwrite(uint64_t virtualAddress, word_t value) {
    word_t address = PageTableManager::manageMemory(virtualAddress, TABLES_DEPTH);
    if (address == INVALID) { return FAILURE; }

    uint64_t offset = calculateInnerOffset(virtualAddress);
    writeFrame(address, offset, value);
    return SUCCESS;
}

// -------------------------- Page Table Manager ----------------------------- //

/**
 * Manage the memory for a given virtual address
 * @param virtualAddress the virtual address to manage
 * @param depth the depth of the tables
 * @return the address of the frame
 */
word_t PageTableManager::manageMemory(uint64_t virtualAddress, unsigned int depth) {
    if ((virtualAddress >> VIRTUAL_ADDRESS_WIDTH) > 0) { return INVALID; }

    word_t parentFrames[TABLES_DEPTH] = {0};
    word_t address = 0;

    traversePageTable(virtualAddress, address, parentFrames, depth);

    return address;
}

/**
 * Travelling on the tree of the virtual memory
 * @param virtualAddress the virtual address to traverse
 * @param address the address of the frame
 * @param parentFrames the parentFrames of the frame
 * @param depth the depth of the tables
 */
void PageTableManager::traversePageTable(uint64_t virtualAddress, word_t &address, word_t parentFrames[TABLES_DEPTH], unsigned int depth) {
    uint64_t currentPage = virtualAddress >> OFFSET_WIDTH;

    unsigned int level = 0;

    while (level < depth) {
        uint64_t pageIndex = extractPageIndex(virtualAddress, depth, level);
        uint64_t innerOffset = calculateInnerOffset(pageIndex);

        word_t parentAddr = address;
        address = readFrame(address, innerOffset);

        if (address == 0) {
            address = handlePageFault(parentAddr, innerOffset, currentPage, parentFrames, level);
        }
        parentFrames[level] = address;
        ++level;
    }
}

/**
 * Handle a page fault in the virtual memory
 * @param prevAddress the previous address
 * @param innerOffset the inner offset
 * @param currPage the current page
 * @param parentFrames the parentFrames of the frame
 * @param depth the depth of the tables
 * @return the address of the frame
 */
word_t PageTableManager::handlePageFault(word_t prevAddress, uint64_t innerOffset, uint64_t currPage, word_t parentFrames[TABLES_DEPTH], unsigned int depth) {
    word_t frame = FrameManager::findFrameToUse(parentFrames, currPage);

    if (frame == INVALID) { return INVALID; }

    writeFrame(prevAddress, innerOffset, frame);

    if (depth < TABLES_DEPTH - 1) {
        FrameManager::clearFrame(frame);
        return frame;
    } else if (FrameManager::isLeaf(depth)) {
        PMrestore(frame, currPage);
        return frame;
    }
    return frame;
}

// -------------------------- Frame Manager ----------------------------- //

/**
 * Find a frame to use in the virtual memory
 * @param parentFrames the ancestors of the frame
 * @param page the page that was swapped in
 * @return the address of the frame
 */
word_t FrameManager::findFrameToUse(const word_t parentFrames[TABLES_DEPTH], uint64_t page) {
    word_t frame = findAvailableFrame(parentFrames);
    if (frame == INVALID) {
        frame = findUnusedFrame();
        if (frame == INVALID) {
            word_t bestP;
            getMaxCyclicalDistance(parentFrames, ROOT_FRAME, 0, page, 0, &bestP);

            frame = resolveFrameAddress(bestP);
            PMevict(frame, bestP);

            word_t address = PageTableManager::manageMemory(bestP, TABLES_DEPTH - 1);
            uint64_t offset = calculateInnerOffset(bestP);
            writeFrame(address, offset, 0);
        }
    }
    return frame;
}

/**
 * Find an available frame in the virtual memory.
 * @param parentFrames The parent frames of the frame.
 * @return The address of the available frame or INVALID if not found.
 */
word_t FrameManager::findAvailableFrame(const word_t parentFrames[TABLES_DEPTH]) {
    word_t frame = ROOT_FRAME;  // Start with the root node
    unsigned int startDepth = 0;
    while (frame < NUM_FRAMES) {
        if (!didComeFromTheSamePath(parentFrames, frame) && checkIfFrameIsEmpty(frame) &&
            !isFrameInUseRecEng(ROOT_FRAME, frame, startDepth)) {
            removeFrameConnection(ROOT_FRAME, frame, startDepth);
            return frame;
        }
        ++frame;
    }
    return INVALID;
}

/**
 * Check if a frame is completely empty.
 * @param frame The frame to check.
 * @return true if the frame is empty, false otherwise.
 */
bool FrameManager::checkIfFrameIsEmpty(word_t frame) {
    for (unsigned int entry = 0; entry < PAGE_SIZE; ++entry) {
        if (readFrame(frame, entry) != 0) { return false; }
    }
    return true;
}

/**
 * Find the next unused frame in the virtual memory.
 * @return the address of the next available frame, or INVALID if no frames are available.
 */
word_t FrameManager::findUnusedFrame() {
    // Find the maximum frame value in use starting from the root frame
    word_t maxFrameValue = getMaxFrameValue(ROOT_FRAME, 0);

    // Calculate the next available frame
    word_t nextAvailableFrame = maxFrameValue + 1;

    // Check if the next available frame is within the allowed range of frames
    if (nextAvailableFrame < NUM_FRAMES) { return nextAvailableFrame; }
    else { return INVALID; }
}

/**
 * Find the maximum frame value seen from the given frame.
 * @param startFrame The frame to start from.
 * @param currentDepth The current depth of the tables.
 * @return The maximum frame value seen.
 */
word_t FrameManager::getMaxFrameValue(word_t startFrame, unsigned int currentDepth) {
    if (currentDepth >= TABLES_DEPTH) { return FAILURE; } // Base case

    word_t maxFrameValue = 0;
    findMaxFrameValue(startFrame, maxFrameValue, currentDepth);
    return maxFrameValue;
}

/**
 * Helper function to recursively find the maximum frame value.
 * @param frame The current frame to inspect.
 * @param maxFrameValue Reference to the current maximum frame value.
 * @param depth The current depth of the tables.
 */
void FrameManager::findMaxFrameValue(word_t frame, word_t &maxFrameValue, unsigned int depth) {
    if (depth >= TABLES_DEPTH) {
        return;
    }

    for (int row = 0; row < PAGE_SIZE; ++row) {
        word_t value = readFrame(frame, row);
        if (value > maxFrameValue) {
            maxFrameValue = value;
        }
        findMaxFrameValue(value, maxFrameValue, depth + 1);
    }
}

/**
 * Calculate the maximum cyclical distance.
 * @param parentFrames The parents of the frame.
 * @param startFrame The frame to start from.
 * @param currentDepth The current depth of the tables.
 * @param page The page that was swapped in.
 * @param currentPage The current page.
 * @param optimalPage The best page.
 * @return The maximum cyclical distance.
 */
word_t FrameManager::getMaxCyclicalDistance(const word_t parentFrames[TABLES_DEPTH],
                                            word_t startFrame,
                                            unsigned int currentDepth,
                                            uint64_t page,
                                            word_t currentPage,
                                            word_t *optimalPage) {

    if (currentDepth >= TABLES_DEPTH) {
        return computeDistance(parentFrames, currentPage, page, optimalPage);
    }

    word_t maxDistance = 0;
    int offset = 0;
    while (offset < PAGE_SIZE) {
        word_t nextFrame = readFrame(startFrame, offset);
        if (nextFrame != 0) {
            word_t newPage = (currentPage << OFFSET_WIDTH) | offset;
            word_t tempOptimalPage;
            unsigned int newDepth = currentDepth + 1;
            word_t distance = getMaxCyclicalDistance(parentFrames, nextFrame, newDepth, page, newPage, &tempOptimalPage);
            if (distance > maxDistance) {
                maxDistance = distance;
                *optimalPage = tempOptimalPage;
            }
        }
        ++offset;
    }
    return maxDistance;
}

/**
 * Check if a frame came from the same path
 * @param parentFrames the parentFrames of the frame
 * @param frame the frame to check
 * @return true if the frame came from the same path, false otherwise
 */
bool FrameManager::didComeFromTheSamePath(const word_t parentFrames[TABLES_DEPTH], word_t frame) {
    int level = 0;
    while (level < TABLES_DEPTH) {
        if (frame == parentFrames[level]) { return true; }
        ++level;
    }
    return false;
}

/**
 * Reset a frame in the virtual memory
 * @param frame the frame to reset
 */
void FrameManager::clearFrame(word_t frame) {
    for (int i = 0; i < PAGE_SIZE; ++i) { writeFrame(frame, i, 0); }
}

/**
 * Remove the connection between two frames.
 * @param startFrame  The frame to start from.
 * @param targetFrame  The frame to remove the connection to.
 * @param currentDepth  The current depth of the tables.
 * @return noting - void.
 */
void FrameManager::removeFrameConnection(word_t startFrame, word_t targetFrame, unsigned int currentDepth) {
    if (currentDepth >= TABLES_DEPTH) { return; }

    int offset = 0;
    while (offset < PAGE_SIZE) {
        word_t linkedFrame = readFrame(startFrame, offset);
        if (linkedFrame != 0) {
            if (linkedFrame == targetFrame) { writeFrame(startFrame, offset, 0); }
            else { removeFrameConnection(linkedFrame, targetFrame, currentDepth + 1); }
        }
        ++offset;
    }
}

/**
 * Determine if the target frame is a leaf node.
 * @param startFrame The frame to start from.
 * @param targetFrame The frame to check.
 * @param currentDepth The current depth of the tables.
 * @return true if the target frame is a leaf node, false otherwise.
 */
bool FrameManager::isFrameInUseRecEng(word_t startFrame, word_t targetFrame, unsigned int currentDepth) {
    if (currentDepth >= TABLES_DEPTH) { return false; }
    int entry = 0;
    while (entry < PAGE_SIZE) {
        word_t nextFrame = readFrame(startFrame, entry);
        if (nextFrame != 0) {
            if (nextFrame == targetFrame) { return isLeaf(currentDepth); }
            if (isFrameInUseRecEng(nextFrame, targetFrame, currentDepth + 1)) { return true; }
        }
        ++entry;
    }
    return false;
}

/**
 * Determine if the target frame is a leaf node.
 * @param currentDepth The current depth of the tables.
 * @return true if the target frame is a leaf node, false otherwise.
 */
bool FrameManager::isLeaf(unsigned int currentDepth) {
    if (currentDepth == TABLES_DEPTH - 1) { return true; }
    else { return false; }
}

// -------------------------- Utility Functions ----------------------------- //

/**
 * Get the frame by a given page.
 * @param page The page to get the frame for.
 * @return The address of the frame.
 */
word_t resolveFrameAddress(word_t page) {
    word_t frameAddress = 0;

    // Traverse the page table hierarchy to locate the frame
    for (unsigned int level = 0; level < TABLES_DEPTH; ++level) {
        frameAddress = fetchNextFrameAddress(frameAddress, level, page);
    }
    return frameAddress;
}

/**
 * Fetch the next frame address in the page table hierarchy.
 * @param currentAddress The current address in the hierarchy.
 * @param level The current depth level in the hierarchy.
 * @param page The page number being translated.
 * @return The address of the next frame.
 */
word_t fetchNextFrameAddress(word_t currentAddress, unsigned int level, word_t page) {
    // Compute the page index and offset at the current depth
    unsigned int leafDepth = TABLES_DEPTH - 1;
    uint64_t pageIndex = extractPageIndex(page, leafDepth, level);
    uint64_t offset = calculateInnerOffset(pageIndex);

    // Read the next frame address from physical memory
    word_t nextAddress = readFrame(currentAddress, offset);

    return nextAddress;
}

/**
 * Compute the cyclical distance for a given page.
 * @param parentFrames The parents of the frame.
 * @param currentPage The current page.
 * @param swappedPage The page that was swapped in.
 * @param optimalPage The best page.
 * @return The cyclical distance.
 */
word_t computeDistance(const word_t parentFrames[TABLES_DEPTH], word_t currentPage, word_t swappedPage, word_t *optimalPage) {
    if (FrameManager::didComeFromTheSamePath(parentFrames, resolveFrameAddress(currentPage))) { return FAILURE; }
    *optimalPage = currentPage;
    int distance1 = NUM_PAGES - std::abs(swappedPage - currentPage);
    int distance2 = std::abs(swappedPage - currentPage);
    return (distance1 < distance2) ? distance1 : distance2;
}

/**
 * Read a frame from the virtual memory
 * @param address the address of the frame
 * @param offset the offset of the frame
 * @return the value of the frame
 */
word_t readFrame(word_t address, uint64_t offset) {
    word_t value;
    PMread(address * PAGE_SIZE + offset, &value);
    return value;
}

/**
 * Write a frame to the virtual memory
 * @param address the address of the frame
 * @param offset the offset of the frame
 * @param value the value of the frame
 */
void writeFrame(word_t address, uint64_t offset, word_t value) {
    PMwrite(address * PAGE_SIZE + offset, value);
}

/**
 * Extract the index for the current level from the page number.
 * @param page the page number.
 * @param depth the depth of the tables.
 * @param level the current level in the page table hierarchy.
 * @return the index for the current level.
 */
uint64_t extractPageIndex(uint64_t page, unsigned int depth, unsigned int level) {
    int shiftAmount = OFFSET_WIDTH * (depth - level);
    return page >> shiftAmount;
}

/**
 * Calculate the offset within the page table entry.
 * @param pageIndex the page index for the current level.
 * @return the inner offset within the page table entry.
 */
uint64_t calculateInnerOffset(uint64_t pageIndex) {
    return pageIndex & (PAGE_SIZE - 1);
}
