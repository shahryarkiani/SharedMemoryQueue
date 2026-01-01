#include "SharedQueue.h"
#include <string>

int main() {
    SpinQueueWriter<int> queue("abc");
    
    for(int i = 0; i < 100000001; i++) {
        queue.push(i);
    }
}