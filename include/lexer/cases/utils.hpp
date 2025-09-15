//
// Created by aowei on 2025 9月 15.
//

#ifndef POCOM_UTILS_HPP
#define POCOM_UTILS_HPP

namespace lexer::cases {
    class StateIdCounter {
    public:
        StateIdCounter() : counter(0) {}

        StateIdCounter(const StateIdCounter &) = delete;

        StateIdCounter &operator =(const StateIdCounter &) = delete;

        static StateIdCounter *get_instance() {
            if (instance == nullptr) { instance = new StateIdCounter(); }
            return instance;
        }

        int get_nextid() { return this->counter++; }

    private:
        int counter;
        static StateIdCounter *instance;
    };
}
#endif //POCOM_UTILS_HPP
