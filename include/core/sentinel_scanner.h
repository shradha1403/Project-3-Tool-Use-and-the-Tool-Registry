// sentinel_scanner.h
// finds the stop sentinel in a stream that comes in random sized chunks
//
// went with KMP (the stretch goal) instead of re-searching pending_+chunk
// every time. goes one char at a time so each char is amortized O(1)
//
// invariant: pending_ is always a proper prefix of the sentinel,
// so pending_.size() <= sentinel_.size() - 1 (proof in the design log)

#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

class SentinelScanner {
public:
    // throws invalid_argument if sentinel is empty
    explicit SentinelScanner(std::string sentinel);

    struct Out { std::string safe_text; bool sentinel_found; };

    // returns the text thats safe to print + whether we hit the sentinel.
    // anything after the sentinel gets dropped, and once its found every
    // feed after that just returns {"", true}
    Out feed(std::string_view chunk);

    // call at the end of the stream to get back whatever was held
    Out flush();

    // for testing (not in the required interface)
    std::size_t pending_size() const noexcept { return pending_.size(); }
    bool found() const noexcept { return found_; }

private:
    std::string sentinel_;
    std::string pending_;            // at most sentinel_.size() - 1 chars
    std::vector<std::size_t> fail_;  // KMP failure table
    bool found_ = false;
};
