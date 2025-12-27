#include "SharedQueue.h"
#include <string>

int main() {
    SharedQueueReader queue("abc");
    queue.readVal();
}