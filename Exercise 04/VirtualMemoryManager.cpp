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
    resetFrame(0);
}

/**
 * Read a value from the virtual memory
 * @param virtualAddress the virtual address to read from
 * @param value the value to read into
 * @return 1 if the read was successful, 0 otherwise
 */
int VirtualMemoryManager::read(uint64_t virtualAddress, word_t *value) {
    if (value == nullptr) { return FAILURE; }

    if (TABLES_DEPTH == 0) { // No need for virtual memory - shortcut to PMread
        PMread(virtualAddress, value);
        return SUCCESS;
    }

    word_t addr = manageMemory(virtualAddress, TABLES_DEPTH);
    if (addr == -1) { return FAILURE; }

    uint64_t pageOffset = virtualAddress & (PAGE_SIZE - 1);
    *value = readFrame(addr, pageOffset);
    return SUCCESS;
}

/**
 * Write a value to the virtual memory
 * @param virtualAddress the virtual address to write to
 * @param value the value to write
 * @return 1 if the write was successful, 0 otherwise
 */
int VirtualMemoryManager::write(uint64_t virtualAddress, word_t value) {
    if (TABLES_DEPTH == 0) {     // No need for virtual memory
        PMwrite(virtualAddress, value);
        return SUCCESS;
    }

    word_t addr = manageMemory(virtualAddress, TABLES_DEPTH);
    if (addr == -1) { return FAILURE; }

    uint64_t pageOffset = virtualAddress & (PAGE_SIZE - 1);
    writeFrame(addr, pageOffset, value);
    return SUCCESS;
}

/**
 * Manage the memory for a given virtual address
 * @param virtualAddress the virtual address to manage
 * @param depth the depth of the tables
 * @return the address of the frame
 */
word_t VirtualMemoryManager::manageMemory(uint64_t virtualAddress, int depth) {
    if ((virtualAddress >> VIRTUAL_ADDRESS_WIDTH) > 0) { return INVALID; }

    word_t parents[TABLES_DEPTH] = {0};
    word_t addr = 0;

    treeTraveling(virtualAddress, depth, addr, parents);

    return addr;
}

/**
 * Travelling on the tree of the virtual memory
 * @param virtualAddress the virtual address to traverse
 * @param depth the depth of the tables
 * @param addr the address of the frame
 * @param parents the parents of the frame
 */
void VirtualMemoryManager::treeTraveling(uint64_t virtualAddress, int depth, word_t &addr,
                                         word_t parents[TABLES_DEPTH]) {
    uint64_t currentPage = virtualAddress >> OFFSET_WIDTH;

    for (int i = 0; i < depth; ++i) {
        uint64_t pageIndex = virtualAddress >> (OFFSET_WIDTH * (depth - i));
        uint64_t innerOffset = pageIndex & (PAGE_SIZE - 1);

        word_t parentAddr = addr;
        addr = readFrame(addr, innerOffset);

        if (addr == 0) {
            addr = handlePageFault(parentAddr, innerOffset, currentPage, i, parents);
        }
        parents[i] = addr;
    }
}

/**
 * Handle a page fault in the virtual memory
 * @param prevAddr the previous address
 * @param innerOffset the inner offset
 * @param current_page the current page
 * @param depth the depth of the tables
 * @param parents the parents of the frame
 * @return the address of the frame
 */
word_t VirtualMemoryManager::handlePageFault(word_t prevAddr, uint64_t innerOffset, word_t current_page,
                                             int depth, word_t parents[TABLES_DEPTH]) {
    word_t frame = findFrameToUse(current_page, parents); // TODO: check if it's the right frame

    if (frame == -1) { return INVALID; }

    writeFrame(prevAddr, innerOffset, frame);

    if (depth < TABLES_DEPTH - 1) {
        resetFrame(frame);
        return frame;
    } else if (depth == TABLES_DEPTH - 1) {
        PMrestore(frame, current_page);
        return frame;
    }
    return frame;
}

/**
 * Find an empty frame in the virtual memory
 * @param parents the parents of the frame
 * @return the address of the frame
 */
word_t VirtualMemoryManager::findEmptyFrame(const word_t parents[TABLES_DEPTH]) {
    for (word_t frame = 1; frame < NUM_FRAMES; ++frame) {
        if (didComeFromTheSamePath(frame, parents)) { continue; }

        int emptySpots = 0;
        for (int offset = 0; offset < PAGE_SIZE; ++offset) {
            if (readFrame(frame, offset) == 0) { emptySpots++; }
        }

        if (emptySpots == PAGE_SIZE && !isItALeaf(0, 0, frame)) {
            removeParentLink(0, 0, frame);
            return frame;
        }
    }
    return INVALID;
}

/**
 * Find an unused frame in the virtual memory
 * @return the address of the frame
 */
word_t VirtualMemoryManager::findUnusedFrame() {
    word_t nextAvailableFrame = findMaximalSeenFrame(0, 0) + 1;
    if (nextAvailableFrame < NUM_FRAMES) { return nextAvailableFrame; }
    else { return INVALID; }
}

/**
 * Evict a frame with maximal cyclical distance
 * @param pageSwappedIn the page that was swapped in
 * @param parents the ancestors of the frame
 * @return the address of the frame
 */
word_t VirtualMemoryManager::evictFrameWithMaxCyclicalDistance(word_t pageSwappedIn,
                                                               const word_t parents[TABLES_DEPTH]) {
    word_t bestP;
    findTheMaximalCyclicalDistance(0, 0, parents, pageSwappedIn, 0, &bestP);
    return bestP;
}

/**
 * Find a frame to use in the virtual memory
 * @param pageSwappedIn the page that was swapped in
 * @param parents the ancestors of the frame
 * @return the address of the frame
 */
word_t VirtualMemoryManager::findFrameToUse(word_t pageSwappedIn, const word_t parents[TABLES_DEPTH]) {
    word_t frame;

    frame = findEmptyFrame(parents);
    if (frame != -1) { return frame; }

    frame = findUnusedFrame();
    if (frame != -1) { return frame; }

    word_t page = evictFrameWithMaxCyclicalDistance(pageSwappedIn, parents);
    frame = getFrameByPage(page);
    PMevict(frame, page);

    word_t addr = manageMemory(page, TABLES_DEPTH - 1);
    uint64_t pageOffset = page & (PAGE_SIZE - 1);
    writeFrame(addr, pageOffset, 0);

    return frame;
}

/**
 * Check if a frame came from the same path
 * @param frame the frame to check
 * @param parents the parents of the frame
 * @return true if the frame came from the same path, false otherwise
 */
bool VirtualMemoryManager::didComeFromTheSamePath(word_t frame, const word_t parents[TABLES_DEPTH]) {
    for (int i = 0; i < TABLES_DEPTH; ++i) {
        if (frame == parents[i]) { return true; }
    }
    return false;
}

/**
 * Reset a frame in the virtual memory
 * @param frame the frame to reset
 */
void VirtualMemoryManager::resetFrame(word_t frame) {
    for (int i = 0; i < PAGE_SIZE; ++i) {
        PMwrite(frame * PAGE_SIZE + i, 0);
    }
}

/**
 * Remove a parent link from a frame
 * @param frame the frame to remove the parent link from
 * @param depth the depth of the tables
 * @param targetFrame the target frame to remove the parent link from
 */
void VirtualMemoryManager::removeParentLink(word_t frame, int depth, word_t targetFrame) {
    if (depth >= TABLES_DEPTH) { return; }

    word_t value;

    for (int offset = 0; offset < PAGE_SIZE; ++offset) {
        PMread(frame * PAGE_SIZE + offset, &value);
        if (value == targetFrame) { PMwrite(frame * PAGE_SIZE + offset, 0); }
        else { removeParentLink(value, depth + 1, targetFrame); }
    }
}

/**
 * Check if a frame is a leaf
 * @param frame the frame to check
 * @param depth the depth of the tables
 * @param targetFrame the target frame to check
 * @return true if the frame is a leaf, false otherwise
 */
bool VirtualMemoryManager::isItALeaf(word_t frame, int depth, word_t targetFrame) {
    if (depth >= TABLES_DEPTH) { return false; }

    word_t value;
    for (int offset = 0; offset < PAGE_SIZE; ++offset) {
        PMread(frame * PAGE_SIZE + offset, &value);
        if (value == 0) { continue; }

        if (value == targetFrame) { return depth == TABLES_DEPTH - 1; }

        if (isItALeaf(value, depth + 1, targetFrame)) { return true; }
    }
    return false;
//    return depth == TABLES_DEPTH - 1;
}

/**
 * Get the frame by a page
 * @param page the page to get the frame by
 * @return the wanted frame.
 */
word_t VirtualMemoryManager::getFrameByPage(word_t page) {
    word_t addr = 0;
    for (int i = 0; i < TABLES_DEPTH; ++i) {
        uint64_t pageIndex = page >> (OFFSET_WIDTH * (TABLES_DEPTH - i - 1));
        uint64_t innerOffset = pageIndex & (PAGE_SIZE - 1);
        PMread(addr * PAGE_SIZE + innerOffset, &addr);
    }
    return addr;
}

/**
 * Find the maximal seen frame
 * @param frame the frame to start from
 * @param depth the depth of the tables
 * @return the maximal seen frame
 */
word_t VirtualMemoryManager::findMaximalSeenFrame(word_t frame, unsigned int depth) {
    if (depth >= TABLES_DEPTH) { return FAILURE; }

    word_t maxValue = 0, value;
    for (int offset = 0; offset < PAGE_SIZE; ++offset) {
        PMread(frame * PAGE_SIZE + offset, &value);
        if (value > maxValue) { maxValue = value; }

        value = findMaximalSeenFrame(value, depth + 1);

        if (value > maxValue) { maxValue = value; }
    }
    return maxValue;
}

/**
 * Find the maximal cyclical distance
 * @param frame the frame to start from
 * @param depth the depth of the tables
 * @param parents the parents of the frame
 * @param pageSwappedIn the page that was swapped in
 * @param currP the current page
 * @param bestP the best page
 * @return the maximal cyclical distance
 */
word_t VirtualMemoryManager::findTheMaximalCyclicalDistance(word_t frame, unsigned int depth,
                                                            const word_t parents[TABLES_DEPTH],
                                                            word_t pageSwappedIn, word_t currP, word_t *bestP) {
    if (depth >= TABLES_DEPTH) {
        if (didComeFromTheSamePath(getFrameByPage(currP), parents)) { return FAILURE; }
        *bestP = currP;
        int dist1 = NUM_PAGES - std::abs(pageSwappedIn - currP);
        int dist2 = std::abs(pageSwappedIn - currP);
        if (dist1 < dist2) { return dist1; }
        else { return dist2; }
    }

    word_t maxValue = 0;
    word_t value = 0;
    for (int offset = 0; offset < PAGE_SIZE; ++offset) {
        PMread(frame * PAGE_SIZE + offset, &value);
        if (value == 0) { continue; }

        word_t newP = (currP << OFFSET_WIDTH) | offset;
        word_t attemptedBestP;
        value = findTheMaximalCyclicalDistance(value, depth + 1, parents, pageSwappedIn, newP,
                                               &attemptedBestP);
        if (value > maxValue) {
            maxValue = value;
            *bestP = attemptedBestP;
        }
    }
    return maxValue;
}

/**
 * Read a frame from the virtual memory
 * @param addr the address of the frame
 * @param offset the offset of the frame
 * @return the value of the frame
 */
word_t VirtualMemoryManager::readFrame(word_t addr, uint64_t offset) {
    word_t value;
    PMread(addr * PAGE_SIZE + offset, &value);
    return value;
}

/**
 * Write a frame to the virtual memory
 * @param addr the address of the frame
 * @param offset the offset of the frame
 * @param value the value of the frame
 */
void VirtualMemoryManager::writeFrame(word_t addr, uint64_t offset, word_t value) {
    PMwrite(addr * PAGE_SIZE + offset, value);
}
