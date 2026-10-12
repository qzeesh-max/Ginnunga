#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <exception>

namespace ginnunga::test {

struct TestCase {
    std::string name;
    std::function<void()> func;
};

inline std::vector<TestCase>& get_registry() {
    static std::vector<TestCase> registry;
    return registry;
}

inline bool register_test(const std::string& name, std::function<void()> func) {
    get_registry().push_back({name, std::move(func)});
    return true;
}

#define TEST(suite, name) \
    void test_##suite##_##name(); \
    static const bool reg_##suite##_##name = ::ginnunga::test::register_test(#suite "." #name, test_##suite##_##name); \
    void test_##suite##_##name()

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: (" #cond ") at " << __FILE__ << ":" << __LINE__ << "\n"; \
            throw std::runtime_error("Assertion failed: " #cond); \
        } \
    } while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_EQ(a, b) \
    do { \
        if (!((a) == (b))) { \
            std::cerr << "Assertion failed: " #a " == " #b " (" << (a) << " != " << (b) << ") at " << __FILE__ << ":" << __LINE__ << "\n"; \
            throw std::runtime_error("Assertion failed: " #a " == " #b); \
        } \
    } while (0)

#define ASSERT_NE(a, b) \
    do { \
        if ((a) == (b)) { \
            std::cerr << "Assertion failed: " #a " != " #b " at " << __FILE__ << ":" << __LINE__ << "\n"; \
            throw std::runtime_error("Assertion failed: " #a " != " #b); \
        } \
    } while (0)

inline int run_all_tests() {
    int passed = 0;
    int failed = 0;
    std::cout << "========================================================\n";
    std::cout << "  Running Ginnunga Unit Test Suite (" << get_registry().size() << " tests)\n";
    std::cout << "========================================================\n";

    for (const auto& test : get_registry()) {
        std::cout << "  [RUN]  " << test.name << " ... ";
        try {
            test.func();
            std::cout << "PASSED\n";
            ++passed;
        } catch (const std::exception& e) {
            std::cout << "FAILED (" << e.what() << ")\n";
            ++failed;
        } catch (...) {
            std::cout << "FAILED (Unknown exception)\n";
            ++failed;
        }
    }

    std::cout << "--------------------------------------------------------\n";
    std::cout << "  Summary: " << passed << " passed, " << failed << " failed.\n";
    std::cout << "========================================================\n";
    return failed == 0 ? 0 : 1;
}

} // namespace ginnunga::test
