//
// Created by noams on 7/13/2024.
//

#include "VirtualMemoryManager.h"

VirtualMemoryManager::VirtualMemoryManager() {}

void VirtualMemoryManager::initialize() {
    resetFrame(0);
}

int VirtualMemoryManager::read(uint64_t virtualAddress, word_t *value) {
    if (value == nullptr) { return 0; }

    if (TABLES_DEPTH == 0) {
        PMread(virtualAddress, value);
        return 1;
    }

    word_t addr = manageMemory(virtualAddress, TABLES_DEPTH);
    if (addr == -1) { return 0; }

    uint64_t pageOffset = virtualAddress & (PAGE_SIZE - 1);
    *value = readFrame(addr, pageOffset);
    return 1;
}

int VirtualMemoryManager::write(uint64_t virtualAddress, word_t value) {
    if (TABLES_DEPTH == 0) {
        PMwrite(virtualAddress, value);
        return 1;
    }

    word_t addr = manageMemory(virtualAddress, TABLES_DEPTH);
    if (addr == -1) { return 0; }

    uint64_t pageOffset = virtualAddress & (PAGE_SIZE - 1);
    writeFrame(addr, pageOffset, value);
    return 1;
}

word_t VirtualMemoryManager::manageMemory(uint64_t virtualAddress, int depth) {
    if ((virtualAddress >> VIRTUAL_ADDRESS_WIDTH) > 0) { return -1; }

    word_t ancestors[TABLES_DEPTH] = {0};
    word_t addr = 0;

    traverseTree(virtualAddress, depth, addr, ancestors);

    return addr;
}

void VirtualMemoryManager::traverseTree(uint64_t virtualAddress, int depth, word_t &addr,
                                        word_t ancestors[TABLES_DEPTH]) {
    uint64_t current_page = virtualAddress >> OFFSET_WIDTH;

    for (int i = 0; i < depth; i++) {
        uint64_t page_index = virtualAddress >> (OFFSET_WIDTH * (depth - i));
        uint64_t innerOffset = page_index & (PAGE_SIZE - 1);

        word_t prevAddr = addr;
        addr = readFrame(addr, innerOffset);

        if (addr == 0) {
            addr = handlePageFault(prevAddr, innerOffset, current_page, i, ancestors);
        }
        ancestors[i] = addr;
    }
}

word_t
VirtualMemoryManager::handlePageFault(word_t prevAddr, uint64_t innerOffset, word_t current_page, int depth,
                                      word_t ancestors[TABLES_DEPTH]) {
    word_t frame = findFrameToUse(current_page, ancestors);

    if (frame == -1) { return -1; }

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

word_t VirtualMemoryManager::findEmptyFrame(const word_t ancestors[TABLES_DEPTH]) {
    for (word_t frame = 1; frame < NUM_FRAMES; frame++) {
        if (isAncestor(frame, ancestors)) continue;

        int zeros = 0;
        for (int offset = 0; offset < PAGE_SIZE; offset++) {
            if (readFrame(frame, offset) == 0) zeros++;
        }

        if (zeros == PAGE_SIZE && !isLeaf(0, 0, frame)) {
            removeAncestorLink(0, 0, frame);
            return frame;
        }
    }
    return -1;
}

word_t VirtualMemoryManager::findUnusedFrame() {
    word_t nextAvailableFrame = findMaximalSeenFrame(0, 0) + 1;
    if (nextAvailableFrame < NUM_FRAMES) {
        return nextAvailableFrame;
    } else { return -1; }
}

word_t VirtualMemoryManager::evictFrameWithMaxCyclicalDistance(word_t pageSwappedIn,
                                                               const word_t ancestors[TABLES_DEPTH]) {
    word_t bestP;
    findMaximalCyclicalDistance(0, 0, ancestors, pageSwappedIn, 0, &bestP);
    return bestP;
}

word_t VirtualMemoryManager::findFrameToUse(word_t pageSwappedIn, const word_t ancestors[TABLES_DEPTH]) {
    word_t frame;

    frame = findEmptyFrame(ancestors);
    if (frame != -1) { return frame; }

    frame = findUnusedFrame();
    if (frame != -1) { return frame; }

    word_t page = evictFrameWithMaxCyclicalDistance(pageSwappedIn, ancestors);
    frame = getFrameByPage(page);
    PMevict(frame, page);

    word_t addr = manageMemory(page, TABLES_DEPTH - 1);
    uint64_t pageOffset = page & (PAGE_SIZE - 1);
    writeFrame(addr, pageOffset, 0);

    return frame;
}

bool VirtualMemoryManager::isAncestor(word_t frame, const word_t ancestors[TABLES_DEPTH]) {
    for (int i = 0; i < TABLES_DEPTH; i++) {
        if (frame == ancestors[i]) {
            return true;
        }
    }
    return false;
}

void VirtualMemoryManager::resetFrame(word_t frame) {
    for (int j = 0; j < PAGE_SIZE; j++) {
        PMwrite(frame * PAGE_SIZE + j, 0);
    }
}

void VirtualMemoryManager::removeAncestorLink(word_t frame, int depth, word_t targetFrame) {
    if (depth >= TABLES_DEPTH) { return; }

    word_t value;

    for (int offset = 0; offset < PAGE_SIZE; offset++) {
        PMread(frame * PAGE_SIZE + offset, &value);
        if (value == targetFrame) {
            PMwrite(frame * PAGE_SIZE + offset, 0);
        } else { removeAncestorLink(value, depth + 1, targetFrame); }
    }
}

bool VirtualMemoryManager::isLeaf(word_t frame, int depth, word_t targetFrame) {
    if (depth >= TABLES_DEPTH) return false;

    word_t value;
    for (int offset = 0; offset < PAGE_SIZE; offset++) {
        PMread(frame * PAGE_SIZE + offset, &value);
        if (value == 0) { continue; }

        if (value == targetFrame) return (depth == TABLES_DEPTH - 1);
        if (isLeaf(value, depth + 1, targetFrame)) { return true; }
    }
    return false;
}

word_t VirtualMemoryManager::getFrameByPage(word_t page) {
    word_t addr = 0;
    for (int i = 0; i < TABLES_DEPTH; i++) {
        uint64_t page_index = page >> (OFFSET_WIDTH * (TABLES_DEPTH - i - 1));
        uint64_t innerOffset = page_index & (PAGE_SIZE - 1);
        PMread(addr * PAGE_SIZE + innerOffset, &addr);
    }
    return addr;
}

word_t VirtualMemoryManager::findMaximalSeenFrame(word_t frame, unsigned int depth) {
    if (depth >= TABLES_DEPTH) { return 0; }

    word_t maxValue = 0, value;
    for (int offset = 0; offset < PAGE_SIZE; offset++) {
        PMread(frame * PAGE_SIZE + offset, &value);
        if (value > maxValue) { maxValue = value; }

        value = findMaximalSeenFrame(value, depth + 1);

        if (value > maxValue) { maxValue = value; }
    }
    return maxValue;
}

word_t VirtualMemoryManager::findMaximalCyclicalDistance(word_t frame, unsigned int depth,
                                                         const word_t ancestors[TABLES_DEPTH],
                                                         word_t pageSwappedIn, word_t p, word_t *bestP) {
    if (depth >= TABLES_DEPTH) {
        if (isAncestor(getFrameByPage(p), ancestors)) { return 0; }
        *bestP = p;
        int dist1 = NUM_PAGES - std::abs(pageSwappedIn - p);
        int dist2 = std::abs(pageSwappedIn - p);
        if (dist1 < dist2) { return dist1; }
        else { return dist2; }
    }

    word_t maxValue = 0, value;
    for (int offset = 0; offset < PAGE_SIZE; offset++) {
        PMread(frame * PAGE_SIZE + offset, &value);
        if (value == 0) { continue; }

        word_t newP = (p << OFFSET_WIDTH) | offset;
        word_t attemptedBestP;
        value = findMaximalCyclicalDistance(value, depth + 1, ancestors, pageSwappedIn, newP,
                                            &attemptedBestP);
        if (value > maxValue) {
            maxValue = value;
            *bestP = attemptedBestP;
        }
    }
    return maxValue;
}

word_t VirtualMemoryManager::readFrame(word_t addr, uint64_t offset) {
    word_t value;
    PMread(addr * PAGE_SIZE + offset, &value);
    return value;
}

void VirtualMemoryManager::writeFrame(word_t addr, uint64_t offset, word_t value) {
    PMwrite(addr * PAGE_SIZE + offset, value);
}