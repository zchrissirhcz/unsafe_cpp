/// https://godbolt.org/z/sPGn7efWq
#include <stddef.h>
#include <stdlib.h>
#include <iostream>

// 模拟出现bug时，在特定场景下车道线数量为0
__attribute__((noinline)) // 关闭优化，确保“骗过”编译器的O3优化
int get_num_lanes() {
    return 0;
}

int main()
{
    int num_lane = get_num_lanes();
    float s = 0.f;
    for (size_t i = 0; i < num_lane - 1; i++)
    {
        std::cout << i << std::endl;
        s += rand(); // 模拟实际的算法计算
    }
    printf("s : %f\n", s); // 模拟返回算法结果

    return 0;
}