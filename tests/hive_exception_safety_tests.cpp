#include "hive.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace
{

class test_failure final : public std::runtime_error
{
  public:
    explicit test_failure(const char* message) : std::runtime_error(message) {}
};

#define TOYHIVE_STRINGIFY_IMPL(value) #value
#define TOYHIVE_STRINGIFY(value) TOYHIVE_STRINGIFY_IMPL(value)
#define CHECK(condition)                                                       \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
        {                                                                      \
            throw test_failure("CHECK failed: " #condition " (" __FILE__       \
                               ":" TOYHIVE_STRINGIFY(__LINE__) ")");           \
        }                                                                      \
    } while (false)

struct throwing_value
{
    static int live_count;
    static int successful_constructions_before_throw;

    int value;

    static void reset(int allowed_constructions)
    { successful_constructions_before_throw = allowed_constructions; }

    static void maybe_throw()
    {
        if (successful_constructions_before_throw == 0)
        {
            throw std::runtime_error("throwing_value construction failed");
        }
        --successful_constructions_before_throw;
    }

    explicit throwing_value(int input) : value(input)
    {
        maybe_throw();
        ++live_count;
    }

    throwing_value(const throwing_value& other) : value(other.value)
    {
        maybe_throw();
        ++live_count;
    }

    throwing_value(throwing_value&& other) noexcept : value(other.value)
    { ++live_count; }

    ~throwing_value() { --live_count; }
};

int throwing_value::live_count = 0;
int throwing_value::successful_constructions_before_throw = 0;

std::vector<int> values_of(const hive<throwing_value>& values)
{
    std::vector<int> result;
    for (auto it = values.begin(); it != values.end(); ++it)
    {
        result.push_back(it->value);
    }
    return result;
}

void expect_throwing_emplace_preserves_values(hive<throwing_value>& values,
                                              const std::vector<int>& expected)
{
    const int old_live_count = throwing_value::live_count;
    const auto old_size = values.size();

    throwing_value::reset(0);
    bool threw = false;
    try
    {
        values.emplace(999);
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }

    CHECK(threw);
    CHECK(values.size() == old_size);
    CHECK(throwing_value::live_count == old_live_count);
    CHECK(values_of(values) == expected);
}

void emplace_failure_in_existing_block_preserves_state()
{
    throwing_value::reset(64);
    hive<throwing_value> values;
    for (int value = 0; value < 3; ++value)
    {
        values.emplace(value);
    }

    expect_throwing_emplace_preserves_values(values, {0, 1, 2});
}

void emplace_failure_in_new_block_preserves_state()
{
    throwing_value::reset(64);
    hive<throwing_value> values;
    for (int value = 0; value < 64; ++value)
    {
        values.emplace(value);
    }

    // 所有已有 block 都已满，失败发生在新 block 挂入链表之前。
    std::vector<int> expected;
    for (int value = 0; value < 64; ++value)
    {
        expected.push_back(value);
    }
    expect_throwing_emplace_preserves_values(values, expected);
}

void copy_constructor_failure_releases_partial_copy()
{
    throwing_value::reset(64);
    hive<throwing_value> source;
    for (int value = 0; value < 5; ++value)
    {
        source.emplace(value);
    }

    const int live_count_before_copy = throwing_value::live_count;
    throwing_value::reset(2);

    bool threw = false;
    try
    {
        hive<throwing_value> copy(source);
        (void)copy;
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }

    CHECK(threw);
    CHECK(throwing_value::live_count == live_count_before_copy);
    CHECK(values_of(source) == std::vector<int>({0, 1, 2, 3, 4}));
}

void copy_assignment_failure_preserves_target()
{
    throwing_value::reset(64);
    hive<throwing_value> source;
    source.emplace(10);
    source.emplace(20);

    hive<throwing_value> target;
    target.emplace(-1);
    target.emplace(-2);

    const int live_count_before_assignment = throwing_value::live_count;
    throwing_value::reset(1);

    bool threw = false;
    try
    {
        target = source;
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }

    CHECK(threw);
    CHECK(throwing_value::live_count == live_count_before_assignment);
    CHECK(values_of(target) == std::vector<int>({-1, -2}));
    CHECK(values_of(source) == std::vector<int>({10, 20}));
}

struct test_case
{
    std::string_view name;
    void (*run)();
};

constexpr test_case tests[] = {
    {"emplace_failure_in_existing_block_preserves_state",
     emplace_failure_in_existing_block_preserves_state},
    {"emplace_failure_in_new_block_preserves_state",
     emplace_failure_in_new_block_preserves_state},
    {"copy_constructor_failure_releases_partial_copy",
     copy_constructor_failure_releases_partial_copy},
    {"copy_assignment_failure_preserves_target",
     copy_assignment_failure_preserves_target},
};

} // namespace

int main(int argc, char* argv[])
{
    bool success = true;
    bool found = argc != 2;

    for (const test_case& test : tests)
    {
        if (argc == 2 && test.name != argv[1]) { continue; }
        found = true;

        try
        {
            test.run();
            std::cout << "[PASS] " << test.name << '\n';
        }
        catch (const std::exception& error)
        {
            success = false;
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
        }
    }

    if (!found)
    {
        std::cerr << "Unknown test: " << argv[1] << '\n';
        return EXIT_FAILURE;
    }

    CHECK(throwing_value::live_count == 0);
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}