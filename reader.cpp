#include "SharedQueue.h"
#include <string>
#include <chrono>
#include <iostream>

int main() {
    SpinQueueReader<int> queue("abc");
    int received = 0;

    auto start = std::chrono::high_resolution_clock::now();

    while(received != 100000000) {
        received = queue.pop();
    }

    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> diff = end - start;

    std::cout << "Time taken: " << diff.count() << " seconds" << std::endl;
    std::cout << "Throughput: " << (100000000 / diff.count()) << " ops/sec" << std::endl;

    return 0;
}