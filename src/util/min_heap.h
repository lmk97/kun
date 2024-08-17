#ifndef KUN_UTIL_MIN_HEAP_H
#define KUN_UTIL_MIN_HEAP_H

#include <stddef.h>

#include <vector>

namespace kun {

template<typename T, typename U>
class MinHeapData {
public:
    MinHeapData(T* t, U u) : ptr(t), num(u) {}

    bool operator<(const MinHeapData& data) const {
        return this->num < data.num;
    }

    bool operator<=(const MinHeapData& data) const {
        return this->num <= data.num;
    }

    bool operator>(const MinHeapData& data) const {
        return this->num > data.num;
    }

    bool operator>=(const MinHeapData& data) const {
        return this->num >= data.num;
    }

    T* ptr;
    U num;
};

template<typename T, typename U>
class MinHeap {
public:
    MinHeap(size_t capacity) {
        datas.reserve(capacity);
    }

    size_t size() const {
        return datas.size();
    }

    bool empty() const {
        return datas.empty();
    }

    void push(T* ptr, U num) {
        datas.emplace_back(ptr, num);
        shiftUp();
    }

    MinHeapData<T, U> pop() {
        auto data = datas[0];
        datas[0] = datas.back();
        datas.pop_back();
        shiftDown();
        return data;
    }

    MinHeapData<T, U> peek() const {
        return datas[0];
    }

private:
    void shiftUp() {
        size_t i = datas.size() - 1;
        auto hole = datas[i];
        while (i > 0) {
            size_t j = (i - 1) >> 1;
            const auto& parent = datas[j];
            if (hole >= parent) {
                break;
            }
            datas[i] = parent;
            i = j;
        }
        datas[i] = hole;
    }

    void shiftDown() {
        size_t i = 0;
        size_t j = (i << 1) + 1;
        size_t size = datas.size();
        auto hole = datas[i];
        while (j < size) {
            auto k = j + 1;
            if (k < size && datas[j] > datas[k]) {
                j = k;
            }
            const auto& child = datas[j];
            if (hole <= child) {
                break;
            }
            datas[i] = child;
            i = j;
            j = (i << 1) + 1;
        }
        datas[i] = hole;
    }

    std::vector<MinHeapData<T, U>> datas;
};

}

#endif
