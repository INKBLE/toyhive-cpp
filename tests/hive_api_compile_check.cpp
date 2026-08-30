#include "hive.hpp"

int main()
{
    hive<int> values;
    values.emplace(1);

    auto it = values.begin();
    values.erase(it);

    return values.empty() ? 0 : 1;
}