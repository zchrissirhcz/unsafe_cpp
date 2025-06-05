# std::sort 比较器违反 strict weak ordering 引发 crash 的分析

## 1. std::sort 用法说明

`std::sort()` 默认升序排序

```cpp
template< class RandomIt >
void sort( RandomIt first, RandomIt last );
```

也支持传入第三个参数 `comp` 作为比较器:

```cpp
template< class RandomIt, class Compare >
void sort( RandomIt first, RandomIt last, Compare comp );
```

提到 modern C++, 一个常见的例子是用 lambda 表达式作为 `std::sort()` 的比较器参数， 例如 C++ Primer 5e 在 10.3 节提到:
```cpp
stable_sort(words.begin(), words.end(),
    [](const string& a, const string& b)
    {
        return a.size() < b.size();
    }
)
```

C++ Primer 5e 在 11.2 节则对比较器的限制做了说明：

> Just as we can provide our own comparison operation to an algorithm, we can also supply our own operation to use in place of the < operator on keys.
> The specified oepration must define a **strict weak ordering** over the key types.
> We can think of a strict weak ordering as "less than", although our function might use a more complicated procedure.
> However we define it, the comparision function must have the following properties:
> - Two keys cannot both be "less than" each other; if k1 is "less than" k2, then k2 must never be "less than" k1.
> - If k1 is "less than" k2 and k2 is "less than" k3, then k1 must be "less than" k3.
> - If there are two keys, andneigher key is "less than" the other, then we'll say that those keys are "equivalent".
If k1 is "equivalent" to k2 and k2 is "equivalent" to k3, then k1 must be "equivalent" to k3.

CppReference 也明确提到了 (见参考链接 [1] [2])， 比较器要满足的条件:

Establishes strict weak ordering relation with the following properties:
- For all a, `comp(a, a) == false`.
- If `comp(a, b) == true` then `comp(b, a) == false`.
- if `comp(a, b) == true` and `comp(b, c) == true` then `comp(a, c) == true`.


## 2. 比较器的常见错误写法: 带等号

### 2.1 复现
https://godbolt.org/z/eq6o6MMco

```cpp
#include <vector>
#include <algorithm>

int main()
{
    std::vector<int> v1(17, 0);
    std::sort(v1.begin(), v1.end(), [](int a, int b ) { return a >= b; });
}
```

程序用 GCC 14.2 编译，运行会 crash:
```bash
Program returned: 139
Program terminated with signal: SIGSEGV
```

更换编译器， 或修改数量为少于16个， 则不会 crash， 和 `std::sort()` 实现方式相关。


### 2.2 分析

比较器的实现中包含了等号:

```cpp
auto comp = [](int a, int b ) { return a >= b; }
```

这违反了

> If `comp(a, b) == true` then `comp(b, a) == false`.

这条规则.

## 3. 比较器的罕见错误写法: 取地址

### 3.1 原始复现代码
网友 fox 发起提问， 项目中代码遇到 stack smesh 的运行报错, 整理得到最小复现代码如下  <https://godbolt.org/z/7Gz1oM61e>, 环境是 GCC 14.2:

```cpp
#include <algorithm>
#include <vector>
typedef struct loc_
{
    int64_t timestamp_ns = 0;
    float a =0;
    float b =0;
    float c =0;
    float d[1000];
    float e[1000];
    float f[1000];
}loc;
int main(){
loc temp[20];
    std::sort(temp,
            temp + 20,
            [](const loc& a, const loc& b) {
                if(a.timestamp_ns==b.timestamp_ns){
                    return &a>&b;
                }
                    return a.timestamp_ns < b.timestamp_ns;  // 升序排序
            });
            return -1;
}
```

### 3.2 尝试: 调整复现代码， 调试到 STL

上述代码问题不少：
- d, e, f 数组成员数量过多， 占用过大的 stack 空间
- 结构体命名不够规范
- C/C+ 风格杂糅
- 缩进错乱
- main 返回值 -1 误导人

修复上述问题， 并简化代码、开启 ASAN  (`-fsanitize=address` 编译选项), 代码如下:

<https://godbolt.org/z/PE3GrvP78>

```cpp
#include <algorithm>
#include <vector>
struct loc {
    int x = 0;
    int y = 0;
};
int main() {
    loc t[17]{};
    std::sort(t, t + 17, [](const loc& a, const loc& b) {
        if(a.x == b.x){
            return &a>&b;
        }
        return a.x < b.x;
    });
}
```

ASAN 的报错内容不是很直观； 我转到本地 Ubuntu 22.04, VSCode + GDB 调试到 STL 源码， 并增了地址打印。 发现了端倪：
- 参数 `const loc& a` 取值， 出现了和 `end` 相等的情况
- `end` 是数组 `t` 最后一个元素后面一个位置， 通常只用于迭代器方式的比较， 取 `a.x` 是做 dereference 操作， 是非法内存访问， 因此 ASAN 报告为 `stack-buffer-overflow`.


这份“有点脏”的代码放在 <https://godbolt.org/z/esc8T458q> ， 它让 `const loc& a` 取值为 end， 更加直观：

```cpp
#include <algorithm>
#include <stdio.h>
typedef struct loc_
{
    loc_() = default;
    loc_(int x, int y) : x(x), y(y) {}
    int x = 0;
    int y = 0;
}loc;

int main() {
    const int n = 17;
    loc t[n]{};

    loc* p = t + n;
    loc* q = &t[n-1];
    fprintf(stderr, " t = %p\n", t);
    fprintf(stderr, " p = %p\n", p);
    fprintf(stderr, " q = %p\n", q);

    loc* end = t + n;
    const loc& a = *end;

    fprintf(stderr, "end = %p\n", end);
    fprintf(stderr, "&a = %p\n", &a);
    fprintf(stderr, "a.x = %d\n", a.x);
    fprintf(stderr, "bye\n");

    std::sort(t,
        t + n,
        [](const loc& a, const loc& b) {
            fprintf(stderr, "&a = %p, &b=%p\n", &a, &b); // 当 const loc& a = *end; 此时打印 &a 是合法的
            if (a.x == b.x) { // 当 const loc& a = *end, 访问 a.x 是非法的
                //return &a > &b;
                if (&a > &b)
                {
                    fprintf(stderr, "&a > &b : YES\n");
                    return true;
                }
                else
                {
                    fprintf(stderr, "&a > &b : No\n");
                    return false;
                }
            }
            return a.x < b.x;  // 升序排序
        });
    return 0;
}
```

```bash
 t = 0x7f2b56400020
 p = 0x7f2b564000a8
 q = 0x7f2b564000a0
&a = 0x7f2b56400028, &b=0x7f2b56400060
&a > &b : No
&a = 0x7f2b56400028, &b=0x7f2b564000a0
&a > &b : No
&a = 0x7f2b56400060, &b=0x7f2b564000a0
&a > &b : No
&a = 0x7f2b56400028, &b=0x7f2b56400020
&a > &b : YES
&a = 0x7f2b56400030, &b=0x7f2b56400020
&a > &b : YES
&a = 0x7f2b56400038, &b=0x7f2b56400020
&a > &b : YES
&a = 0x7f2b56400040, &b=0x7f2b56400020
&a > &b : YES
&a = 0x7f2b56400048, &b=0x7f2b56400020
&a > &b : YES
&a = 0x7f2b56400050, &b=0x7f2b56400020
&a > &b : YES
&a = 0x7f2b56400058, &b=0x7f2b56400020
&a > &b : YES
&a = 0x7f2b56400060, &b=0x7f2b56400020
&a > &b : YES
&a = 0x7f2b56400068, &b=0x7f2b56400020
&a > &b : YES
&a = 0x7f2b56400070, &b=0x7f2b56400020
&a > &b : YES
&a = 0x7f2b56400078, &b=0x7f2b56400020
&a > &b : YES
&a = 0x7f2b56400080, &b=0x7f2b56400020
&a > &b : YES
&a = 0x7f2b56400088, &b=0x7f2b56400020
&a > &b : YES
&a = 0x7f2b56400090, &b=0x7f2b56400020
&a > &b : YES
&a = 0x7f2b56400098, &b=0x7f2b56400020
&a > &b : YES
&a = 0x7f2b564000a0, &b=0x7f2b56400020
&a > &b : YES
&a = 0x7f2b564000a8, &b=0x7f2b56400020
```

其中 `__first` 就是 `const loc& a` 的马甲， 它是通过如下代码， 一步步增加到 `end` (0x7f2b564000a8) 的：
```cpp
while (true)
{
  while (__comp(__first, __pivot))
    ++__first;
  --__last;
  while (__comp(__pivot, __last))
    --__last;
  if (!(__first < __last))
    return __first;
  std::iter_swap(__first, __last);
  ++__first;
}
```

看到这里， 有点清晰了， 但也有点头大了。 搞这么复杂， 似乎方向错了。

### 3.3 再次简化并分析

被排序的元素， 必须是结构体类型吗？ 不是， `int` 类型也可以复现。

比较器里的代码， 取参数的地址， 能否再简化一下？ 可以， 因为是数组元素， 在栈上分配， 是严格递增（或递减）顺序， 可以改为 `return true`.

得到如下代码:

<https://godbolt.org/z/rhvx75xsM>
```cpp
#include <vector>
#include <algorithm>
int main()
{
    std::vector<int> v1(17, 0);
    std::sort(v1.begin(), v1.end(), [](int a, int b ) {
        return true; // return &a >= &b 等效
    });
}
```

回看 cppreference 上关于 `comp` 要满足条件的表格， 这也是违反了

> If `comp(a, b) == true` then `comp(b, a) == false`.

这条规则。

## 4. 反思和总结

n 年前在刷学校 OJ 题目时， 题目要求对结构体排序， 需要自行实现比较函数。 遇到过两种写法细微差别， 一个能 AC， 一个没有 AC。 现在想想大概是多了等号导致的。 这个“莫名其妙”的情况出现了不止一次， 出现在不止一个人身上， 只不过当时没有仔细探究， 没弄明白。

最近在整理常见的 C++ Undefined Behavior 、易错写法时， 看到 [C++常见错误：std::sort的cmp函数用错，也会导致程序abort](https://mp.weixin.qq.com/s/PseD5y0-xLaM1GSRQGxMfg) 这篇， 简单尝试后， 我的结论是需要元素数量必须 >= 17 个， 主要记住的是 “比较器 comp 必须不能带等号”。

而在网友 fox 发起提问后， 我第一时间想到的是 `stable_sort()` 替代 `sort()`, 但迟迟没看出 `&a > &b` 的问题， 只是直觉觉得 “正常人不会这么写， 但是好像也没啥错？”。 直到最近看 Cpp Primer 5e 中提及的指针比较操作， 用 VSCode + GDB 源码调试到 STL 内部做了尝试， 发现是到达 `end` 元素； 这个分析挺费劲， 重新审视了 strict weak ordering 的说明， 才发现违反了
> If `comp(a, b) == true` then `comp(b, a) == false`.

P.S. 在“低效debug” 的过程中， 也有新发现: 对于 const reference， 打印它的地址， 和打印被引用对象的地址， 取值是相同的, 打印它的地址也是 ok 的， 但是做 reference 肯定是会 crash 的

<https://godbolt.org/z/z7M4hEcef>
```cpp
int a = 42;
const int& b = a;
printf("&a = %p\n", &a);
printf("&b = %p\n", &b);
```

更有趣的一点是,
```cpp
loc t[n];
loc* p = t + n; // end 指针，相当于 std::end(t). 是最后一个元素 t[n-1] 之后的地址.
const loc& a = *p; // const reference
printf("&a = %p\n", &a); // ok
printf(" p = %p\n", p); // ok, 取值和 &a 一样
printf("p->x = %d\n", p->x); // 非法： end 指针上执行了 dereference
```

说回 `std::sort()`. 关于 `std::sort()` 的比较器 `comp` 要遵循 strict weak ordering 的案例和博客有很多， 例如这篇： [违反 strict weak ordering 导致 P0 故障, 损失百万](https://www.cnblogs.com/gaoxingnjiagoutansuo/p/15771439.html)。
再比如 [C++中使用std::sort自定义排序规则时要注意的崩溃问题](https://blog.csdn.net/albertsh/article/details/119523587) 这篇， 对 GCC STL 源码中的 sort 做了分析， 对于认为 “复现代码元素数量不重要” 的说法， 给出了明确“否定” 的证据。


## References
- [1] https://en.cppreference.com/w/cpp/algorithm/sort
- [2] https://en.cppreference.com/w/cpp/named_req/Compare
- [3] [C++常见错误：std::sort的cmp函数用错，也会导致程序abort](https://mp.weixin.qq.com/s/PseD5y0-xLaM1GSRQGxMfg)
- [4] [违反 strict weak ordering 导致 P0 故障, 损失百万](https://www.cnblogs.com/gaoxingnjiagoutansuo/p/15771439.html)
- [5] [C++中使用std::sort自定义排序规则时要注意的崩溃问题](https://blog.csdn.net/albertsh/article/details/119523587)

