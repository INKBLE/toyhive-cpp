#include "hive.hpp"

#include <type_traits>
#include <utility>

using hive_type = hive<int>;

static_assert(std::is_same_v<decltype(std::declval<const hive_type&>().begin()),
                             hive_type::const_iterator>);
static_assert(std::is_same_v<decltype(std::declval<const hive_type&>().end()),
                             hive_type::const_iterator>);
static_assert(
    std::is_same_v<decltype(std::declval<const hive_type&>().cbegin()),
                   hive_type::const_iterator>);
static_assert(std::is_same_v<decltype(std::declval<const hive_type&>().cend()),
                             hive_type::const_iterator>);
static_assert(
    std::is_same_v<decltype(*std::declval<hive_type::const_iterator>()),
                   const int&>);
static_assert(std::is_same_v<
              decltype(std::declval<hive_type::const_iterator>().operator->()),
              const int*>);
static_assert(
    std::is_constructible_v<hive_type::const_iterator, hive_type::iterator>);
static_assert(
    !std::is_constructible_v<hive_type::iterator, hive_type::const_iterator>);
static_assert(noexcept(std::declval<const hive_type&>().begin()));
static_assert(noexcept(std::declval<const hive_type&>().end()));
static_assert(noexcept(std::declval<const hive_type&>().cbegin()));
static_assert(noexcept(std::declval<const hive_type&>().cend()));

int main()
{
    hive_type values;
    for (int value = 0; value < 130; ++value)
    {
        values.emplace(value);
    }

    auto hole = values.begin();
    ++hole;
    values.erase(hole);

    const hive_type& const_values = values;
    int count = 0;
    for (auto it = const_values.begin(); it != const_values.end(); ++it)
    {
        if (*it != (count == 0 ? 0 : count + 1)) { return 1; }
        ++count;
    }
    if (count != 129) { return 2; }

    count = 0;
    for (auto it = values.cbegin(); it != values.cend(); ++it)
    {
        ++count;
    }
    if (count != 129) { return 3; }

    auto it = values.begin();
    values.erase(it);

    hive_type copy(const_values);
    return copy.size() == 128 ? 0 : 4;
}