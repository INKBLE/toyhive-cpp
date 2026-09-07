#include "hive.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
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

std::vector<int> values_of(hive<int>& values)
{
    std::vector<int> result;
    for (auto it = values.begin(); it != values.end(); ++it)
    {
        result.push_back(*it);
    }
    return result;
}

void check_values(hive<int>& values, const std::vector<int>& expected)
{
    CHECK(values_of(values) == expected);
    CHECK(values.size() == expected.size());
}

void fill_with_holes(hive<int>& values)
{
    for (int value = 0; value < 130; ++value)
    {
        values.emplace(value);
    }
    for (int value : {1, 63, 64, 127})
    {
        auto it = values.begin();
        while (*it != value)
        {
            ++it;
        }
        values.erase(it);
    }
}

void copy_constructor_preserves_values_and_holes()
{
    hive<int> source;
    fill_with_holes(source);
    const hive<int>& const_source = source;

    hive<int> copy(const_source);
    check_values(
        source,
        {0,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,
         15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,
         29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,
         43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,
         57,  58,  59,  60,  61,  62,  65,  66,  67,  68,  69,  70,  71,  72,
         73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,
         87,  88,  89,  90,  91,  92,  93,  94,  95,  96,  97,  98,  99,  100,
         101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114,
         115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 128, 129});
    check_values(copy, values_of(source));

    source.emplace(999);
    CHECK(values_of(copy).back() == 129);
}

void copy_assignment_replaces_old_values()
{
    hive<int> source;
    source.emplace(10);
    source.emplace(20);
    hive<int> target;
    target.emplace(-1);
    target.emplace(-2);

    target = source;
    check_values(target, {10, 20});
    check_values(source, {10, 20});
}

void move_constructor_transfers_storage()
{
    hive<int> source;
    source.emplace(1);
    source.emplace(2);
    hive<int> moved(std::move(source));

    check_values(moved, {1, 2});
    CHECK(source.empty());
    source.emplace(3);
    check_values(source, {3});
}

void move_assignment_releases_target_and_transfers_storage()
{
    hive<int> source;
    source.emplace(7);
    source.emplace(8);
    hive<int> target;
    target.emplace(-1);

    target = std::move(source);
    check_values(target, {7, 8});
    CHECK(source.empty());
}

void self_assignment_and_self_move_are_safe()
{
    hive<int> values;
    values.emplace(4);
    values.emplace(5);

    check_values(values, {4, 5});
    // 通过别名表达自移动，避免编译器仅针对字面量 std::move(x) 的诊断。
    hive<int>& alias = values;
    values = std::move(alias);
    check_values(values, {4, 5});
}

struct tracked
{
    static int live_count;
    static int copy_count;
    static int move_count;

    int value;

    explicit tracked(int input) : value(input) { ++live_count; }
    tracked(const tracked& other) : value(other.value)
    {
        ++live_count;
        ++copy_count;
    }
    tracked(tracked&& other) noexcept : value(other.value)
    {
        ++live_count;
        ++move_count;
    }
    ~tracked() { --live_count; }
};

int tracked::live_count = 0;
int tracked::copy_count = 0;
int tracked::move_count = 0;

void non_trivial_copy_move_lifetime()
{
    tracked::live_count = 0;
    tracked::copy_count = 0;
    tracked::move_count = 0;

    {
        hive<tracked> source;
        source.emplace(10);
        source.emplace(20);
        CHECK(tracked::live_count == 2);

        hive<tracked> copy(source);
        CHECK(tracked::live_count == 4);
        CHECK(tracked::copy_count == 2);

        hive<tracked> moved(std::move(copy));
        CHECK(tracked::live_count == 4);
        CHECK(moved.size() == 2);
        CHECK(copy.empty());
    }

    CHECK(tracked::live_count == 0);
}

struct test_case
{
    std::string_view name;
    void (*run)();
};

constexpr test_case tests[] = {
    {"copy_constructor_preserves_values_and_holes",
     copy_constructor_preserves_values_and_holes},
    {"copy_assignment_replaces_old_values",
     copy_assignment_replaces_old_values},
    {"move_constructor_transfers_storage", move_constructor_transfers_storage},
    {"move_assignment_releases_target_and_transfers_storage",
     move_assignment_releases_target_and_transfers_storage},
    {"self_assignment_and_self_move_are_safe",
     self_assignment_and_self_move_are_safe},
    {"non_trivial_copy_move_lifetime", non_trivial_copy_move_lifetime},
};

} // namespace

int main(int argc, char* argv[])
{
    static_assert(std::is_copy_constructible<hive<int>>::value,
                  "hive must be copy constructible");
    static_assert(std::is_copy_assignable<hive<int>>::value,
                  "hive must be copy assignable");
    static_assert(std::is_move_constructible<hive<int>>::value,
                  "hive must be move constructible");
    static_assert(std::is_move_assignable<hive<int>>::value,
                  "hive must be move assignable");

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