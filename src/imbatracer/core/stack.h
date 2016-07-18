#ifndef STACK_H
#define STACK_H

#include <cassert>

namespace imba {

template <typename T, int N = 128>
struct Stack {
    static constexpr int capacity() { return N; }

    T elems[N];
    int top;

    Stack() : top(-1) {}

    template <typename... Args>
    void push(Args... args) {
        assert(!full());
        elems[++top] = T(args...);
    }

    T pop() {
        assert(!is_empty());
        return elems[top--];
    }

    bool is_empty() const { return top < 0; }
    bool full() const { return top >= N - 1; }
    int size() const { return top + 1; }
};

} // namespace imba

#endif
