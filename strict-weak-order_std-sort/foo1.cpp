#include <vector>
#include <algorithm>

int main()
{
    std::vector<int> v1(17, 0);
    std::sort(v1.begin(), v1.end(), [](int a, int b ) { return a >= b; });
}

