#ifndef SHARED_QUEUE_H_
#define SHARED_QUEUE_H_

#include <string>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <array>
#include <pthread.h>

#include "Debug.h"

static const size_t QUEUE_SIZE = 4096;

struct SharedMemoryBlock {
    pthread_mutex_t mutex;
    std::array<char, QUEUE_SIZE - sizeof(pthread_mutex_t)> buffer;
};

static_assert(sizeof(SharedMemoryBlock) == QUEUE_SIZE,
              "SharedMemoryBlock must be exactly QUEUE_SIZE bytes");

class SharedQueue {
protected:
    
    std::string queueName;
    SharedMemoryBlock* data; // ptr to shared memory

    SharedQueue(std::string name) : queueName("/dev/shm/" + name), data(nullptr) {
        LogInfo("Setting up shared memory");


        bool sharedFileExists = access(queueName.c_str(), F_OK) == 0;
        if(sharedFileExists) LogInfo("File already exists");
        // TODO: race condition on opening here
        int fd = open(queueName.c_str(), O_CREAT | O_RDWR, 0666);
        if(fd == -1) {
            LogError("Failed open");
        }
        if(!sharedFileExists) ftruncate(fd, QUEUE_SIZE);
        
        data = static_cast<SharedMemoryBlock*>(mmap(NULL, QUEUE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
        if (data == MAP_FAILED) {
            LogError("Failed MMAP");
        }
        if(!sharedFileExists) { // need to initialize mutex
            pthread_mutexattr_t attr;
            pthread_mutexattr_init(&attr);
            pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
            
            pthread_mutex_init(&data->mutex, &attr);
            pthread_mutexattr_destroy(&attr);
            LogInfo("Shared Mutex Initialized");
        }

        close(fd);
    }

    ~SharedQueue() {
        LogInfo("Cleaning up shared memory");
        munmap(data, QUEUE_SIZE);
        unlink(queueName.c_str());
    }
};

class SharedQueueWriter : SharedQueue {
    
    public:
    SharedQueueWriter(std::string name) : SharedQueue(name) {}

    void writeVal() {
        pthread_mutex_lock(&data->mutex);
        LogInfo("w locked");
        sleep(8);
        pthread_mutex_unlock(&data->mutex);
        LogInfo("w unlocked");
    }
};

class SharedQueueReader : SharedQueue {

    public:
    SharedQueueReader(std::string name) : SharedQueue(name) {}

    void readVal() {
        LogInfo("waiting for lock");
        pthread_mutex_lock(&data->mutex);
        LogInfo("r locked");
        pthread_mutex_unlock(&data->mutex);
        LogInfo("r unlocked");
    }
};

#endif