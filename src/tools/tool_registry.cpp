#include "tools/tool_registry.h"

#include <cstdint>
#include <utility>

// FNV-1a, 64-bit. Uses every byte, order-sensitive, returns the full-width
// value (the table reduces it to an index separately).
std::size_t ToolRegistry::hash(std::string_view key) noexcept {
    std::uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : key) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return static_cast<std::size_t>(h);
}

ToolRegistry::ToolRegistry() : buckets_(kInitialBuckets) {}

ToolRegistry::~ToolRegistry() = default;   // unique_ptrs destroy every tool

std::size_t ToolRegistry::size() const noexcept { return size_; }

std::size_t ToolRegistry::bucket_count() const noexcept { return buckets_.size(); }

double ToolRegistry::load_factor() const noexcept {
    return static_cast<double>(size_) / static_cast<double>(buckets_.size());
}

Tool* ToolRegistry::find(std::string_view name) const {
    const auto& chain = buckets_[hash(name) & (buckets_.size() - 1)];
    for (const auto& t : chain) {
        if (t->name() == name) return t.get();
    }
    return nullptr;
}

void ToolRegistry::rehash(std::size_t new_bucket_count) {
    std::vector<std::vector<std::unique_ptr<Tool>>> fresh(new_bucket_count);
    for (auto& chain : buckets_) {
        for (auto& t : chain) {
            // Recompute the index with the NEW bucket count. Only the
            // unique_ptr moves; the Tool object stays where it is.
            std::size_t idx = hash(t->name()) & (new_bucket_count - 1);
            fresh[idx].push_back(std::move(t));
        }
    }
    buckets_ = std::move(fresh);
}

bool ToolRegistry::add(std::unique_ptr<Tool> tool) {
    if (!tool) return false;
    const std::string key = tool->name();
    if (find(key) != nullptr) return false;   // duplicate: new tool destroyed on return

    // Check BEFORE inserting so load_factor() never exceeds 1.0 (chaining threshold 1.0).
    if (size_ + 1 > buckets_.size()) rehash(buckets_.size() * 2);

    buckets_[hash(key) & (buckets_.size() - 1)].push_back(std::move(tool));
    ++size_;
    return true;
}

void ToolRegistry::for_each(const std::function<void(const Tool&)>& fn) const {
    for (const auto& chain : buckets_)
        for (const auto& t : chain) fn(*t);
}

std::size_t ToolRegistry::max_probe_length() const {
    std::size_t longest = 0;
    for (const auto& chain : buckets_)
        if (chain.size() > longest) longest = chain.size();
    return longest;
}
