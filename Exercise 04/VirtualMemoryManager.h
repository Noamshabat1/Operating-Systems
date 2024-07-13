//
// Created by noams on 7/13/2024.
//

#ifndef VIRTUALMEMORYMANAGER_H
#define VIRTUALMEMORYMANAGER_H

#include "PhysicalMemory.h"
#include <algorithm>
#include <cmath>

class VirtualMemoryManager {
public:
    VirtualMemoryManager();

    void initialize();

    int read(uint64_t virtualAddress, word_t *value);

    int write(uint64_t virtualAddress, word_t value);

private:
    word_t manageMemory(uint64_t virtualAddress, int depth);

    word_t findEmptyFrame(const word_t ancestors[TABLES_DEPTH]);

    word_t findUnusedFrame();

    word_t evictFrameWithMaxCyclicalDistance(word_t pageSwappedIn, const word_t ancestors[TABLES_DEPTH]);

    word_t findFrameToUse(word_t pageSwappedIn, const word_t ancestors[TABLES_DEPTH]);

    bool isAncestor(word_t frame, const word_t ancestors[TABLES_DEPTH]);

    void resetFrame(word_t frame);

    void removeAncestorLink(word_t frame, int depth, word_t targetFrame);

    bool isLeaf(word_t frame, int depth, word_t targetFrame);

    word_t getFrameByPage(word_t page);

    word_t findMaximalSeenFrame(word_t frame, unsigned int depth);

    word_t findMaximalCyclicalDistance(word_t frame, unsigned int depth, const word_t ancestors[TABLES_DEPTH],
                                       word_t pageSwappedIn, word_t p, word_t *bestP);

    word_t readFrame(word_t addr, uint64_t offset);

    void writeFrame(word_t addr, uint64_t offset, word_t value);

    void traverseTree(uint64_t virtualAddress, int depth, word_t &addr, word_t ancestors[TABLES_DEPTH]);

    word_t handlePageFault(word_t prevAddr, uint64_t innerOffset, word_t current_page, int depth,
                           word_t ancestors[TABLES_DEPTH]);
};

#endif // VIRTUALMEMORYMANAGER_H