// sentinel_scanner.cpp

#include "core/sentinel_scanner.h"
#include <stdexcept>
#include <utility>

SentinelScanner::SentinelScanner(std::string sentinel)
    : sentinel_(std::move(sentinel)) {
    if (sentinel_.empty()) {
        throw std::invalid_argument("sentinel cant be empty");
    }

    // build the KMP failure table
    // fail_[i] = longest proper prefix of sentinel_[0..i] thats also a suffix
    const std::size_t n = sentinel_.size();
    fail_.assign(n, 0);
    std::size_t k = 0;
    for (std::size_t i = 1; i < n; ++i) {
        while (k > 0 && sentinel_[i] != sentinel_[k]) k = fail_[k - 1];
        if (sentinel_[i] == sentinel_[k]) ++k;
        fail_[i] = k;
    }
    pending_.reserve(n);
}

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
    Out out{"", found_};
    if (found_) return out;
    out.safe_text.reserve(chunk.size());

    const std::size_t n = sentinel_.size();
    for (char c : chunk) {
        // pending_ is always sentinel_[0..q) so its length is the match state
        std::size_t q = pending_.size();

        // follow failure links until c extends the match or q = 0
        while (q > 0 && c != sentinel_[q]) q = fail_[q - 1];
        if (c == sentinel_[q]) ++q;

        if (q == n) {
            // whole sentinel matched, stop here
            pending_.clear();
            found_ = true;
            out.sentinel_found = true;
            return out;
        }

        // old pending_ + c has (old q + 1) chars, only the last q can still
        // be part of the sentinel, so everything before that is safe
        pending_.push_back(c);
        const std::size_t release = pending_.size() - q;
        if (release > 0) {
            out.safe_text.append(pending_, 0, release);
            pending_.erase(0, release);
        }
    }
    return out;
}

SentinelScanner::Out SentinelScanner::flush() {
    if (found_) return {"", true};
    Out out{std::move(pending_), false};
    pending_.clear();  // moved-from string isnt guaranteed empty
    return out;
}
