# 忽视类型比较中的转换警告，感知算法卡滞2分钟

## 1. 问题

路测时感知算法卡滞2分钟，原因是车道线检测SDK中的一个for循环问题：

https://godbolt.org/z/sPGn7efWq

```cpp
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
```

GCC 11.4 会发出警告（被忽略）：

```bash
<source>: In function 'int main()':
<source>:15:26: warning: comparison of integer expressions of different signedness: 'size_t' {aka 'long unsigned int'} and 'int' [-Wsign-compare]
   15 |     for (size_t i = 0; i < num_lane - 1; i++)
      |                        ~~^~~~~~~~~~~~~~
Compiler returned: 0
```

将循环变量改为int即可修复问题：
```cpp
//for (size_t i = 0; i < num_lane - 1; i++)
for (int i = 0; i < num_lane - 1; i++)
```

下面给出原因分析。

## 2. 规则

涉及到了内置整数类型的比较，和类型转换。 

### 2.1 规则1: 有符号与无符号混合运算时的转换

当表达式中同时存在unsigned和int值时，int值会转为unsigned值：

```cpp
unsigned u = 10;
int i = -42;
std::cout << u + i << std::endl; // 如果int占32位，输出4294967264
```

根据 [usual arithmetic conversions](https://en.cppreference.com/w/cpp/language/usual_arithmetic_conversions.html) 规则：

- 当一个操作数是有符号类型(S)，另一个是无符号类型(U)时：
    - 如果U的整数转换等级大于等于S，结果类型是U
    - 如果S能表示U的所有值，结果类型是S
    - 否则，结果类型是S对应的无符号类型

在我们的代码中：
- size_t(U)与int(S)比较
- size_t的等级高于int
- 因此结果类型是size_t
- `-1` 会先转为 size_t 类型再做比较

### 2.2 规则2: 无符号类型的取值溢出处理

当给无符号类型赋值一个超出其表示范围的值时，结果是对无符号类型表示数值总数取模：

```cpp
unsigned char a = -1; // 得到255
```

根据 [Integral_promotion_and_conversion](https://en.cppreference.com/w/cpp/language/implicit_conversion.html#Integral_promotion_and_conversion), 无符号类型的值计算公式：

```
结果值 = 源值 modulo 2^n (n是目标类型的位数)
```

## 3. 简明问题分析

当num_lane = 0时，问题代码实际等价于：
```cpp
for (size_t i = 0; i < 0 - 1; i++)
```

根据规则1， -1 会转为 size_t 类型， 相当于做了这样的转换：
```cpp
size_t len = -1;
=>
size_t len = (0 - 1 + std::numeric_limits<size_t>::max()) % std::numeric_limits<size_t>::max();
=> 
size_t len = std::numeric_limits<size_t>::max() - 1;
=>
size_t len 18446744073709551615
```

因此 for 循环实际执行的是:
```cpp
for (size_t i = 0; i < 18446744073709551615; i++)
{
    ...
}
```

## 4. 总结

第一， 不能轻易忽略编译器警告， 日常尽可能做到 0 warning， 一旦出现 warning 认真对待和修复。

第二， C++ Primer 第五版第二章类型转换有提及这两条转换规则， 但比较不正式， 在 cppreference 文档中叫做 integer conversion rank， 有明确的规则， 编译器也是根据规则做实现的。

## References

- [Operator Comparison](https://en.cppreference.com/w/cpp/language/operator_comparison.html)
- [Usual arithmetic conversions](https://en.cppreference.com/w/cpp/language/usual_arithmetic_conversions.html)
- [Integer promition and conversion](https://en.cppreference.com/w/cpp/language/implicit_conversion.html#Integral_promotion_and_conversion)