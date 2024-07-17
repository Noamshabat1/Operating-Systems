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

    word_t findEmptyFrame(const word_t parents[TABLES_DEPTH]);

    word_t findUnusedFrame();

    word_t evictFrameWithMaxCyclicalDistance(word_t pageSwappedIn, const word_t parents[TABLES_DEPTH]);

    word_t findFrameToUse(word_t pageSwappedIn, const word_t parents[TABLES_DEPTH]);

    static bool didComeFromTheSamePath(word_t frame, const word_t parents[TABLES_DEPTH]);

    static void resetFrame(word_t frame);

    void removeParentLink(word_t frame, int depth, word_t targetFrame);

    bool isItALeaf(word_t frame, int depth, word_t targetFrame);

    static word_t getFrameByPage(word_t page);

    word_t findMaximalSeenFrame(word_t frame, unsigned int depth);

    word_t findMaximalCyclicalDistance(word_t frame, unsigned int depth, const word_t parents[TABLES_DEPTH],
                                       word_t pageSwappedIn, word_t currP, word_t *bestP);

    static word_t readFrame(word_t addr, uint64_t offset);

    static void writeFrame(word_t addr, uint64_t offset, word_t value);

    void treeTraveling(uint64_t virtualAddress, int depth, word_t &addr, word_t parents[TABLES_DEPTH]);

    word_t handlePageFault(word_t prevAddr, uint64_t innerOffset, word_t current_page, int depth,
                           word_t ancestors[TABLES_DEPTH]);
};

#endif // VIRTUALMEMORYMANAGER_H
