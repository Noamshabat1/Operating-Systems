#ifndef THREAD_H
#define THREAD_H

#include "uthreads.h"
#include <csignal>
#include <csetjmp>
#include <cstring>

#ifdef __x86_64__
/* code for 64 bit Intel arch */
typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7
#else
/* code for 32 bit Intel arch */
typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5
#endif

address_t translateAddress(address_t addr);

/**
 * @brief The Thread class.
 */
class Thread {

public:
    unsigned int tid;
    char *stack;
    sigjmp_buf env{};
    unsigned int quantumCounter;
    thread_entry_point entryPoint;
    bool isBlocked;
    unsigned int sleepCounter;

    /**
     * @brief Constructor for the Thread class.
     * @param id the thread id.
     * @param entryPoint the thread entry point.
     * @return a new Thread object.
     */
    Thread(unsigned int id, thread_entry_point entryPoint);

    /**
     * @brief Copy constructor for the Thread class.
     * @param other the Thread object to copy.
     * @return a new Thread object.
     */
    Thread(const Thread &other);

    /**
     * @brief Assignment operator for the Thread class.
     * @param other the Thread object to copy.
     * @return a new Thread object.
     */
    Thread &operator=(const Thread &other);

    /**
     * @brief Destructor for the Thread class.
     */
    ~Thread();

    /**
     * @brief Get the thread id.
     * @return the thread id.
     */
    unsigned int getThreadTid() const;

    /**
     * @brief Get the thread stack.
     * @return the thread stack.
     */
    bool getThreadBlockedStatus() const;

    /**
     * @brief Get the thread stack.
     * @return the thread stack.
     */
    unsigned int getThreadQuantumCounter() const;

    /**
     * @brief Get the thread stack.
     * @return the thread stack.
     */
    unsigned int getThreadSleepCounter() const;

    /**
     * @brief Set the thread stack.
     * @param status the new thread stack.
     */
    void setThreadBlockedStatus(bool status);

    /**
     * @brief Set the thread stack.
     * @param status the new thread stack.
     */
    void setThreadQuantumCounter(unsigned int status);

    /**
     * @brief Set the thread stack.
     * @param status the new thread stack.
     */
    void setThreadSleepCounter(unsigned int status);

private:
    /**
     * @brief Initialize the thread environment.
     */
    void initEnv();
};

#endif // THREAD_H
