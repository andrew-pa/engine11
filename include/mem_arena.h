#pragma once
#include <cstddef>
#include <vector>

template<typename T>
class arena {
    size_t          next_item = 0, items_per_block;
    T*              current_block;
    std::vector<T*> used_blocks;

    void new_block() {
        used_blocks.push_back(current_block);
        current_block = new T[items_per_block];
        next_item     = 0;
    }

  public:
    arena(size_t items_per_block = 16) : items_per_block(items_per_block), current_block(nullptr) {
        current_block = new T[items_per_block];
    }

    T* alloc(T&& init = T()) {
        if(next_item >= items_per_block) new_block();
        return new(&current_block[next_item++]) T(init);
    }

    T* alloc_array(size_t count) {
        T* p = current_block + next_item;
        if(next_item + count > items_per_block) {
            p = new T[count];
            used_blocks.push_back(p);
        } else {
            next_item += count;
        }
        return p;
    }

    ~arena() {
        for(T* b : used_blocks)
            delete b;
        delete current_block;
    }
};
