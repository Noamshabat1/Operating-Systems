//
// Created by noams on 7/13/2024.
//

#include "VirtualMemoryManager.h"

VirtualMemoryManager vmManager;

void VMinitialize() {
    vmManager.initialize();
}

int VMread(uint64_t virtualAddress, word_t *value) {
    return vmManager.read(virtualAddress, value);
}

int VMwrite(uint64_t virtualAddress, word_t value) {
    return vmManager.write(virtualAddress, value);
}
