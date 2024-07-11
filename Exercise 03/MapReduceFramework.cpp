#include "Barrier.h"
#include "MapReduceFramework.h"

#include <iostream>
#include <atomic>
#include <semaphore.h>
#include <pthread.h>
#include <algorithm>
#include <mutex>

// Error messages for error handling
namespace eMsg {
    const char *CREATE_THREAD_ERR = "Failed to create the thread. ";
    const char *JOIN_THREAD_ERR = "Failed to join the thread. ";
    const char *INIT_CONTEXT_ERR = "Failed to allocate memory for job context. ";
    const char *INIT_THREADS_ERR = "Failed to allocate memory for threads. ";
    const char *INIT_THREADS_CONTEXTS_ERR = "Failed to allocate memory for thread contexts. ";
    const char *INIT_SHUFFLED_DATA_ERR = "Failed to allocate memory for shuffled data. ";
    const char *INIT_BARRIER_ERR = "Failed to allocate memory for barrier. ";
    const char *INIT_INTERMEDIATE_DATA_ERR = "Failed to allocate memory for intermediate data. ";
    const char *INIT_SEMAPHORE_ERR = "Failed to initialize the semaphore. ";
    const char *DESTROY_SEMAPHORE_ERR = "Failed to destroy semaphore. ";
    const char *POST_SEMAPHORE_ERR = "Failed to post the semaphore. ";
    const char *WAIT_SEMAPHORE_ERR = "Failed to wait the semaphore. ";
}

/**
 * @brief define function that handle error message action.
 * @param msg the message to be printed.
 * @return make the program exit with code 1 after printing.
 */
#define HANDLE_ERROR_MSG(msg) { std::cout << "system error: " << msg << std::endl; exit(1); }

// Struct declarations:
struct IntermediateInformation;
struct ThreadInformation;
struct JobContext;

// Function declarations:
void *startThreadJobFramework(void *arg);

void executeMapStage(JobContext *jobContext, ThreadInformation *threadInformation);

void sortIntermediateData(ThreadInformation *threadInformation);

void executeShuffleStage(JobContext *jobContext);

K2 *getMaxKey(JobContext *jobContext);

IntermediateVec *getAllKeysByMatchingMaxKey(JobContext *jobContext, K2 *max_key);

void executeReduceStage(JobContext *jobContext);

/**
 * @brief A class representing a semaphore.
 * The class is used to synchronize threads in the MapReduceFramework.
 */
class Semaphore {
 public:
  // Constructor
  explicit Semaphore(int value = 1) : sem() {
    if (sem_init(&sem, 0, value) != 0) HANDLE_ERROR_MSG(eMsg::INIT_SEMAPHORE_ERR)
  }

  // Destructor
  ~Semaphore() {
    if (sem_destroy(&sem) != 0) HANDLE_ERROR_MSG(eMsg::DESTROY_SEMAPHORE_ERR)
  }

  // Wait action function
  void wait() {
    if (sem_wait(&sem) != 0) HANDLE_ERROR_MSG(eMsg::WAIT_SEMAPHORE_ERR)
  }

  // Post action function
  void post() {
    if (sem_post(&sem) != 0) HANDLE_ERROR_MSG(eMsg::POST_SEMAPHORE_ERR)
  }

 private:
  sem_t sem;
};


/**
 * @brief A struct representing the context of a single job.
 */
struct JobContext {
    const MapReduceClient *client;
    int numberOfThreads;
    bool waitedFlag;

    const InputVec *inputData;
    OutputVec *outputData;
    std::vector<IntermediateVec *> *shuffledData;

    pthread_t *threads;
    ThreadInformation *threadsInformation;

    Semaphore statusSemaphore;
    Semaphore outputSemaphore;
    Semaphore reduceSemaphore;

    Barrier *barrier;

    std::atomic<uint64_t> state;
    std::atomic<int> mapStageCounter;
    std::atomic<int> intermediateStageCounter;
    std::atomic<int> shuffledStageCounter;
    std::atomic<int> reduceStageCounter;

    std::mutex stateMutex;
};
/**
 * @brief A struct representing the context of a single thread.
 */
struct ThreadInformation {
    JobContext *jobContext;
    unsigned int tid;
    IntermediateVec *intermediateInformation;
};

/**
 * @brief A struct representing the context of a single intermediate element.
 */
struct IntermediateInformation {
    ThreadInformation *threadInformation;
    JobContext *jobContext;
};

/**
 * @brief Releases memory allocated for the job context up to the point of atomic creation failure.
 * @param jobContext The job context.
 */
void releaseMemoryOnAtomicFailure(JobContext *jobContext) {
  delete[] jobContext->threads;
  delete[] jobContext->threadsInformation;
  delete jobContext;
}

/**
 * @brief Releases memory allocated for the job context up to the point of thread creation failure.
 * @param jobContext The job context.
 * @param threadsCreated The number of threads successfully created.
 */
void releaseMemoryOnThreadFailure(JobContext *jobContext, int threadsCreated) {
  for (int i = 0; i < threadsCreated; ++i) {
      pthread_cancel(jobContext->threads[i]);
    }
  delete jobContext;
}

/**
 * @brief Allocates memory for a new object of type T or handles an error.
 * @tparam T  The type of the object to allocate.
 * @param errorMessage  The error message to print in case of an error.
 * @return A pointer to the allocated object.
 */
template<typename T>
T *allocateOrHandleError(const std::string &errorMessage) {
  T *ptr = new(std::nothrow) T();
  if (!ptr) HANDLE_ERROR_MSG(errorMessage)
  return ptr;
}

/**
 * @brief Allocates memory for a new array of objects of type T or handles an error.
 * @tparam T  The type of the objects to allocate.
 * @param size  The size of the array to allocate.
 * @param errorMessage  The error message to print in case of an error.
 * @return  A pointer to the allocated array.
 */
template<typename T>
T *allocateArrayOrHandleError(size_t size, const std::string &errorMessage) {
  T *ptr = new T[size];
  if (!ptr) HANDLE_ERROR_MSG(errorMessage)
  return ptr;
}

/**
 * @brief Initializes the job context.
 * @param client  The client to use for the job.
 * @param inputVec  The input vector to use for the job.
 * @param outputVec  The output vector to use for the job.
 * @param multiThreadLevel  The number of threads to use for the job.
 * @return  The pointer to the initialized job context.
 */
JobContext *initializeJobContext(const MapReduceClient &client, const InputVec &inputVec, OutputVec &outputVec, int multiThreadLevel) {
  auto jobContext = allocateOrHandleError<JobContext>(eMsg::INIT_CONTEXT_ERR);

  jobContext->client = &client;
  jobContext->numberOfThreads = multiThreadLevel;
  jobContext->waitedFlag = false;

  jobContext->threads = allocateArrayOrHandleError<pthread_t>(multiThreadLevel, eMsg::INIT_THREADS_ERR);
  jobContext->threadsInformation = allocateArrayOrHandleError<ThreadInformation>(multiThreadLevel, eMsg::INIT_THREADS_CONTEXTS_ERR);

  jobContext->inputData = &inputVec;
  jobContext->shuffledData = allocateOrHandleError<std::vector<IntermediateVec *>>(eMsg::INIT_SHUFFLED_DATA_ERR);
  jobContext->outputData = &outputVec;

  jobContext->statusSemaphore.wait();
  jobContext->state = (static_cast<long>(inputVec.size()) << 31 | static_cast<long>(UNDEFINED_STAGE) << 62);
  jobContext->statusSemaphore.post();

  jobContext->mapStageCounter = 0;
  jobContext->intermediateStageCounter = 0;
  jobContext->shuffledStageCounter = 0;
  jobContext->reduceStageCounter = 0;

  jobContext->barrier = new(std::nothrow) Barrier(multiThreadLevel);
  if (!jobContext->barrier) {
      releaseMemoryOnAtomicFailure(jobContext);
      HANDLE_ERROR_MSG(eMsg::INIT_BARRIER_ERR);
    }
  return jobContext;
}

/**
 * @brief Initializes thread information.
 * @param jobContext The job context.
 * @param multiThreadLevel  The number of threads to use for the job.
 */
void initializeThreadInformation(JobContext *jobContext, int multiThreadLevel) {
  for (int i = 0; i < multiThreadLevel; ++i) {
      jobContext->threadsInformation[i].tid = i;
      jobContext->threadsInformation[i].jobContext = jobContext;
      jobContext->threadsInformation[i].intermediateInformation = new(std::nothrow) IntermediateVec();
      if (!jobContext->threadsInformation[i].intermediateInformation) {
          HANDLE_ERROR_MSG(eMsg::INIT_INTERMEDIATE_DATA_ERR);
        }
    }
}

/**
 * @brief Creates the threads for the job.
 * @param jobContext The job context.
 */
void createThreads(JobContext *jobContext) {
  for (int i = 0; i < jobContext->numberOfThreads; ++i) {
      if (pthread_create(jobContext->threads + i, nullptr, startThreadJobFramework, &jobContext->threadsInformation[i]) != 0) {
          releaseMemoryOnThreadFailure(jobContext, i);
          HANDLE_ERROR_MSG(eMsg::CREATE_THREAD_ERR);
        }
    }
}

/**
 * @brief Starts a new MapReduce job.
 * @param client  The client to use for the job.
 * @param inputVec  The input vector to use for the job.
 * @param outputVec  The output vector to use for the job.
 * @param multiThreadLevel  The number of threads to use for the job.
 * @return  The handle of the job.
 */
JobHandle startMapReduceJob(const MapReduceClient &client, const InputVec &inputVec, OutputVec &outputVec, int multiThreadLevel) {
  JobContext *jobContext = initializeJobContext(client, inputVec, outputVec, multiThreadLevel);
  initializeThreadInformation(jobContext, multiThreadLevel);
  createThreads(jobContext);

  {
    std::lock_guard<std::mutex> lock(jobContext->stateMutex);
    jobContext->state = (static_cast<long>(inputVec.size()) << 31 | static_cast<long>(MAP_STAGE) << 62);
  }

  return (JobHandle) jobContext;
}

/**
 * @brief The function that starts the job framework.
 * @param arg  The argument to pass to the function.
 * @return A pointer to the result of the function.
 */
void *startThreadJobFramework(void *arg) {
  auto *threadInformation = (ThreadInformation *) arg;
  JobContext *jobContext = threadInformation->jobContext;

  executeMapStage(jobContext, threadInformation);

  jobContext->barrier->barrier();

  if (threadInformation->tid == 0) {
      executeShuffleStage(jobContext);
    }
  jobContext->barrier->barrier();

  executeReduceStage(jobContext);
  return nullptr;
}

/**
 * @brief Executes the map stage of the job.
 * @param jobContext The context of the job.
 * @param threadInformation The information of the thread.
 */
void executeMapStage(JobContext *jobContext, ThreadInformation *threadInformation) {
  while (true) {
      int oldValue = jobContext->mapStageCounter.fetch_add(1);
      if (oldValue >= jobContext->inputData->size()) {
          break;
        }

      InputPair currentPair = jobContext->inputData->at(oldValue);
      jobContext->statusSemaphore.wait();

      auto *tempContext = new IntermediateInformation();
      tempContext->threadInformation = threadInformation;
      tempContext->jobContext = jobContext;

      jobContext->client->map(currentPair.first, currentPair.second, tempContext);
      delete tempContext;
      jobContext->statusSemaphore.post();

      {
        std::lock_guard<std::mutex> lock(jobContext->stateMutex);
        jobContext->state++; // update the state
      }
    }
  sortIntermediateData(threadInformation);
}

/**
 * @brief Sorts the intermediate data of a thread.
 * @param threadInformation The information of the thread.
 * @return The sorted intermediate data.
 */
void sortIntermediateData(ThreadInformation *threadInformation) {
  std::sort(threadInformation->intermediateInformation->begin(),
            threadInformation->intermediateInformation->end(),
            [](IntermediatePair &p1, IntermediatePair &p2) {
                return *p1.first < *p2.first; });
}

/**
 * @brief Executes the shuffle stage of the job.
 * @param jobContext The context of the job.
 * @return The shuffled data.
 */
void executeShuffleStage(JobContext *jobContext) {
  {
//    std::lock_guard<std::mutex> lock(jobContext->stateMutex);
    jobContext->statusSemaphore.wait();
    jobContext->state = (static_cast<long>(jobContext->intermediateStageCounter.load()) << 31 | static_cast<long> (SHUFFLE_STAGE) << 62);
    jobContext->statusSemaphore.post();
  }

  while (true) {
      K2 *max_key = getMaxKey(jobContext);

      if (max_key == nullptr) {
          break;
        }

      auto *key_data = getAllKeysByMatchingMaxKey(jobContext, max_key);

      jobContext->shuffledData->push_back(key_data);
      jobContext->shuffledStageCounter++;
    }

  {
    std::lock_guard<std::mutex> lock(jobContext->stateMutex);
    jobContext->state = static_cast<long>(jobContext->shuffledStageCounter.load()) << 31 | static_cast<long>(REDUCE_STAGE) << 62;
  }
}

/**
 * @brief Executes the reduce stage of the job.
 * @param jobContext The context of the job.
 */
void executeReduceStage(JobContext *jobContext) {
  while (true) {
      jobContext->reduceSemaphore.wait();

      if (jobContext->shuffledData->empty()) {
          jobContext->reduceSemaphore.post();
          break;
        }

      IntermediateVec *kv_pair_data = jobContext->shuffledData->back();
      jobContext->shuffledData->pop_back();

      jobContext->reduceSemaphore.post();

      jobContext->client->reduce(kv_pair_data, jobContext);
      delete kv_pair_data;
    }
}

/**
 * @brief Finds the maximum key in the job context.
 * @param jobContext The context of the job.
 * @return The maximum key.
 */
K2 *getMaxKey(JobContext *jobContext) {
  K2 *maximumKey = nullptr;
  for (int i = 0; i < jobContext->numberOfThreads; i++) {
      if (jobContext->threadsInformation[i].intermediateInformation->empty()) {
          continue;
        }
      K2 *currentKey = jobContext->threadsInformation[i].intermediateInformation->back().first;
      if (maximumKey == nullptr || *maximumKey < *currentKey) {
          maximumKey = currentKey;
        }
    }
  return maximumKey;
}

/**
 * @brief Groups the intermediate data by the maximum key.
 * @param jobContext The context of the job.
 * @param max_key The maximum key.
 * @return The grouped intermediate data.
 */
IntermediateVec *getAllKeysByMatchingMaxKey(JobContext *jobContext, K2 *max_key) {
  if (max_key == nullptr) {
      return nullptr;
    }

  auto *key_data = new IntermediateVec();

  for (int i = 0; i < jobContext->numberOfThreads; i++) {
      IntermediateVec *intermediateInformation = jobContext->threadsInformation[i].intermediateInformation;
      while (!intermediateInformation->empty() && !(*intermediateInformation->back().first < *max_key) &&
             !(*max_key < *intermediateInformation->back().first)) {
          key_data->push_back(intermediateInformation->back());
          intermediateInformation->pop_back();
          {
            std::lock_guard<std::mutex> lock(jobContext->stateMutex);
            jobContext->state++;
          }
        }
    }
  return key_data;
}

/**
 * @brief Emits a key-value pair to the intermediate data.
 * @param key The key to emit.
 * @param value The value to emit.
 * @param context The context of the job.
 * @return The emitted key-value pair.
 */
void emit2(K2 *key, V2 *value, void *context) {
  auto *intermediateContext = (IntermediateInformation *) context;
  intermediateContext->threadInformation->intermediateInformation->emplace_back(key, value);
  intermediateContext->jobContext->intermediateStageCounter++;
}

/**
 * @brief Emits a key-value pair to the output data.
 * @param key The key to emit.
 * @param value The value to emit.
 * @param context The context of the job.
 * @return The emitted key-value pair.
 */
void emit3(K3 *key, V3 *value, void *context) {
  auto *jobContext = (JobContext *) context;

  jobContext->outputSemaphore.wait();
  jobContext->outputData->emplace_back(key, value);
  jobContext->outputSemaphore.post();

  {
    std::lock_guard<std::mutex> lock(jobContext->stateMutex);
    jobContext->reduceStageCounter++;
    jobContext->state++;
  }
}

/**
 * @brief Waits for a job to finish.
 * @param job The job to wait for.
 * @return The waited job.
 */
void waitForJob(JobHandle job) {
  auto *jobContext = static_cast<JobContext *>(job);

  if (jobContext == nullptr || jobContext->waitedFlag) {
      return;
    }

  jobContext->waitedFlag = true;

  for (int i = 0; i < jobContext->numberOfThreads; i++) {
      if (pthread_join(jobContext->threads[i], nullptr) != 0) HANDLE_ERROR_MSG(eMsg::JOIN_THREAD_ERR)
    }
}

/**
 * @brief Gets the state of a job.
 * @param job The job to get the state of.
 * @param state The state of the job.
 * @return The state of the job.
 */
void getJobState(JobHandle job, JobState *state) {
  auto *jobContext = static_cast<JobContext *>(job);

  std::lock_guard<std::mutex> lock(jobContext->stateMutex);
  uint64_t state_value = jobContext->state.load();

  state->stage = static_cast<stage_t>(state_value >> 62);
  int total_tasks = (state_value >> 31) & 0x7FFFFFFF;
  int completed_tasks = state_value & 0x7FFFFFFF;

  if (total_tasks > 0) {
      state->percentage = std::min(100.0f, ((float) completed_tasks / total_tasks) * 100);
    } else {
      state->percentage = 0;
    }
}

/**
 * @brief Closes a job handle and frees its resources.
 * @param job The job to close.
 * @return nothing.
 */
void closeJobHandle(JobHandle job) {
  auto *jobContext = static_cast<JobContext *>(job);

  if (jobContext == nullptr) {
      return;
    }

  waitForJob(job);

  delete jobContext->barrier;

  for (int i = 0; i < jobContext->numberOfThreads; ++i) {
      delete jobContext->threadsInformation[i].intermediateInformation;
    }

  delete jobContext->shuffledData;
  delete[] jobContext->threadsInformation;
  delete[] jobContext->threads;

  delete jobContext;
  jobContext = nullptr;
}
