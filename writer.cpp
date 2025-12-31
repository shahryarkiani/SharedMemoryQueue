#include "SharedQueue.h"
#include <string>

int main() {
    SharedQueueWriter<int> queue("abc");
    
    
    for(int i = 0; i < 10000001; i++) {
        queue.push(i);
    }
}