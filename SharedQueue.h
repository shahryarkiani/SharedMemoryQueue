#ifndef SHARED_QUEUE_H_
#define SHARED_QUEUE_H_

#include <string>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <array>
#include <mutex>

#include "Debug.h"

static const size_t QUEUE_SIZE = 4096;

struct SharedMemoryBlock {
    std::mutex mutex;
    std::array<char, QUEUE_SIZE - sizeof(std::mutex)> buffer;
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
        
        int fd = open(queueName.c_str(), O_CREAT | O_RDWR);
        if(!sharedFileExists) ftruncate(fd, QUEUE_SIZE);
        
        data = static_cast<SharedMemoryBlock*>(mmap(NULL, QUEUE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));

        if(!sharedFileExists) { // need to initialize mutex
            new (&data->mutex) std::mutex{};
            LogInfo("Shared Mutex Initialized");
        }

        close(fd);
    }

    ~SharedQueue() {
        LogInfo("Cleaning up shared memory");

        unlink(queueName.c_str());
        munmap(data, QUEUE_SIZE);
    }
};

class SharedQueueWriter : SharedQueue {
    
    public:
    SharedQueueWriter(std::string name) : SharedQueue(name) {}

    void writeVal() {
        data->mutex.lock();
        LogInfo("w locked");
        sleep(15);
        data->mutex.unlock();
        LogInfo("w unlocked");
    }
};

class SharedQueueReader : SharedQueue {

    public:
    SharedQueueReader(std::string name) : SharedQueue(name) {}

    void readVal() {
        LogInfo("waiting for lock");
        data->mutex.lock();
        LogInfo("r locked");
        data->mutex.unlock();
        LogInfo("r unlocked");
    }
};

#endif