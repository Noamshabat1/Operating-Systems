#include "uthreads.h"
#include "thread.h"

#include <iostream>
#include <list>
#include <queue>
#include <unordered_set>
#include <csignal>
#include <sys/time.h>

// Constants:
#define TIMER_ERR "system error: set-itimer had failed. "

#define INVALID_ENTRY_POINT_ERR "thread library error: Null entry point. "

#define MAX_THREADS_ERR "thread library error: the max number of threads reached. "

#define MAIN_THREAD_BLOCKED_ERR "thread library error: cannot block the main thread. "

#define ALLOCATION_FAILURE_ERR "system error: thread allocation has failed. "

#define INVALID_QUANTUM_ERR "thread library error: invalid sleep quantum's. "

#define INVALID_SLEEP_REQUEST_TO_MAIN_THREAD_ERR "thread library error: cannot do sleep to main thread. "

#define INVALID_TID_ERR "thread library error: invalid thread id. "

#define UNDEFINED_TID_ERR "thread library error: the thread does not exist. "

#define INVALID_QUANTUM_FOR_INIT_ERR "thread library error: quantum_usecs must not be negative. "

#define SIGACTTION_ERR "system error: sigaction failed for SIGVTALRM signal. "

#define SIGPROCMASK_ERR "system error: sigprocmask error. "

#define EMPTY_READY_Q_ERR "thread library error: no more threads are available to run. "

#define OPEN_SPOT false

#define TAKEN_SPOT true

#define SUCCESS_EXIT 0

#define FAILURE_EXIT (-1)

/**
 * @brief The ThreadAction enum represents the possible actions that can be taken by the thread manager.
 */
enum class ThreadAction {
    TERMINATE,
    BLOCKED,
    CYCLE
};

/**
 * @brief The ThreadsEngine class represents the thread manager.
 */
void timerHandler(int sig);

/**
 * @brief The ThreadsEngine class represents the thread manager.
 */
class ThreadsEngine {
public:
    int totalNumOfQuantumsCount;
    unsigned int quantumUsecs;
    struct itimerval timer{};
    Thread *runningThread;
    std::deque<Thread *> readyQueue;
    std::unordered_set<Thread *> blockedSet;
    bool threadTidAvailability[MAX_THREAD_NUM] = {OPEN_SPOT};

    /**
     * @brief Constructs a new ThreadsEngine object.
     * @param quantumUsecs the quantum time in microseconds.
     */
    explicit ThreadsEngine(unsigned int quantumUsecs) : quantumUsecs(quantumUsecs),
                                                        totalNumOfQuantumsCount(1) {
        auto emptyLambda = []() {}; // just not a nullptr argument for the main thread
        runningThread = new Thread(0, emptyLambda);
        runningThread->quantumCounter++;
        threadTidAvailability[0] = TAKEN_SPOT;
    }

    /**
     * @brief Constructs a new ThreadsEngine object.
     */
    ThreadsEngine() : ThreadsEngine(0) {}

    /**
     * @brief Destructs the ThreadsEngine object.
     */
    void scheduler() {
        struct sigaction sa{nullptr};
        sa.sa_handler = &timerHandler;
        if (sigaction(SIGVTALRM, &sa, nullptr) < 0) {
            std::cerr << SIGACTTION_ERR << std::endl;
            exit(1);
        }
        timer.it_value.tv_sec = quantumUsecs / 1000000;
        timer.it_value.tv_usec = quantumUsecs % 1000000;
        timer.it_interval = timer.it_value;
        restartTheClock();
    }

    /**
     * @brief Restarts the clock.
     */
    void restartTheClock() {
        if (setitimer(ITIMER_VIRTUAL, &timer, nullptr) < 0) {
            std::cerr << TIMER_ERR << std::endl;
            exit(1);
        }
    }

    /**
     * @brief Creates a new thread.
     * @param entryPoint the entry point of the thread.
     * @return the thread id.
     */
    int createThread(thread_entry_point entryPoint) {
        if (!entryPoint) {
            std::cerr << INVALID_ENTRY_POINT_ERR << std::endl;
            return FAILURE_EXIT;
        }

        int tid = getNextAvailableTid();
        if (tid == -1) {
            std::cerr << MAX_THREADS_ERR << std::endl;
            return FAILURE_EXIT;
        }

        threadTidAvailability[tid] = TAKEN_SPOT; // Mark the TID as taken

        // Create the new thread and handle potential allocation failure
        Thread *newThread = nullptr;
        try {
            newThread = new Thread(tid, entryPoint);
        } catch (const std::bad_alloc &) {
            threadTidAvailability[tid] = OPEN_SPOT; // Roll back TID spot as available
            std::cerr << ALLOCATION_FAILURE_ERR << std::endl;
            return FAILURE_EXIT;
        }

        // Add the new thread to the readyQueue
        readyQueue.push_back(newThread);
        return tid;
    }

    /**
     * @brief Terminates a thread.
     * @param tid the thread id.
     * @return 0 if the thread was successfully terminated and -1 otherwise.
     */
    int terminateThread(int tid) {
        if (!isValidTid(tid)) {
            std::cerr << INVALID_TID_ERR << std::endl;
            return FAILURE_EXIT;
        }
        if (!DoseThreadExists(tid)) {
            std::cerr << UNDEFINED_TID_ERR << std::endl;
            return FAILURE_EXIT;
        }
        if (tid == 0) {
            clearThreads();
            exit(0);
        }

        threadTidAvailability[tid] = OPEN_SPOT;
        removeThreadFromContainer(tid, readyQueue);
        removeThreadFromContainer(tid, blockedSet);
        if (runningThread->getThreadTid() == tid) {
            switchThread(ThreadAction::TERMINATE);
        }
        return SUCCESS_EXIT;
    }

    /**
     * @brief Blocks a thread.
     * @param tid the thread id.
     * @return 0 if the thread was successfully blocked and -1 otherwise.
     */
    int blockThread(int tid) {
        if (!isValidTid(tid)) {
            std::cerr << INVALID_TID_ERR << std::endl;
            return FAILURE_EXIT;
        }
        if (!DoseThreadExists(tid)) {
            std::cerr << UNDEFINED_TID_ERR << std::endl;
            return FAILURE_EXIT;
        }
        if (tid == 0) {
            std::cerr << MAIN_THREAD_BLOCKED_ERR << std::endl;
            return FAILURE_EXIT;
        }
        if (isThreadBlocked(tid)) {
            return SUCCESS_EXIT;
        }
        if (isThreadSleepy(tid)) { // already in blocked q
            addSleepThreadToBlocked(tid);
            return SUCCESS_EXIT;
        }
        if (isThreadInContainer(tid, readyQueue)) {
            moveToBlocked(tid);
            return SUCCESS_EXIT;
        }
        if (runningThread->getThreadTid() == tid) { // if current one need to be blocked
            runningThread->setThreadBlockedStatus(true);
            switchThread(ThreadAction::BLOCKED);
            return SUCCESS_EXIT;
        }
        return FAILURE_EXIT;
    }

    /**
     * @brief Resumes a thread.
     * @param tid the thread id.
     * @return 0 if the thread was successfully resumed and -1 otherwise.
     */
    int resumeThread(int tid) {
        if (!isValidTid(tid)) {
            std::cerr << INVALID_TID_ERR << std::endl;
            return FAILURE_EXIT;
        }
        if (!DoseThreadExists(tid)) {
            std::cerr << UNDEFINED_TID_ERR << std::endl;
            return FAILURE_EXIT;
        }

        auto it = blockedSet.begin();
        while (it != blockedSet.end()) {
            if ((*it)->getThreadTid() == static_cast<unsigned int>(tid)) {
                (*it)->setThreadBlockedStatus(false);
                if ((*it)->getThreadSleepCounter() == 0) {
                    readyQueue.push_back(*it);
                    it = blockedSet.erase(it); // Erase and get next iterator
                } else {
                    ++it;
                }
                return SUCCESS_EXIT;
            } else {
                ++it;
            }
        }
        return SUCCESS_EXIT;
    }


    /**
     * @brief Puts a thread to sleep.
     * @param sleepQuantums the number of quantums to sleep.
     * @return 0 if the thread was successfully put to sleep and -1 otherwise.
     */
    int sleepThread(int sleepQuantums) {
        if (sleepQuantums < 0) {
            std::cerr << INVALID_QUANTUM_ERR << std::endl;
            return FAILURE_EXIT;
        }
        if (runningThread->getThreadTid() == 0) {
            std::cerr << INVALID_SLEEP_REQUEST_TO_MAIN_THREAD_ERR << std::endl;
            return FAILURE_EXIT;
        }
        runningThread->setThreadSleepCounter(sleepQuantums);
        switchThread(ThreadAction::BLOCKED);
        return SUCCESS_EXIT;
    }

    /**
     * @brief Gets the current thread id.
     * @return the current thread id.
     */
    unsigned int getThreadQuantums(int tid) {
        if (!isValidTid(tid)) {
            std::cerr << INVALID_TID_ERR << std::endl;
            return FAILURE_EXIT;
        }
        if (!DoseThreadExists(tid)) {
            std::cerr << UNDEFINED_TID_ERR << std::endl;
            return FAILURE_EXIT;
        }
        if (runningThread->getThreadTid() == static_cast<unsigned int>(tid)) {
            return runningThread->getThreadQuantumCounter();
        }
        for (const auto &thread: readyQueue) {
            if (thread->getThreadTid() == static_cast<unsigned int>(tid)) {
                return thread->getThreadQuantumCounter();
            }
        }
        for (const auto &thread: blockedSet) {
            if (thread->getThreadTid() == static_cast<unsigned int>(tid)) {
                return thread->getThreadQuantumCounter();
            }
        }
        return FAILURE_EXIT;
    }

    /**
     * @brief Switches the thread.
     * @param action the action to take.
     */
    void switchThread(ThreadAction action) {
        updateSleepCounters();

        if (sigsetjmp(runningThread->env, 1) != 0) {
            return;
        }

        if (action == ThreadAction::CYCLE) {
            if (!readyQueue.empty()) {
                readyQueue.push_back(runningThread);
            } else {
                unsigned int cur = runningThread->getThreadQuantumCounter() + 1;
                runningThread->setThreadQuantumCounter(cur);
                totalNumOfQuantumsCount++;
                return;
            }
        } else if (action == ThreadAction::TERMINATE) {
            delete runningThread;
            runningThread = nullptr;
        } else if (action == ThreadAction::BLOCKED) {
            blockedSet.insert(runningThread);
        }

        if (!readyQueue.empty()) {
            runningThread = readyQueue.front();
            readyQueue.pop_front();
        } else {
            std::cerr << EMPTY_READY_Q_ERR << std::endl;
            exit(1);
        }

        unsigned int cur = runningThread->getThreadQuantumCounter() + 1;
        runningThread->setThreadQuantumCounter(cur);
        totalNumOfQuantumsCount++;

        restartTheClock();

        siglongjmp(runningThread->env, 1);
    }


private:
    /**
     * @brief Checks if the thread id is valid.
     * @param tid the thread id.
     * @return true if the thread id is valid and false otherwise.
     */
    static bool isValidTid(int tid) {
        return tid >= 0 && tid < MAX_THREAD_NUM;
    }

    /**
     * @brief Checks if the thread exists.
     * @param tid the thread id.
     * @return true if the thread exists and false otherwise.
     */
    bool DoseThreadExists(int tid) const {
        return threadTidAvailability[tid] == TAKEN_SPOT;
    }

    /**
     * @brief Clears the threads.
     */
    void clearThreads() {
        readyQueue.clear();
        blockedSet.clear();
        delete runningThread;
        runningThread = nullptr;
    }

    /**
     * @brief Gets the next available thread id.
     * @return the next available thread id.
     */
    int getNextAvailableTid() const {
        for (int i = 1; i < MAX_THREAD_NUM; i++) {
            if (threadTidAvailability[i] == OPEN_SPOT) {
                return i;
            }
        }
        return FAILURE_EXIT;
    }

    /**
     *
     * @param Checks if the thread is in the set.
     * @param set the set.
     * @return The thread and nullptr otherwise.
     */
    static Thread *findThreadInSet(unsigned int tid, const std::unordered_set<Thread *> &set) {
        for (const auto &thread: set) {
            if (thread->getThreadTid() == static_cast<unsigned int>(tid)) {
                return thread;
            }
        }
        return nullptr;
    }

    /**
     * @brief Checks if the thread is blocked.
     * @param tid the thread id.
     * @return true if the thread is blocked and false otherwise.
     */
    bool isThreadBlocked(int tid) const {
        Thread *thread = findThreadInSet(tid, blockedSet);
        return thread != nullptr && thread->getThreadBlockedStatus();
    }

    /**
     * @brief Checks if the thread is sleepy.
     * @param tid the thread id.
     * @return true if the thread is sleepy and false otherwise.
     */
    bool isThreadSleepy(unsigned int tid) const {
        Thread *thread = findThreadInSet(tid, blockedSet);
        return thread != nullptr && thread->getThreadSleepCounter() > 0;
    }

    /**
     * @brief Checks if the thread is in the queue.
     * @param tid the thread id.
     * @param container the container.
     * @return true if the thread is in the queue and false otherwise.
     */
    template<typename Container>
    static bool isThreadInContainer(int tid, const Container &container) {
        for (const auto &thread: container) {
            if (thread->getThreadTid() == static_cast<unsigned int>(tid)) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Adds a sleep thread to the blocked queue.
     * @param tid the thread id.
     */
    void addSleepThreadToBlocked(int tid) {
        for (auto &thread: blockedSet) {
            if (thread->getThreadTid() == static_cast<unsigned int>(tid)) {
                thread->setThreadBlockedStatus(true);
                return;
            }
        }
    }

    /**
     * @brief Moves a thread to the blocked queue.
     * @param tid the thread id.
     */
    void moveToBlocked(int tid) {
        for (auto it = readyQueue.begin(); it != readyQueue.end(); ++it) {
            if ((*it)->getThreadTid() == static_cast<unsigned int>(tid)) {
                Thread *thread = *it;
                thread->setThreadBlockedStatus(true);
                blockedSet.insert(thread); // Insert into unordered_set
                readyQueue.erase(it); // Remove from deque
                break;
            }
        }
    }

    /**
    * @brief Removes a thread from the container.
    * @param tid the thread id.
    * @param container the container (e.g., deque, unordered_set).
    * @tparam Container the type of container (e.g., deque<Thread*>, unordered_set<Thread*>).
    */
    template<typename Container>
    static void removeThreadFromContainer(int tid, Container &container) {
        for (auto it = container.begin(); it != container.end(); ++it) {
            if ((*it)->getThreadTid() == static_cast<unsigned int>(tid)) {
                delete *it;
                container.erase(it);
                return;
            }
        }
    }

    /**
     * @brief Updates the sleep counters.
     */
    void updateSleepCounters() {
        for (auto it = blockedSet.begin(); it != blockedSet.end();) {
            if ((*it)->getThreadSleepCounter() > 0) {
                Thread *thread = *it;
                unsigned int cur = thread->getThreadSleepCounter() - 1;
                thread->setThreadSleepCounter(cur);
            }
            if ((*it)->getThreadSleepCounter() == 0 && !(*it)->getThreadBlockedStatus()) { // back to ready.
                readyQueue.push_back(*it);
                it = blockedSet.erase(it); // Erase and get next iterator
            } else {
                ++it;
            }
        }
    }
};

ThreadsEngine threadsEngine;

/**
 * @brief The timer handler.
 * @param sig the signal.
 */
void timerHandler(int sig) {
    threadsEngine.switchThread(ThreadAction::CYCLE);
}

/**
 * blocks the signals.
 */
void blockSignal() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGVTALRM);
    if (sigprocmask(SIG_BLOCK, &set, nullptr) == -1) {
        std::cerr << SIGPROCMASK_ERR << std::endl;
        exit(1);
    }
}

/**
 * unblocks the signals.
 */
void unblockSignal() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGVTALRM);
    if (sigprocmask(SIG_UNBLOCK, &set, nullptr) == -1) {
        std::cerr << SIGPROCMASK_ERR << std::endl;
        exit(1);
    }
}

/**
 * @brief Initializes the thread library.
 * @param quantum_usecs the quantum time in microseconds.
 * @return 0 if the thread library was successfully initialized and -1 otherwise.
 */
int uthread_init(int quantum_usecs) {
    if (quantum_usecs < 0) {
        std::cerr << INVALID_QUANTUM_FOR_INIT_ERR << std::endl;
        return FAILURE_EXIT;
    }
    threadsEngine = ThreadsEngine(quantum_usecs);
    threadsEngine.scheduler();
    return SUCCESS_EXIT;
}

/**
 * @brief Creates a new thread.
 * @param entry_point the entry point of the thread.
 * @return the thread id.
 */
int uthread_spawn(thread_entry_point entry_point) {
    blockSignal();
    int result = threadsEngine.createThread(entry_point);
    unblockSignal();
    return result;
}

/**
 * @brief Terminates a thread.
 * @param tid the thread id.
 * @return 0 if the thread was successfully terminated and -1 otherwise.
 */
int uthread_terminate(int tid) {
    blockSignal();
    int result = threadsEngine.terminateThread(tid);
    unblockSignal();
    return result;
}

/**
 * @brief Blocks a thread.
 * @param tid the thread id.
 * @return 0 if the thread was successfully blocked and -1 otherwise.
 */
int uthread_block(int tid) {
    blockSignal();
    int result = threadsEngine.blockThread(tid);
    unblockSignal();
    return result;
}

/**
 * @brief Resumes a thread.
 * @param tid the thread id.
 * @return 0 if the thread was successfully resumed and -1 otherwise.
 */
int uthread_resume(int tid) {
    blockSignal();
    int result = threadsEngine.resumeThread(tid);
    unblockSignal();
    return result;
}

/**
 * @brief Puts a thread to sleep.
 * @param numQuantums the number of quantums to sleep.
 * @return 0 if the thread was successfully put to sleep and -1 otherwise.
 */
int uthread_sleep(int numQuantums) {
    blockSignal();
    int result = threadsEngine.sleepThread(numQuantums);
    unblockSignal();
    return result;
}

/**
 * @brief Gets the current thread id.
 * @return the current thread id.
 */
int uthread_get_tid() {
    return (int)threadsEngine.runningThread->getThreadTid();
}

/**
 * @brief Gets the total number of quantums.
 * @return the total number of quantums.
 */
int uthread_get_total_quantums() {
    return threadsEngine.totalNumOfQuantumsCount;
}

/**
 * @brief Gets the number of quantums of a thread.
 * @param tid the thread id.
 * @return the number of quantums of a thread.
 */
int uthread_get_quantums(int tid) {
    blockSignal();
    int result = (int) threadsEngine.getThreadQuantums(tid);
    unblockSignal();
    return result;
}