#pragma once
// Only the PRIVATE section is yours to change; the public API is fixed by the spec.
#include <cstddef>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

#include "tools/tool.h"

class ToolRegistry {
public:
    ToolRegistry();
    ~ToolRegistry();
    ToolRegistry(const ToolRegistry&) = delete;
    ToolRegistry& operator=(const ToolRegistry&) = delete;

    bool add(std::unique_ptr<Tool> tool);
    Tool* find(std::string_view name) const;
    std::size_t size() const noexcept;
    void for_each(const std::function<void(const Tool&)>& fn) const;

    // ---- Instrumentation (for tests and your design log) ----
    std::size_t bucket_count() const noexcept;
    double      load_factor() const noexcept;
    std::size_t max_probe_length() const;
    static std::size_t hash(std::string_view key) noexcept;

private:
    // Separate chaining: each bucket is a vector of owning pointers.
    // Moving a unique_ptr during rehash never moves the Tool itself,
    // so Tool* values handed out by find() stay valid.
    static constexpr std::size_t kInitialBuckets = 16;   // power of two

    void rehash(std::size_t new_bucket_count);

    std::vector<std::vector<std::unique_ptr<Tool>>> buckets_;
    std::size_t size_ = 0;
};
