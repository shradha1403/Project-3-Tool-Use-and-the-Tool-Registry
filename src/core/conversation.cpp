// conversation.cpp

#include "core/conversation.h"
#include <stdexcept>
#include <string>
#include <utility>

Conversation::~Conversation() {
    delete[] data_;  // fine even if nullptr (moved-from / empty)
}

Conversation::Conversation(const Conversation& other) {
    if (other.size_ == 0) return;  // nothing to copy, dont allocate

    // copy only needs size_ slots, capacity doesnt have to match
    Message* fresh = new Message[other.size_];
    try {
        for (std::size_t i = 0; i < other.size_; ++i) {
            fresh[i] = other.data_[i];  // string copy could throw bad_alloc
        }
    } catch (...) {
        delete[] fresh;  // dont leak the half built buffer
        throw;
    }
    data_ = fresh;
    size_ = other.size_;
    capacity_ = other.size_;
}

Conversation& Conversation::operator=(const Conversation& other) {
    // copy-and-swap: make the copy first, then swap it in
    // if the copy throws, *this doesnt change
    if (this != &other) {
        Conversation tmp(other);
        swap(tmp);
    }  // tmp has our old buffer now and frees it here
    return *this;
}

Conversation::Conversation(Conversation&& other) noexcept
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}

Conversation& Conversation::operator=(Conversation&& other) noexcept {
    if (this != &other) {
        delete[] data_;  // free what we had first
        data_ = other.data_;
        size_ = other.size_;
        capacity_ = other.capacity_;
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }
    return *this;
}

void Conversation::swap(Conversation& other) noexcept {
    std::swap(data_, other.data_);
    std::swap(size_, other.size_);
    std::swap(capacity_, other.capacity_);
}

void Conversation::reallocate(std::size_t new_capacity) {
    Message* fresh = new Message[new_capacity];  // if this throws nothing changed yet
    // moving a Message is noexcept (enum + string) so this cant fail midway
    for (std::size_t i = 0; i < size_; ++i) {
        fresh[i] = std::move(data_[i]);
    }
    delete[] data_;
    data_ = fresh;
    capacity_ = new_capacity;
}

void Conversation::append(Message m) {
    if (m.role() == Role::System && size_ != 0) {
        throw std::logic_error("system message has to be first");
    }
    if (size_ == capacity_) {
        reallocate(capacity_ == 0 ? kInitialCapacity : capacity_ * kGrowthFactor);
    }
    data_[size_] = std::move(m);
    ++size_;
}

const Message& Conversation::at(std::size_t i) const {
    if (i >= size_) {
        throw std::out_of_range("at(): index " + std::to_string(i) +
                                " but size is " + std::to_string(size_));
    }
    return data_[i];
}
