//
// Created by noams on 7/13/2024.
//

#ifndef VIRTUALMEMORYMANAGER_H
#define VIRTUALMEMORYMANAGER_H

#include "PhysicalMemory.h"
#include <algorithm>
#include <cmath>

#define SUCCESS 1
#define FAILURE 0
#define INVALID (-1)

class VirtualMemoryManager {
public:
    VirtualMemoryManager();

    void initialize();

    word_t read(uint64_t virtualAddress, word_t *value);

    word_t write(uint64_t virtualAddress, word_t value);

private:
    word_t manageMemory(uint64_t virtualAddress, unsigned int depth);

    word_t findAvailableFrame(const word_t parentFrames[TABLES_DEPTH]);

    word_t findUnusedFrame();

    word_t evictFrameWithMaxCyclicalDistance(uint64_t page, const word_t parentFrames[TABLES_DEPTH]);

    word_t findFrameToUse(uint64_t page, const word_t parentFrames[TABLES_DEPTH]);

    word_t getMaxFrameValue(word_t frame, unsigned int depth);

    word_t getMaxCyclicalDistance(word_t frame, unsigned int depth, const word_t parentFrames[TABLES_DEPTH],
                                  uint64_t page, word_t currP, word_t *bestP);

    void traversePageTable(uint64_t virtualAddress, unsigned int depth, word_t &addr, word_t
    parentFrames[TABLES_DEPTH]);

    word_t handlePageFault(word_t prevAddr, uint64_t innerOffset, uint64_t currentPage, unsigned int
    depth, word_t parentFrames[TABLES_DEPTH]);

    void findMaxFrameRecursive(word_t frame, unsigned int depth, word_t &maxFrameValue);

    void removeFrameConnection(word_t startFrame, unsigned int currentDepth, word_t targetFrame);

    bool isLeafFrame(word_t frame, unsigned int depth, word_t targetFrame);

    bool isFrameInUse(word_t frame);

    void removeParentReference(word_t frame);

    // static methods:

    static bool didComeFromTheSamePath(word_t frame, const word_t parentFrames[TABLES_DEPTH]);

    static void clearFrame(word_t frame);

    static word_t readFrame(word_t addr, uint64_t offset);

    static void writeFrame(word_t addr, uint64_t offset, word_t value);

    static uint64_t extractPageIndex(word_t page, unsigned int level);

    static uint64_t calculateInnerOffset(uint64_t pageIndex);

    static word_t resolveFrameAddress(word_t page);

    static word_t fetchNextFrameAddress(word_t currentAddr, word_t page, unsigned int level);

    static word_t computeDistance(word_t currentPage, const word_t parentFrames[TABLES_DEPTH], word_t
    swappedPage, word_t *optimalPage);

    static bool frameIsEmpty(word_t frame);

};

#endif // VIRTUALMEMORYMANAGER_H
