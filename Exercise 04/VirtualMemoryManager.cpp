//
// Created by noams on 7/13/2024.
//

#include "VirtualMemoryManager.h"

/**
 * VirtualMemoryManager constructor
 */
VirtualMemoryManager::VirtualMemoryManager() {}

/**
 * Initialize the virtual memory manager
 */
void VirtualMemoryManager::initialize() {
    clearFrame(0);
}

/**
 * Read a value from the virtual memory
 * @param virtualAddress the virtual address to read from
 * @param value the value to read into
 * @return 1 if the read was successful, 0 otherwise
 */
word_t VirtualMemoryManager::read(uint64_t virtualAddress, word_t *value) {
    if (value == nullptr) { return FAILURE; }

    word_t address = manageMemory(virtualAddress, TABLES_DEPTH);
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
word_t VirtualMemoryManager::write(uint64_t virtualAddress, word_t value) {
    word_t address = manageMemory(virtualAddress, TABLES_DEPTH);
    if (address == INVALID) { return FAILURE; }

    uint64_t offset = calculateInnerOffset(virtualAddress);
    writeFrame(address, offset, value);
    return SUCCESS;
}

/**
 * Manage the memory for a given virtual address
 * @param virtualAddress the virtual address to manage
 * @param depth the depth of the tables
 * @return the address of the frame
 */
word_t VirtualMemoryManager::manageMemory(uint64_t virtualAddress, unsigned int depth) {
    if ((virtualAddress >> VIRTUAL_ADDRESS_WIDTH) > 0) { return INVALID; }

    word_t parentFrames[TABLES_DEPTH] = {0};
    word_t address = 0;

    traversePageTable(virtualAddress, depth, address, parentFrames);

    return address;
}

/**
 * Travelling on the tree of the virtual memory
 * @param virtualAddress the virtual address to traverse
 * @param depth the depth of the tables
 * @param address the address of the frame
 * @param parentFrames the parentFrames of the frame
 */
void VirtualMemoryManager::traversePageTable(uint64_t virtualAddress, unsigned int depth, word_t &address,
                                             word_t parentFrames[TABLES_DEPTH]) {
    uint64_t currentPage = virtualAddress >> OFFSET_WIDTH;
    int level = 0;

    while (level < depth) {
        uint64_t pageIndex = extractPageIndex(virtualAddress, depth, level);
        uint64_t innerOffset = calculateInnerOffset(pageIndex);

        word_t parentAddr = address;
        address = readFrame(address, innerOffset);

        if (address == 0) {
            address = handlePageFault(parentAddr, innerOffset, currentPage, level, parentFrames);
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
 * @param depth the depth of the tables
 * @param parentFrames the parentFrames of the frame
 * @return the address of the frame
 */
word_t VirtualMemoryManager::handlePageFault(word_t prevAddress, uint64_t innerOffset, uint64_t currPage,
                                             unsigned int depth, word_t parentFrames[TABLES_DEPTH]) {
    word_t frame = findFrameToUse(currPage, parentFrames);

    if (frame == INVALID) { return INVALID; }

    writeFrame(prevAddress, innerOffset, frame);

    if (depth < TABLES_DEPTH - 1) {
        clearFrame(frame);
        return frame;
    } else if (depth == TABLES_DEPTH - 1) {
        PMrestore(frame, currPage);
        return frame;
    }
    return frame;
}

/**
 * Find a frame to use in the virtual memory
 * @param page the page that was swapped in
 * @param parentFrames the ancestors of the frame
 * @return the address of the frame
 */
word_t VirtualMemoryManager::findFrameToUse(uint64_t page, const word_t parentFrames[TABLES_DEPTH]) {
    word_t frame = findAvailableFrame(parentFrames);
    if (frame == INVALID) {
        frame = findUnusedFrame();
        if (frame == INVALID) {
            word_t currPage = evictFrameWithMaxCyclicalDistance(page, parentFrames);
            frame = resolveFrameAddress(currPage);
            PMevict(frame, currPage);

            word_t address = manageMemory(currPage, TABLES_DEPTH - 1);
            uint64_t offset = calculateInnerOffset(currPage);
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
word_t VirtualMemoryManager::findAvailableFrame(const word_t parentFrames[TABLES_DEPTH]) {
    word_t frame = 1;  // Start with frame 1
    while (frame < NUM_FRAMES) {
        if (!didComeFromTheSamePath(frame, parentFrames) && frameIsEmpty(frame) && !isFrameInUse(frame)) {
            removeParentReference(frame);
            return frame;
        }
        ++frame;
    }
    return INVALID;
}

/**
 * Evict a frame with maximal cyclical distance
 * @param page the page that was swapped in
 * @param parentFrames the ancestors of the frame
 * @return the address of the frame
 */
word_t VirtualMemoryManager::evictFrameWithMaxCyclicalDistance(uint64_t page, const word_t
parentFrames[TABLES_DEPTH]) {
    word_t bestP;
    getMaxCyclicalDistance(0, 0, parentFrames, page, 0, &bestP);
    return bestP;
}

/**
 * Get the frame by a given page.
 * @param page The page to get the frame for.
 * @return The address of the frame.
 */
word_t VirtualMemoryManager::resolveFrameAddress(word_t page) { // TODO - new instead of getFrameByPage
    word_t frameAddress = 0;

    // Traverse the page table hierarchy to locate the frame
    for (int level = 0; level < TABLES_DEPTH; ++level) {
        frameAddress = fetchNextFrameAddress(frameAddress, page, level);
    }
    return frameAddress;
}

/**
 * Fetch the next frame address in the page table hierarchy.
 * @param currentAddress The current address in the hierarchy.
 * @param page The page number being translated.
 * @param level The current depth level in the hierarchy.
 * @return The address of the next frame.
 */
word_t VirtualMemoryManager::fetchNextFrameAddress(word_t currentAddress, word_t page, unsigned int level) {
    // Compute the page index and offset at the current depth
    uint64_t pageIndex = extractPageIndex(page, TABLES_DEPTH - 1, level);
    uint64_t offset = calculateInnerOffset(pageIndex);

    // Read the next frame address from physical memory
    word_t nextAddress = readFrame(currentAddress, offset);

    return nextAddress;
}

/**
 * Check if a frame is completely empty.
 * @param frame The frame to check.
 * @return true if the frame is empty, false otherwise.
 */
bool VirtualMemoryManager::frameIsEmpty(word_t frame) {
    for (int offset = 0; offset < PAGE_SIZE; ++offset) {
        if (readFrame(frame, offset) != 0) { return false; }
    }
    return true;
}

/**
 * Check if a frame is currently in use.
 * @param frame The frame to check.
 * @return true if the frame is in use, false otherwise.
 */
bool VirtualMemoryManager::isFrameInUse(word_t frame) {
    return isLeafFrame(0, 0, frame);
}

/**
 * Remove a reference to the parent frame.
 * @param frame The frame to remove the reference from.
 */
void VirtualMemoryManager::removeParentReference(word_t frame) {
    removeFrameConnection(0, 0, frame);
}

/**
 * Find an unused frame in the virtual memory
 * @return the address of the frame
 */
word_t VirtualMemoryManager::findUnusedFrame() {
    word_t nextAvailableFrame = getMaxFrameValue(0, 0) + 1;
    if (nextAvailableFrame < NUM_FRAMES) { return nextAvailableFrame; }
    else { return INVALID; }
}

/**
 * Find the maximum frame value seen from the given frame.
 * @param startFrame The frame to start from.
 * @param currentDepth The current depth of the tables.
 * @return The maximum frame value seen.
 */
word_t VirtualMemoryManager::getMaxFrameValue(word_t startFrame, unsigned int currentDepth) { // TODO - new
    if (currentDepth >= TABLES_DEPTH) { return FAILURE; }

    word_t maxFrameValue = 0;
    findMaxFrameRecursive(startFrame, currentDepth, maxFrameValue);
    return maxFrameValue;
}

/**
 * Helper function to recursively find the maximum frame value.
 * @param frame The current frame to inspect.
 * @param depth The current depth of the tables.
 * @param maxFrameValue Reference to the current maximum frame value.
 */
void VirtualMemoryManager::findMaxFrameRecursive(word_t frame, unsigned int depth, word_t &maxFrameValue) {
    if (depth >= TABLES_DEPTH) {
        return;
    }

    for (int offset = 0; offset < PAGE_SIZE; ++offset) {
        word_t value = readFrame(frame, offset);
        if (value > maxFrameValue) {
            maxFrameValue = value;
        }
        findMaxFrameRecursive(value, depth + 1, maxFrameValue);
    }
}

/**
 * Calculate the maximum cyclical distance.
 * @param startFrame The frame to start from.
 * @param currentDepth The current depth of the tables.
 * @param parentFrames The parents of the frame.
 * @param page The page that was swapped in.
 * @param currentPage The current page.
 * @param optimalPage The best page.
 * @return The maximum cyclical distance.
 */
word_t VirtualMemoryManager::getMaxCyclicalDistance(word_t startFrame, unsigned int currentDepth,
                                                    const word_t parentFrames[TABLES_DEPTH],
                                                    uint64_t page, word_t currentPage,
                                                    word_t *optimalPage) {
    if (currentDepth >= TABLES_DEPTH) {
        return computeDistance(currentPage, parentFrames, page, optimalPage);
    }

    word_t maxDistance = 0;
    int offset = 0;
    while (offset < PAGE_SIZE) {
        word_t nextFrame = readFrame(startFrame, offset);
        if (nextFrame != 0) {
            word_t newPage = (currentPage << OFFSET_WIDTH) | offset;
            word_t tempOptimalPage;
            word_t distance = getMaxCyclicalDistance(nextFrame, currentDepth + 1, parentFrames,
                                                     page, newPage, &tempOptimalPage);
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
 * Compute the cyclical distance for a given page.
 * @param currentPage The current page.
 * @param parentFrames The parents of the frame.
 * @param swappedPage The page that was swapped in.
 * @param optimalPage The best page.
 * @return The cyclical distance.
 */
word_t VirtualMemoryManager::computeDistance(word_t currentPage, const word_t parentFrames[TABLES_DEPTH],
                                             word_t swappedPage, word_t *optimalPage) {
    if (didComeFromTheSamePath(resolveFrameAddress(currentPage), parentFrames)) { return FAILURE; }
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
word_t VirtualMemoryManager::readFrame(word_t address, uint64_t offset) {
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
void VirtualMemoryManager::writeFrame(word_t address, uint64_t offset, word_t value) {
    PMwrite(address * PAGE_SIZE + offset, value);
}

/**
 * Extract the index for the current level from the page number.
 * @param page the page number.
 * @param depth the depth of the tables.
 * @param level the current level in the page table hierarchy.
 * @return the index for the current level.
 */
uint64_t VirtualMemoryManager::extractPageIndex(uint64_t page, unsigned int depth, unsigned int level) {
    int shiftAmount = OFFSET_WIDTH * (depth - level);
    return page >> shiftAmount;
}

/**
 * Calculate the offset within the page table entry.
 * @param pageIndex the page index for the current level.
 * @return the inner offset within the page table entry.
 */
uint64_t VirtualMemoryManager::calculateInnerOffset(uint64_t pageIndex) {
    return pageIndex & (PAGE_SIZE - 1);
}

/**
 * Check if a frame came from the same path
 * @param frame the frame to check
 * @param parentFrames the parentFrames of the frame
 * @return true if the frame came from the same path, false otherwise
 */
bool VirtualMemoryManager::didComeFromTheSamePath(word_t frame, const word_t parentFrames[TABLES_DEPTH]) {
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
void VirtualMemoryManager::clearFrame(word_t frame) {
    for (int i = 0; i < PAGE_SIZE; ++i) { writeFrame(frame, i, 0); }
}

/**
 * Remove the connection between two frames.
 * @param startFrame  The frame to start from.
 * @param currentDepth  The current depth of the tables.
 * @param targetFrame  The frame to remove the connection to.
 * @return noting - void.
 */
void VirtualMemoryManager::removeFrameConnection(word_t startFrame, unsigned int currentDepth, word_t
targetFrame) {
    if (currentDepth >= TABLES_DEPTH) { return; }

    int offset = 0;
    while (offset < PAGE_SIZE) {
        word_t linkedFrame = readFrame(startFrame, offset);
        if (linkedFrame != 0) {
            if (linkedFrame == targetFrame) { writeFrame(startFrame, offset, 0); }
            else { removeFrameConnection(linkedFrame, currentDepth + 1, targetFrame); }
        }
        ++offset;
    }
}

/**
 * Determine if the target frame is a leaf node.
 * @param startFrame The frame to start from.
 * @param currentDepth The current depth of the tables.
 * @param targetFrame The frame to check.
 * @return true if the target frame is a leaf node, false otherwise.
 */
bool VirtualMemoryManager::isLeafFrame(word_t startFrame, unsigned int currentDepth, word_t targetFrame) {
    if (currentDepth >= TABLES_DEPTH) { return false; }

    int offset = 0;
    while (offset < PAGE_SIZE) {
        word_t nextFrame = readFrame(startFrame, offset);
        if (nextFrame != 0) {
            if (nextFrame == targetFrame) { return currentDepth == TABLES_DEPTH - 1; }
            if (isLeafFrame(nextFrame, currentDepth + 1, targetFrame)) { return true; }
        }
        ++offset;
    }
    return false;
}
