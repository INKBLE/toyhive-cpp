#include "hive.hpp"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <initializer_list>
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

std::vector<int> forward_values(hive<int>& values)
{
    std::vector<int> result;
    for (auto it = values.begin(); it != values.end(); ++it)
    {
        result.push_back(*it);
    }
    return result;
}

std::vector<int> reverse_values(hive<int>& values)
{
    std::vector<int> result;
    for (auto it = values.end(); it != values.begin();)
    {
        --it;
        result.push_back(*it);
    }
    return result;
}

auto find_value(hive<int>& values, int value)
{
    auto it = values.begin();
    while (it != values.end() && *it != value)
    {
        ++it;
    }
    CHECK(it != values.end());
    return it;
}

void check_state(hive<int>& values, const std::vector<int>& expected)
{
    CHECK(forward_values(values) == expected);

    std::vector<int> expected_reverse(expected.rbegin(), expected.rend());
    CHECK(reverse_values(values) == expected_reverse);
    CHECK(values.size() == expected.size());
    CHECK(values.empty() == expected.empty());
}

void erase_and_expect_next(hive<int>& values, int erased, int next)
{
    const auto returned = values.erase(find_value(values, erased));
    if (next < 0) { CHECK(returned == values.end()); }
    else
    {
        CHECK(returned != values.end());
        CHECK(*returned == next);
    }
}

void single_hole_merge_and_erase_return()
{
    hive<int> values;
    for (int value : {10, 20, 30})
    {
        values.emplace(value);
    }

    // [10][20][30] -> [10][ ][30]
    erase_and_expect_next(values, 20, 30);
    check_state(values, {10, 30});
}

void left_hole_merge()
{
    hive<int> values;
    for (int value : {10, 20, 30, 40})
    {
        values.emplace(value);
    }

    // [10][20][30][40] -> [ ][20][30][40] -> [ ][ ][30][40]
    erase_and_expect_next(values, 10, 20);
    erase_and_expect_next(values, 20, 30);
    check_state(values, {30, 40});
}

void right_hole_merge()
{
    hive<int> values;
    for (int value : {10, 20, 30, 40})
    {
        values.emplace(value);
    }

    // [10][20][30][40] -> [10][20][ ][40] -> [10][ ][ ][40]
    erase_and_expect_next(values, 30, 40);
    erase_and_expect_next(values, 20, 40);
    check_state(values, {10, 40});
}

void bidirectional_hole_merge()
{
    hive<int> values;
    for (int value : {10, 20, 30, 40, 50})
    {
        values.emplace(value);
    }

    // [10][20][30][40][50] -> [10][ ][30][ ][50] -> [10][ ][ ][ ][50]
    erase_and_expect_next(values, 20, 30);
    erase_and_expect_next(values, 40, 50);
    erase_and_expect_next(values, 30, 50);
    check_state(values, {10, 50});
}

void alternating_holes_bidirectional_iteration()
{
    hive<int> values;
    std::vector<int> expected;
    for (int value = 0; value < 130; ++value)
    {
        values.emplace(value);
        expected.push_back(value);
    }

    for (int value = 1; value < 130; value += 2)
    {
        erase_and_expect_next(values, value, value + 1 < 130 ? value + 1 : -1);
        expected.erase(std::find(expected.begin(), expected.end(), value));
    }

    check_state(values, expected);
}

void erase_entire_middle_block_and_cross_it()
{
    hive<int> values;
    std::vector<int> expected;
    for (int value = 0; value < 192; ++value)
    {
        values.emplace(value);
        expected.push_back(value);
    }

    // 删除第二个完整 block（槽位 64..127），留下首尾两个 block。
    for (int value = 64; value < 128; ++value)
    {
        erase_and_expect_next(values, value, value == 127 ? 128 : value + 1);
        expected.erase(std::find(expected.begin(), expected.end(), value));
    }

    check_state(values, expected);

    auto it = find_value(values, 63);
    ++it;
    CHECK(it != values.end());
    CHECK(*it == 128);

    it = find_value(values, 128);
    --it;
    CHECK(*it == 63);
}

void erase_range_within_block()
{
    hive<int> values;
    for (int value = 0; value < 10; ++value)
    {
        values.emplace(value);
    }

    auto first = find_value(values, 3);
    auto last = find_value(values, 7);
    const auto returned = values.erase(first, last);

    CHECK(returned != values.end());
    CHECK(*returned == 7);
    check_state(values, {0, 1, 2, 7, 8, 9});
}

void erase_range_across_blocks_and_existing_holes()
{
    hive<int> values;
    for (int value = 0; value < 130; ++value)
    {
        values.emplace(value);
    }

    values.erase(find_value(values, 63));
    values.erase(find_value(values, 65));

    auto first = find_value(values, 60);
    auto last = find_value(values, 70);
    const auto returned = values.erase(first, last);

    CHECK(returned != values.end());
    CHECK(*returned == 70);

    std::vector<int> expected;
    for (int value = 0; value < 130; ++value)
    {
        if (value < 60 || value >= 70) { expected.push_back(value); }
    }
    check_state(values, expected);
}

void erase_range_to_end_and_empty_range()
{
    hive<int> values;
    for (int value = 0; value < 130; ++value)
    {
        values.emplace(value);
    }

    auto first = find_value(values, 64);
    const auto returned = values.erase(first, values.end());
    CHECK(returned == values.end());

    std::vector<int> expected;
    for (int value = 0; value < 64; ++value)
    {
        expected.push_back(value);
    }
    check_state(values, expected);

    const auto unchanged = values.erase(values.begin(), values.begin());
    CHECK(unchanged == values.begin());
    check_state(values, expected);
}

struct test_case
{
    std::string_view name;
    void (*run)();
};

constexpr test_case tests[] = {
    {"single_hole_merge_and_erase_return", single_hole_merge_and_erase_return},
    {"left_hole_merge", left_hole_merge},
    {"right_hole_merge", right_hole_merge},
    {"bidirectional_hole_merge", bidirectional_hole_merge},
    {"alternating_holes_bidirectional_iteration",
     alternating_holes_bidirectional_iteration},
    {"erase_entire_middle_block_and_cross_it",
     erase_entire_middle_block_and_cross_it},
    {"erase_range_within_block", erase_range_within_block},
    {"erase_range_across_blocks_and_existing_holes",
     erase_range_across_blocks_and_existing_holes},
    {"erase_range_to_end_and_empty_range", erase_range_to_end_and_empty_range},
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

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}