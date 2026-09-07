#include "hive.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace
{

class test_failure final : public std::runtime_error
{
  public:
    explicit test_failure(const char* message) : std::runtime_error(message) {}
};

#define CHECK(condition)                                                       \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
        {                                                                      \
            throw test_failure("CHECK failed: " #condition " (" __FILE__       \
                               ":" TOYHIVE_STRINGIFY(__LINE__) ")");           \
        }                                                                      \
    } while (false)

#define TOYHIVE_STRINGIFY_IMPL(value) #value
#define TOYHIVE_STRINGIFY(value) TOYHIVE_STRINGIFY_IMPL(value)

void empty_container()
{
    hive<int> values;
    CHECK(values.size() == 0);
    CHECK(values.empty());
}

void insertion_boundaries_and_size()
{
    for (const int count : {1, 63, 64, 65, 127, 128, 129, 192, 193})
    {
        hive<int> values;

        for (int value = 0; value < count; ++value)
        {
            const auto inserted = values.emplace(value);
            CHECK(*inserted == value);
            CHECK(values.size() == static_cast<hive<int>::size_t>(value + 1));
            CHECK(!values.empty());
        }
    }
}

struct tracked
{
    static int live_count;
    static int construction_count;
    static int destruction_count;

    int value;

    explicit tracked(int input) : value(input)
    {
        ++live_count;
        ++construction_count;
    }

    tracked(const tracked& other) : value(other.value)
    {
        ++live_count;
        ++construction_count;
    }

    ~tracked()
    {
        --live_count;
        ++destruction_count;
    }
};

int tracked::live_count = 0;
int tracked::construction_count = 0;
int tracked::destruction_count = 0;

void non_trivial_object_lifetime()
{
    tracked::live_count = 0;
    tracked::construction_count = 0;
    tracked::destruction_count = 0;

    {
        hive<tracked> values;
        for (int value = 0; value < 65; ++value)
        {
            values.emplace(value);
        }
        CHECK(tracked::live_count == 65);

        CHECK(values.size() == 65);
    }

    CHECK(tracked::live_count == 0);
    CHECK(tracked::construction_count == tracked::destruction_count);
}

struct test_case
{
    std::string_view name;
    void (*run)();
};

constexpr test_case tests[] = {
    {"empty_container", empty_container},
    {"insertion_boundaries_and_size", insertion_boundaries_and_size},
    {"non_trivial_object_lifetime", non_trivial_object_lifetime},
};

} // namespace

int main(int argc, char* argv[])
{
    bool success = true;

    for (const test_case& test : tests)
    {
        if (argc == 2 && test.name != argv[1]) { continue; }

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

    if (argc == 2)
    {
        bool found = false;
        for (const test_case& test : tests)
        {
            found = found || test.name == argv[1];
        }
        if (!found)
        {
            std::cerr << "Unknown test: " << argv[1] << '\n';
            return EXIT_FAILURE;
        }
    }

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}