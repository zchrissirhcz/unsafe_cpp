// https://godbolt.org/z/hfE65zMoW
#include <algorithm>

int main() {
    const int n = 17;
    int t[n]{};
    std::sort(t,
        t + n,
        [](const int& a, const int& b) {
            //return &a < &b;
            return true;
        });
    return 0;
}

