#include "modern.hpp"

#include <cstdio>
#include <utility>

namespace {
int checks = 0;
int failures = 0;

void check(bool condition, const char* name) {
    ++checks;
    if (!condition) {
        ++failures;
        std::printf("  FAIL: %s\n", name);
    }
}

void test_array_queue() {
    dsa::ArrayQueue<int> queue(3);
    check(queue.empty() && !queue.dequeue(), "代码3.13 empty queue is optional");
    check(queue.front() == nullptr && queue.size() == 0, "代码3.13 empty front and size");
    check(queue.enqueue(1) && queue.enqueue(2) && queue.enqueue(3),
          "代码3.14 fills usable capacity");
    check(queue.full() && queue.size() == 3, "代码3.14 sacrificed slot detects full");
    check(!queue.enqueue(4) && queue.size() == 3, "代码3.14 full enqueue preserves size");
    check(queue.dequeue() == 1 && queue.enqueue(4), "代码3.14 first wrap preparation");
    check(queue.dequeue() == 2 && queue.dequeue() == 3 && queue.dequeue() == 4,
          "代码3.13 FIFO across wrap");
    check(queue.empty() && queue.front() == nullptr, "代码3.13 empty boundary after drain");

    for (int round = 0; round < 9; ++round) {
        check(queue.enqueue(round), "代码3.14 enqueue repeatedly wraps rear");
        check(queue.dequeue() == round, "代码3.14 dequeue repeatedly wraps front");
    }
    check(queue.empty() && queue.size() == 0, "代码3.14 a full ring returns to empty");

    dsa::ArrayQueue<int> source(2);
    check(source.enqueue(7) && source.enqueue(8), "代码3.13 array copy source setup");
    auto copy = source;
    check(copy.dequeue() == 7 && source.front() && *source.front() == 7,
          "代码3.13 array copy is deep");
    dsa::ArrayQueue<int>& alias = copy;
    copy = alias;
    check(copy.dequeue() == 8, "代码3.13 array self assignment");
}

void test_linked_queue() {
    dsa::LinkedQueue<int> source;
    source.enqueue(1);
    source.enqueue(2);
    auto copy = source;
    check(copy.dequeue() == 1 && source.front() && *source.front() == 1,
          "代码3.15 linked copy is deep");
    copy.enqueue(3);
    check(copy.dequeue() == 2 && copy.dequeue() == 3, "代码3.15 copied queue is independent");
    check(source.dequeue() == 1 && source.dequeue() == 2, "代码3.15 source survives copied mutation");
    check(source.empty() && !source.dequeue() && source.front() == nullptr,
          "代码3.15 linked empty boundary");
    source.enqueue(4);
    source.clear();
    check(source.empty() && source.size() == 0 && source.front() == nullptr,
          "代码3.15 clear restores empty endpoints");
    dsa::LinkedQueue<int> moved = std::move(copy);
    check(copy.empty() && moved.empty(), "代码3.15 move transfers ownership");
}
}  // namespace

int main() {
    test_array_queue();
    test_linked_queue();
    std::printf("Queue: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
