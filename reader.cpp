#include "SharedQueue.h"
#include <string>

int main() {
    SharedQueueReader<int> queue("abc");
    
    int received = queue.pop();

    while(received != 10000000) {
        received = queue.pop();
    }
}