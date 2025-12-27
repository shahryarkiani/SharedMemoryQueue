#include "SharedQueue.h"
#include <string>

int main() {
    SharedQueueWriter queue("abc");
    queue.writeVal();
}