// tests/p3/test_p3.cpp — P3 test suite.
//
// Build & run:   cmake --build build && ./build/test_p3
// One area:      ./build/test_p3 reg_        (names starting with "reg_")
// One test:      ./build/test_p3 =reg_add_and_find

#include "../mini_test.h"
#include "../test_helpers.h"     // TempDir, write_text, args1, ...

#include "tools/calculator.h"
#include "tools/tool_registry.h"
#include "tools/workspace.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// A tool whose name you choose — use it to fill the registry in tests.
class NamedTool : public Tool {
public:
    explicit NamedTool(std::string n) : n_(std::move(n)), schema_(JsonValue::object()) {}
    std::string name() const override { return n_; }
    std::string description() const override { return "test tool"; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue&, ToolContext&) override { return ToolResult::success(n_); }
private:
    std::string n_;
    JsonValue   schema_;   // an empty JSON object: {}
};

// Tokenize with the provided tokenizer, then run YOUR calc_evaluate.
bool eval(const std::string& expr, double* value, std::string* error) {
    std::vector<CalcToken> tokens;
    if (!calc_tokenize(expr, &tokens, error)) return false;
    return calc_evaluate(tokens, value, error);
}

}  // namespace

// ------------------------------------------------------------ registry

TEST(reg_add_and_find) {
    ToolRegistry r;
    auto t = std::make_unique<NamedTool>("alpha");
    Tool* raw = t.get();                        // remember the address before moving
    REQUIRE(r.add(std::move(t)));
    CHECK(r.size() == 1);
    CHECK(r.find("alpha") == raw);              // same object, not a copy
    CHECK(r.find("beta") == nullptr);
}

TEST(reg_duplicate_keeps_original) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("alpha");
    Tool* original = first.get();
    REQUIRE(r.add(std::move(first)));
    CHECK(!r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(r.find("alpha") == original);
    CHECK(r.size() == 1);
}

TEST(reg_null_rejected) {
    ToolRegistry r;
    CHECK(!r.add(nullptr));
    CHECK(r.size() == 0);
    r.add(std::make_unique<NamedTool>("a"));
    CHECK(!r.add(nullptr));
    CHECK(r.size() == 1);
}

TEST(reg_find_absent_and_empty_table) {
    ToolRegistry r;
    CHECK(r.find("anything") == nullptr);
    CHECK(r.max_probe_length() == 0);
    CHECK(r.bucket_count() >= 1 && r.bucket_count() <= 64);
    r.add(std::make_unique<NamedTool>("a"));
    CHECK(r.find("teleport") == nullptr);
    CHECK(r.find("") == nullptr);
}

TEST(reg_for_each_visits_each_once) {
    ToolRegistry r;
    for (const char* n : {"a", "b", "c", "d", "e"}) REQUIRE(r.add(std::make_unique<NamedTool>(n)));
    int count = 0;
    std::vector<std::string> seen;
    r.for_each([&](const Tool& t) { ++count; seen.push_back(t.name()); });
    CHECK(count == 5);
    for (const char* n : {"a", "b", "c", "d", "e"}) {
        int times = 0;
        for (const auto& s : seen) if (s == n) ++times;
        CHECK(times == 1);
    }
}

TEST(reg_hash_order_sensitive_and_uses_every_char) {
    CHECK(ToolRegistry::hash("ab") != ToolRegistry::hash("ba"));
    CHECK(ToolRegistry::hash("tool_1234") != ToolRegistry::hash("tool_1243"));
    CHECK(ToolRegistry::hash("abcdef") != ToolRegistry::hash("abcdeg"));
}

TEST(reg_10000_tools) {
    ToolRegistry r;
    Tool* first = nullptr;
    for (int i = 0; i < 10000; ++i) {
        REQUIRE(r.add(std::make_unique<NamedTool>("tool_" + std::to_string(i))));
        if (i == 0) first = r.find("tool_0");
        CHECK(r.load_factor() <= 1.0);
    }
    CHECK(r.size() == 10000);
    CHECK(first != nullptr);
    CHECK(r.find("tool_0") == first);           // pointer stable across every resize
    for (int i = 0; i < 10000; ++i) CHECK(r.find("tool_" + std::to_string(i)) != nullptr);
    CHECK(r.find("tool_10000") == nullptr);
    CHECK(r.max_probe_length() <= 64);
    std::printf("  [10000 tools] bucket_count=%zu load_factor=%.3f max_probe_length=%zu\n",
                r.bucket_count(), r.load_factor(), r.max_probe_length());
}

TEST(reg_alternative_hash) {
    ToolRegistry r;
    for (int i = 0; i < 10000; ++i) REQUIRE(r.add(std::make_unique<NamedTool>("tool_" + std::to_string(i))));
    const std::size_t B = r.bucket_count();
    std::vector<int> weak(B, 0);
    for (int i = 0; i < 10000; ++i) {
        std::string s = "tool_" + std::to_string(i);
        std::size_t h = static_cast<unsigned char>(s[0]) + s.size();   // the weak hash
        ++weak[h % B];
    }
    int longest = 0;
    for (int c : weak) if (c > longest) longest = c;
    std::printf("  [alt hash] real FNV-1a max chain=%zu vs weak s[0]+size max chain=%d (buckets=%zu)\n",
                r.max_probe_length(), longest, B);
    CHECK(longest > 1000);
    CHECK(r.max_probe_length() < static_cast<std::size_t>(longest));
}

// ----------------------------------------------------------- workspace

TEST(ws_accepts_plain_and_normalized) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string why;
    REQUIRE(ws.resolve("notes/todo.txt", &out, &why));
    CHECK(out == ws.root() / "notes" / "todo.txt");
    REQUIRE(ws.resolve("./notes//todo.txt", &out, &why));
    CHECK(out == ws.root() / "notes" / "todo.txt");
    REQUIRE(ws.resolve("a/../b", &out, &why));
    CHECK(out == ws.root() / "b");
    REQUIRE(ws.resolve("new/dir/file.txt", &out, &why));      // does not exist yet
    CHECK(out == ws.root() / "new" / "dir" / "file.txt");
    REQUIRE(ws.resolve("my..notes.txt", &out, &why));
    CHECK(out == ws.root() / "my..notes.txt");
}

TEST(ws_dot_and_dotpair_map_to_root) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string why;
    REQUIRE(ws.resolve(".", &out, &why));
    CHECK(out == ws.root());
    REQUIRE(ws.resolve("a/..", &out, &why));
    CHECK(out == ws.root());
}

TEST(ws_rejects_empty_and_nul) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string why;
    CHECK(!ws.resolve("", &out, &why));
    CHECK(!why.empty());
    why.clear();
    CHECK(!ws.resolve(std::string("notes") + '\0' + "/x", &out, &why));
    CHECK(!why.empty());
}

TEST(ws_rejects_absolute) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string why;
    CHECK(!ws.resolve("/etc/passwd", &out, &why));
    CHECK(!why.empty());
    why.clear();
    // absolute is rejected even when it points inside the root
    CHECK(!ws.resolve((ws.root() / "x.txt").string(), &out, &why));
    CHECK(!why.empty());
}

TEST(ws_rejects_dotdot_escapes) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    const std::string back_in = std::string("notes/../../") + ws.root().filename().string() + "/notes/todo.txt";
    for (const std::string& p : {std::string(".."), std::string("../etc/passwd"),
                                 std::string("a/../../b"), back_in}) {
        std::string why;
        CHECK(!ws.resolve(p, &out, &why));
        CHECK(!why.empty());
    }
}

TEST(ws_does_not_touch_filesystem) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string why;
    REQUIRE(ws.resolve("deep/er/file.txt", &out, &why));
    CHECK(!fs::exists(d.path() / "deep"));      // resolve must not create anything
}

// ---------------------------------------------------------- calculator

TEST(calc_precedence) {
    double v = 0;
    std::string err;
    REQUIRE(eval("2 + 3 * 4", &v, &err));
    CHECK(v == 14);
}

TEST(calc_left_to_right) {
    double v = 0;
    std::string err;
    REQUIRE(eval("10 - 4 - 3", &v, &err));
    CHECK(v == 3);
    REQUIRE(eval("100 / 10 / 5", &v, &err));
    CHECK(v == 2);
}

TEST(calc_parentheses) {
    double v = 0;
    std::string err;
    REQUIRE(eval("(2 + 3) * 4", &v, &err));
    CHECK(v == 20);
    REQUIRE(eval("2 * (3 + 4) - 5", &v, &err));
    CHECK(v == 9);
}

TEST(calc_negatives_and_decimals) {
    double v = 0;
    std::string err;
    REQUIRE(eval("-3 * -3", &v, &err));
    CHECK(v == 9);
    REQUIRE(eval("7 / 2", &v, &err));
    CHECK(v == 3.5);
    REQUIRE(eval("0.5 * 4 - 1.5", &v, &err));
    CHECK(v == 0.5);
}

TEST(calc_division_by_zero) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / 0", &v, &err));
    CHECK(err == "division by zero");
    err.clear();
    CHECK(!eval("1 / (2 - 2)", &v, &err));
    CHECK(err == "division by zero");
}

TEST(calc_own_mixed_expressions) {
    double v = 0;
    std::string err;
    REQUIRE(eval("1 + 2 * (3 - 1) / 4", &v, &err));   // 1 + 4/4 = 2
    CHECK(v == 2);
    REQUIRE(eval("((2))*((3+1))", &v, &err));
    CHECK(v == 8);
}

TEST(calc_very_long_and_deeply_nested) {
    double v = 0;
    std::string err;
    std::string longexpr = "0";
    for (int i = 0; i < 20000; ++i) longexpr += "+1";
    REQUIRE(eval(longexpr, &v, &err));
    CHECK(v == 20000);

    std::string nested = std::string(5000, '(') + "7" + std::string(5000, ')');
    if (eval(nested, &v, &err)) CHECK(v == 7);       // tokenizer may cap depth; evaluator must not crash
}

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
