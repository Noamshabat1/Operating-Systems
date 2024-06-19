#include "thread.h"

#ifdef __x86_64__

address_t translateAddress(address_t addr) {
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
            : "=g" (ret)
            : "0" (addr));
    return ret;
}

#else
address_t translateAddress(address_t addr) {
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}
#endif

/**
 * @brief Constructor for the Thread class.
 * @param id the thread id.
 * @param entryPoint the thread entry point.
 * @return a new Thread object.
 */
Thread::Thread(unsigned int id, thread_entry_point entryPoint)
        : tid(id), stack(new char[STACK_SIZE]), entryPoint(entryPoint),
          quantumCounter(0), sleepCounter(0), isBlocked(false) {
    initEnv();
}
/**
 * @brief Copy constructor for the Thread class.
 * @param other the Thread object to copy.
 * @return a new Thread object.
 */

Thread::Thread(const Thread &other)
        : tid(other.tid), stack(new char[STACK_SIZE]), entryPoint(other.entryPoint),
          quantumCounter(other.quantumCounter), sleepCounter(other.sleepCounter), isBlocked(other.isBlocked) {
    std::memcpy(stack, other.stack, STACK_SIZE);
    std::memcpy(&env, &other.env, sizeof(sigjmp_buf));
}

/**
 * @brief Assignment operator for the Thread class.
 * @param other the Thread object to copy.
 * @return a new Thread object.
 */
Thread &Thread::operator=(const Thread &other) {
    if (this != &other) {
        tid = other.tid;
        delete[] stack;
        stack = new char[STACK_SIZE];
        std::memcpy(stack, other.stack, STACK_SIZE);
        quantumCounter = other.quantumCounter;
        entryPoint = other.entryPoint;
        isBlocked = other.isBlocked;
        sleepCounter = other.sleepCounter;
        std::memcpy(&env, &other.env, sizeof(sigjmp_buf));
    }
    return *this;
}

/**
 * @brief Destructor for the Thread class.
 */
Thread::~Thread() {
    delete[] stack;
    stack = nullptr;
}

/**
 * @return the current threadId.
 */
 unsigned int Thread::getThreadTid() const {
    return this->tid;
}

/**
 * @return the current thread blocked status.
 */
bool Thread::getThreadBlockedStatus() const {
    return this->isBlocked;
}

/**
 * @return the current thread quantum counter.
 */
unsigned int  Thread::getThreadQuantumCounter() const {
    return this->quantumCounter;
}

/**
 * @return the current thread sleep counter.
 */
unsigned int Thread::getThreadSleepCounter() const {
    return this->sleepCounter;
}

/**
 * @brief set the current thread blocked status.
 * @param status the new status.
 */
void Thread::setThreadBlockedStatus(bool status) {
    this->isBlocked = status;
}

/**
 * @brief set the current thread quantum counter.
 * @param status the new status.
 */
void Thread::setThreadQuantumCounter(unsigned int status)  {
    this->quantumCounter = status;
}

/**
 * @brief set the current thread sleep counter.
 * @param status the new status.
 */
void  Thread::setThreadSleepCounter(unsigned int status)  {
    this->sleepCounter = status;
}

/**
 * @brief Initialize the thread environment.
 */
void Thread::initEnv() {
    auto sp = reinterpret_cast<address_t>(&stack[STACK_SIZE]);
    auto pc = reinterpret_cast<address_t>(entryPoint);

    sigsetjmp(env, 1);
    env->__jmpbuf[JB_SP] = translateAddress(sp);
    env->__jmpbuf[JB_PC] = translateAddress(pc);
    sigemptyset(&env->__saved_mask);
}
