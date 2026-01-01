#ifndef SHARED_QUEUE_H_
#define SHARED_QUEUE_H_

#include <string>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <array>
#include <pthread.h>
#include <atomic>
#include "Debug.h"

static const size_t QUEUE_SIZE = 4096 * 8;

template<typename T>
struct SpinQueue {
    struct {
        alignas(64) int64_t readIdx;
        alignas(64) int64_t localReadCount;
        alignas(64) std::atomic_int64_t count;
        alignas(64) int64_t writeIdx;
    } control;

    static const int capacity = (QUEUE_SIZE - sizeof(control)) / sizeof(T);

    std::array<T, capacity> buffer;

    void init() {
        control.readIdx = 0;
        control.localReadCount = 0;
        control.writeIdx = 0;
        control.count.store(0);
        for(auto& data : buffer) data = 0;
    }

    void push(T val) {
        while(control.count.load(std::memory_order_acquire) == capacity) {}

        buffer[control.writeIdx] = std::move(val);
        control.writeIdx = (control.writeIdx + 1) % capacity;
        control.count.fetch_add(1, std::memory_order_release);
    }

    T pop() {
        // while (control.localReadCount == 0) {
        //     control.localReadCount = control.count.load(std::memory_order_relaxed);
        // }
        while (control.count.load(std::memory_order_acquire) == 0) {}
        T retVal = std::move(buffer[control.readIdx]);
        control.readIdx = (control.readIdx + 1) % capacity;
        // control.localReadCount--;
        control.count.fetch_sub(1, std::memory_order_release);

        return retVal;
    }
};

// Alternate Implementation, very similar/slighty slower performance
// template<typename T>
// struct SpinQueue {
//     struct {
//         alignas(64) std::atomic_uint64_t readIdx;
//         alignas(64) std::atomic_uint64_t writeIdx;
//         alignas(64) uint64_t localReadIdx;
//         alignas(64) uint64_t localWriteIdx;
//     } control;

//     static const int capacity = (QUEUE_SIZE - sizeof(control)) / sizeof(T);

//     std::array<T, capacity> buffer;

//     void init() {
//         control.readIdx.store(0);
//         control.writeIdx.store(0);
//         control.localReadIdx = 0;
//         control.localWriteIdx = 0;
//         for(auto& data : buffer) data = 0;
//     }

//     void push(T val) {
//         uint64_t localWrite = control.writeIdx.load(std::memory_order_acquire);
//         while((localWrite - control.localReadIdx) == capacity) {
//             control.localReadIdx = control.readIdx.load(std::memory_order_acquire);
//         }

//         buffer[localWrite % capacity] = std::move(val);
//         control.writeIdx.fetch_add(1, std::memory_order_release);
//     }

//     T pop() {
//         uint64_t localRead = control.readIdx.load(std::memory_order_acquire);
//         while (localRead == control.localWriteIdx) {
//             control.localWriteIdx = control.writeIdx.load(std::memory_order_acquire);
//         }
//         T retVal = std::move(buffer[localRead % capacity]);
//         control.readIdx.fetch_add(1, std::memory_order_release);

//         return retVal;
//     }
// };

static_assert(sizeof(SpinQueue<char>) == QUEUE_SIZE,
            "SpinQueue must be exactly QUEUE_SIZE bytes");


template<typename T>
struct BlockingQueue {
    struct {
        pthread_mutex_t mutex;
        pthread_cond_t condVar;
        alignas(64) int readIdx;
        alignas(64) int insertIdx;     
    } control;

    static const int capacity = (QUEUE_SIZE - sizeof(control)) / sizeof(T);

    std::array<T, capacity> buffer;


    void init() {
        pthread_mutexattr_t mutexAttr;
        pthread_mutexattr_init(&mutexAttr);
        pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);
        
        pthread_mutex_init(&control.mutex, &mutexAttr);
        pthread_mutexattr_destroy(&mutexAttr);

        pthread_condattr_t condAttr;
        pthread_condattr_init(&condAttr);
        pthread_condattr_setpshared(&condAttr, PTHREAD_PROCESS_SHARED);

        pthread_cond_init(&control.condVar, &condAttr);
        pthread_condattr_destroy(&condAttr);

        LogInfo("Shared Mutex and Condition Variable Initialized");
        
        control.readIdx = -1;
        control.insertIdx = 0;

        for(auto& data : buffer) {
            data = 0;
        }
    }

    bool isFull() {
        return control.readIdx == control.insertIdx;
    }

    bool isEmpty() {
        return control.readIdx == -1;
    }

    void push(T val) {
        pthread_mutex_lock(&control.mutex);
        
        while(isFull()) { // need to wait until a read happens
            pthread_cond_wait(&control.condVar, &control.mutex);
        }

        bool wasEmpty = isEmpty();

        buffer[control.insertIdx] = std::move(val);
        if(control.readIdx == -1) control.readIdx = 0;

        control.insertIdx = (control.insertIdx + 1) % capacity;

        pthread_mutex_unlock(&control.mutex);
        if(wasEmpty) pthread_cond_signal(&control.condVar);
    }

    T pop() {
        pthread_mutex_lock(&control.mutex);

        while(isEmpty()) { // need to wait until a write happens
            pthread_cond_wait(&control.condVar, &control.mutex);
        }

        bool wasFull = isFull();

        T returnVal = std::move(buffer[control.readIdx]);
        
        control.readIdx = (control.readIdx + 1) % capacity;

        if(isFull()) { // shouldn't be full after a read, so that means the queue is actually empty
            control.readIdx = -1;
            control.insertIdx = 0;
        }

        pthread_mutex_unlock(&control.mutex);
        
        if(wasFull) pthread_cond_signal(&control.condVar);

        return returnVal;
    }
};


static_assert(sizeof(BlockingQueue<char>) == QUEUE_SIZE,
            "BlockingQueue must be exactly QUEUE_SIZE bytes");


template<typename T, template<typename> class BackingQueue>
class SharedQueue {
protected:
    
    std::string queueName;
    BackingQueue<T>* data; // ptr to shared memory

    SharedQueue(std::string name) : queueName("/dev/shm/" + name), data(nullptr) {
        LogInfo("Setting up shared memory");


        bool sharedFileExists = access(queueName.c_str(), F_OK) == 0;
        if(sharedFileExists) LogInfo("File already exists");
        // TODO: race condition on opening here
        int fd = open(queueName.c_str(), O_CREAT | O_RDWR, 0666);
        if(fd == -1) {
            LogError("Failed open");
        }
        ftruncate(fd, QUEUE_SIZE); // safe to call from multiple processes
        
        data = static_cast<BackingQueue<T>*>(mmap(NULL, QUEUE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
        if (data == MAP_FAILED) {
            LogError("Failed MMAP");
        }
        if(!sharedFileExists) { 
            // race condition here too, non initializing process might try locking before its initialized
            data->init();
        }

        close(fd);
    }

    ~SharedQueue() {
        LogInfo("Cleaning up shared memory");
        munmap(data, QUEUE_SIZE);
        // unlink(queueName.c_str());
    }
};

template<typename T>
class BlockingQueueWriter : SharedQueue<T, BlockingQueue> {
    
    public:
    BlockingQueueWriter(std::string name) : SharedQueue<T, BlockingQueue>(name) {}

    void push(T val) {
        this->data->push(val);
    }
};

template<typename T>
class BlockingQueueReader : SharedQueue<T, BlockingQueue> {

    public:
    BlockingQueueReader(std::string name) : SharedQueue<T, BlockingQueue>(name) {}

    T pop() {
        return this->data->pop();
    }
};

template<typename T>
class SpinQueueWriter : SharedQueue<T, SpinQueue> {
    
    public:
    SpinQueueWriter(std::string name) : SharedQueue<T, SpinQueue>(name) {}

    void push(T val) {
        this->data->push(val);
    }
};

template<typename T>
class SpinQueueReader : SharedQueue<T, SpinQueue> {

    public:
    SpinQueueReader(std::string name) : SharedQueue<T, SpinQueue>(name) {}

    T pop() {
        return this->data->pop();
    }
};

#endif