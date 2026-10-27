// conversation.h
// my own growable array for Messages (no std::vector allowed)
// this + conversation.cpp are the only places using raw new/delete
//
// growth: 0 -> 4 -> 8 -> 16 ... (start at 4, then double)
// amortized O(1) proof is in docs/design-log-p2.md

#pragma once
#include "core/message.h"
#include <cstddef>

class Conversation {
public:
    // public so the tests can check growth against them
    static constexpr std::size_t kInitialCapacity = 4;
    static constexpr std::size_t kGrowthFactor    = 2;

    Conversation() noexcept = default;  // empty, nothing allocated yet
    ~Conversation();

    // deep copy, gets its own buffer
    Conversation(const Conversation& other);
    Conversation& operator=(const Conversation& other);

    // steals the buffer, other ends up empty (nullptr/0/0)
    Conversation(Conversation&& other) noexcept;
    Conversation& operator=(Conversation&& other) noexcept;

    // adds m to the end, grows if full. amortized O(1)
    // throws logic_error if you try to add a System msg when it's not
    // the first one, that way the system prompt always stays at index 0
    void append(Message m);

    std::size_t size() const noexcept { return size_; }

    // not in the required interface, added it so i can test growth
    std::size_t capacity() const noexcept { return capacity_; }

    // throws out_of_range if i >= size()
    const Message& at(std::size_t i) const;

    // for range-for, oldest first. begin() == end() when empty
    const Message* begin() const noexcept { return data_; }
    const Message* end()   const noexcept { return data_ + size_; }

private:
    void swap(Conversation& other) noexcept;         // for copy-and-swap
    void reallocate(std::size_t new_capacity);       // moves msgs to a bigger buffer

    Message*    data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};
