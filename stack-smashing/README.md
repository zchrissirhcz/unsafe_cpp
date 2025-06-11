# stack smashing 是怎么回事

## 1. 经典报错信息

当程序运行出现 crash， 有时候会打印出 `*** stack smashing detected ***`, e.g.

```bash
$ ./a 1 2 3 4 5 6 7 8
*** stack smashing detected ***: terminated
```

stack 的意思是 call stack.  smashing 的意思是破碎。 因此这句提示信息是说， call stack 被破坏了。

这句信息的打印有几个条件：
1） 启用了 `-fstack-protector-strong` 或 `-fstack-protector-all` 编译选项
2） 发生了越界访问
3） 越界访问到的内容， 恰好被检查到了

## 2. stack smashing 不是 stack overflow

初学者很容易把 stack overflow 和 stack smashing 混为一谈， 觉得都是 ”栈溢出“ 的意思。

stack overflow 指的是， 程序运行的某个时刻对应的 call stack 的大小， 超过了阈值（MSVC是1M， Linux是8M）。

stack smashing 意思是， call stack 中的某个一个 segment / frame （也就是其中一个函数对应的栈空间）， 最后一个合法位置之后的位置， 被写入了内容。

## 3. 直观理解 stack smashing

[参考链接1] 给出了直观理解 stack smashing 的代码：

用户原始代码，是向16元素的数组拷贝字符串：
```c
void foo(const char* str)
{
	char buffer[16];
	strcpy(buffer, str);
}
```

编译和运行程序:
```bash
gcc a.c -o a -fstack-protector-all
./a 1 2 3 4 5 6 7 8
```

开启 `-fstack-protector-strong` 编译选项后， 编译器相当于生成了如下代码： 在当前函数栈空间额外分配了一小块内存 canary（金丝雀）， 取值来自 `__stack_chk_guard`; 执行完函数原本的功能（即字符串拷贝）后， 检查 canary 的值和 `__stack_chk_guard` 是否相同， 如果不同则说明发生了越界
```c
/* Note how buffer overruns are undefined behavior and the compilers tend to
   optimize these checks away if you wrote them yourself, this only works
   robustly because the compiler did it itself. */
extern uintptr_t __stack_chk_guard;
noreturn void __stack_chk_fail(void);
void foo(const char* str)
{
	uintptr_t canary = __stack_chk_guard;
	char buffer[16];
	strcpy(buffer, str);
	if ( (canary = canary ^ __stack_chk_guard) != 0 )
		__stack_chk_fail();
}
```

## 4. 汇编层面理解

### 4.1 Linux-x64 GCC 反汇编

gentoo wiki 这篇分析写的很直观：

https://wiki.gentoo.org/wiki/Stack_smashing_debugging_guide

```c
int main(int argc, char * argv[]) {
    volatile long v[8];
    v[argc] = 42;

    printf("Hello! Is my stack OK?\n");

    return v[argc+1];
}
```

在函数正式执行前， 会把 `fs:0x28` 内容放到栈里面， 也就是 「放置金丝雀」:
```
   0x0000000000401136 <+0>:     push   %rbp
   0x0000000000401137 <+1>:     mov    %rsp,%rbp
   0x000000000040113a <+4>:     sub    $0x60,%rsp
   0x000000000040113e <+8>:     mov    %edi,-0x54(%rbp)
   0x0000000000401141 <+11>:    mov    %rsi,-0x60(%rbp)
   0x0000000000401145 <+15>:    mov    %fs:0x28,%rax       // 获取 fs:0x28 内容到 rax
   0x000000000040114e <+24>:    mov    %rax,-0x8(%rbp)     // 从 rax 拷贝到栈上。 则 $rbp-8 就是 canary
   0x0000000000401152 <+28>:    xor    %eax,%eax
```

函数正式执行。 这里略过。

当函数正式内容运行完毕， 执行 canary 的检查：
```
   0x0000000000401176 <+64>:    mov    -0x50(%rbp,%rax,8),%rax
   0x000000000040117b <+69>:    mov    -0x8(%rbp),%rdx    // 把栈上的 canary 拷贝到 rdx 寄存器里
   0x000000000040117f <+73>:    sub    %fs:0x28,%rdx      // 拿现在的 canary 和原始的 canary 做比较
   0x0000000000401188 <+82>:    je     0x40118f <main+89> // 如果相等， 那当前函数正常返回
   0x000000000040118a <+84>:    callq  0x401040 <__stack_chk_fail@plt> // 不相等， 说明发生了越界写入， 则调用 __stack_chk_fail()
=> 0x000000000040118f <+89>:    leaveq
   0x0000000000401190 <+90>:    retq
```

其中 `__stack_chk_fail()` 搞得神神秘秘的， 其实它很简单: 打印「遗言」并「死掉」：

```c
void __stack_chk_fail()
{
    printf("*** stack smashing detection ***\n");
    abort();
}
```

### 4.2 Windows MSVC x64 反汇编

使用 Visual Studio 2022, Debug 模式，会默认开启 `/RTCs` 编译选项 (参考链接[3])

> Enables stack frame run-time error checking, as follows:

> Detection of overruns and underruns of local variables such as arrays

实验下来发现需要让数组有9个元素； 输入不用修改仍然是 `1 2 3 4 5 6 7 8`.

```cpp
#include <stdio.h>
int main(int argc, char * argv[]) {
    volatile long v[9]; // 这里需要改为9， 用8无法触发crash
    v[argc] = 42;

    printf("Hello! Is my stack OK?\n");

    return v[argc+1];
}
```

MSVC 会友好的弹窗提示：

> Run-Time Check Failure #2 - Stack around the variable 'v' was corrupted

这相当于 GCC 的 `*** stack smashing detected ***`. 对应的反汇编如下:

```asm
```asm
#include <stdio.h>

int main(int, argc, char* argv[])
{
    mov qwrod ptr [rsp+10h], rdx
    mov dword ptr [rsp+8],ecx
    push rdi
    sub rsp,70h
    lea rdi,[rsp+20h]
    mov ecx,14h
    mov eax, 0CCCCCCCCh
    rep stos dword ptr [rdi]
    mov ecx, dwrod ptr [argc]
    rax, qword ptr [__security_cookie (07FF724FEC040h)]
    xor rax, rsp
    mov qword ptr [rsp + 60h], rax
volatile long v[9];
v[argc]=42;
    movxsd rax, dwrod ptr [argc]
    mov dwrod ptr v[rax*4], 2Ah

printf("Hello! Is my stack OK?\n")
    lea rcx, [$xdatasym+4C0h (07FF724FEC00h)]
    call printf (07FF724FE1113h)

return v[argc+1];
    mov eax, dwrod ptr [argc]
    inc eax
    cdqe
    mov eax, dword ptr v[rax*4]
}
    mov edi, eax
    mov rcx, rsp
    lea rdx,[__xt_z+1E0h (07FF724FE9C80h)]
    call _RTC_CheckStackVars(07FF724FE11F9h)
=>  mov eax, edi
    mov rcx, qword ptr [rsp+60h]
    xor rcx, rsp
    call __security_check_cookit (07FF724FE1122h)
    add rsp, 70h
    pop rdi
    ret
```

可以看到， `_RTC_CheckStackVars()` 函数被调用，来表达 “stack smashing" 的意思。 它相当于 GCC 下的 `__stack_chk_fail()` 函数。

## 5. 通过代码实现定制的栈保护

可以用 RAII 的方式， 自行写一个 stack smashing detector， 而不是由编译器生成。 受限于检查原理的局限性， 权当娱乐吧：

```cpp
#include <stdio.h>

#include <cstdio>
#include <cstdlib>
#include <cstdint>

// 简化版，无头文件
static constexpr std::uint64_t kMagic = 0xDEADC0DECAFEBABEull;

class StackCanary {
public:
    StackCanary()  noexcept : canary_(kMagic) {}
    ~StackCanary() noexcept {
        if (canary_ != kMagic) {
            std::fprintf(stderr, "[!] *** Stack smashing detected! ***\n");
            std::abort();
        }
    }
private:
    volatile std::uint64_t canary_;
};
#define LOCAL_STACK_GUARD() StackCanary __auto_stack_guard__


// $ gcc a.c -o a
// $ ./a 1 2 3 4 5 6 7 8
// *** stack smashing detected ***: terminated
#include <stdio.h>
int main(int argc, char * argv[]) {
    LOCAL_STACK_GUARD();
    volatile long v[8];
    v[argc] = 42;

    printf("Hello! Is my stack OK?\n");

    return v[argc+1];
}
```

## 6. 编译器默认开了栈保护的编译选项吗？


查询方法：
```bash
gcc -Q --help=common | grep 'stack-protector'
```

### 6.1 linux-x64 ubuntu

| flags                      | gcc 7.5    | gcc 9.4    | gcc 11.4   |
| -------------------------- | ---------- | ---------- | ---------- |
| -Wstack-protector          | [disabled] | [disabled] | [disabled] |
| -fstack-protector          | [disabled] | [disabled] | [disabled] |
| -fstack-protector-all      | [disabled] | [disabled] | [disabled] |
| -fstack-protector-explicit | [disabled] | [disabled] | [disabled] |
| -fstack-protector-strong   | [disabled] | [disabled] | [enabled]  |

解释：（参考链接4）：

stack-protector：保护函数中通过alloca()分配缓存以及存在大于8字节的缓存。缺点是保护能力有限。

stack-protector-all：保护所有函数的栈。缺点是增加很多额外栈空间，增加程序体积。

stack-protector-strong：在stack-protector基础上，增加本地数组、指向本地帧栈地址空间保护。

stack-protector-explicit：在stack-protector基础上，增加程序中显式属性"stack_protect"空间。

如果要停止使用stack-protector功能，需要加上-fno-stack-protector。

stack-protector性能：stack-protector > stack-protector-strong > stack-protector-all。

stack-protector覆盖范围：stack-protector < stack-protector-strong < stack-protector-all。

### 6.2 QNX 编译器

无论是 qnx710 还是 qnx800， 文档上都宣称:

1)  使用 qcc 作为编译器（会检查license）， 会隐式启用 -fstack-protector-strong 选项

2） 使用 ntoaarch64-gcc （不会检查license）， 则不会启用这一选项

实际验证，发现 `ntoaarch64-gcc -Q --help=common | grep 'stack-protector'` 方式查询到是 disable 状态， `qcc` 则无法查到信息； 而根据 gentoo wiki 提供的代码实验， 在数组大小为9时也能触发 stack smashing detected:

```c
#include <stdio.h>
int main(int argc, char * argv[]) {
    volatile long v[9]; // 这里需要改为9， 用8无法触发crash
    v[argc] = 42;

    printf("Hello! Is my stack OK?\n");

    return v[argc+1];
}
```

```
https://www.qnx.com/developers/docs/8.0/com.qnx.doc.security.system/topic/manual/stack_protection.html

> By default, qcc implicitly uses the -fstack-protector-strong option. However, when you directly invoke either ntoaarch64-gcc or ntox86_64-gcc, -fstack-protector-strong is not implicitly used.

https://www.qnx.com/developers/docs/7.1/index.html#com.qnx.doc.security.system/topic/manual/compiler_defences.html

[QNX SDP 7.1 - Stack protection](https://www.qnx.com/developers/docs/7.1/index.html#com.qnx.doc.security.system/topic/manual/stack_protection.html)

> By default, qcc implicitly uses the -fstack-protector-strong option. However, when you directly invoke either ntoaarch64-gcc or ntox86_64-gcc, -fstack-protector-strong is not implicitly used.
```

## 7. stack smashing 无法被检查到？

即便是同一段发生了越界访问的代码， stack stashing 未必会被检测到。


### 7.1 写入数据但没变化

向 canary 写入了和 canary 取值相同的内容。 概率较小但也可能出现。 这导致函数 epilogue 时判断为 「函数正常执行， 没破坏 canary 」。

### 7.2 写入数据， 但位置更靠后

canary 是很小一块内存。 

```
正常内存 -> canary 内存 -> 外部内存
```

假设写入到 canary 之外的内存， 也是无法检查到越界访问的。 这时候还是用 Address Sanitizer 和各种 Linter 吧。

### 7.3 函数被 inline

例如 GCC -O2， 会把函数内联到更大的函数中， 于是虽然越界， 但越过的是数组的界、 没有越过函数的界。 

可以手动告诉编译器不要 inline：
```cpp
void __attribute__ ((noinline)) foo( /* args */ )
{
    // Code goes here
}
```

### 7.4 有其他干扰

例如预期发生 stack smashing 的函数中， 使用了 `SPDLOG_INFO` 而不是 `printf` 打印， 需要先展开 `SPDLOG_INFO` 宏。 展开后可能发现它额外分配了一些局部内存， 干扰了原本的 stack smashing 的检测。

## 8. 什么时候会 stack smashing ?

gentoo wiki 给出的例子代码中， 局部数组被越界访问， 这是最经典的场景。

还有一个场景： 违反 ODR (One Definition Rule) 时也可能触发， 例如两个 .cpp 文件中用到同名结构体 `MYRECT`, 它的数据成员数量不同，成员少的那份实例，对应的构造函数如果调用到了成员多的那份结构体的构造函数， 也会出现 stack smashing detected (Linux GCC) 或 Run-Time Check Failure #2 - Stack around the variable 'v' was corrupted (MSVC) 的错误。 

运行报错仅在 Debug 模式出现， 在 Release 模式则不会， 原因有两个：
1） Debug 模式默认开启了 `/RTC1`
2)  MRECT 构造函数没被 inline， 导致链接器找到不匹配的那一份

The function `world()` use `MYRECT`, whose definition should be the 5-member one:
```cpp
// rect.h
struct MYRECT
{
    float x;
    float y;
    float width;
    float height;
    float angle;
    MYRECT()
    {
        printf("hello's MYRECT\n");
    }
    MYRECT(float x, float y, float width, float height, float angle) : x(x), y(y), width(width), height(height), angle(angle) {}
};
```

However, the debugger show us that it uses the 6-member one:
```cpp
// hello.cpp
struct MYRECT
{
    float left;
    float top;
    float right;
    float down;
    float cx;
    float cy;
    MYRECT()
    {
        left = 0, right = 0, top = 0, down = 0, cx = 0, cy = 0;
        printf("rect.h's MYRECT\n");
    }
    MYRECT(float left, float top, float right, float down) : left(left), top(top), right(right), down(down) { cx = (left + right) / 2; cy = (top + down) / 2; }
};
```

The hello.cpp's `MYRECT` is with `sizeof(MYRECT)=6*sizeof(float)`, which is greater than rect.h's `sizeof(MYRECT)=5*sizeof(float)`, thus the callstack is corrupted.


## 9. 总结

1. 程序报错 `*** stack smashing detected***` 不是坏事， 是好事， 最应该检查的是局部数组被越界写入， 其次检查 ODR 被违反的情况

2. A 模块调用 B 模块出现 `*** stack smashing detected***`, B 模块测试程序无法复现， 也是正常的， 取决于具体调用代码， 例如是否被 inline， 是否有宏方式的调用干扰了栈分配等。

3. MSVC 换了叫法， 叫 run time checker， 原理基本一样。

4. arm64 的汇编暂未有时间分析， 原理基本一样。

5. 真正的 AI 编译器， 应该在编译发现坏代码时给出提示， 而不是搞什么 “只选择需要的 .c/.o 编译出模型最小依赖的二进制”。

## References

- 1. https://wiki.osdev.org/Stack_Smashing_Protector
- 2. https://wiki.gentoo.org/wiki/Stack_smashing_debugging_guide
- 3. https://learn.microsoft.com/en-us/cpp/build/reference/rtc-run-time-error-checks?view=msvc-170
- 4. https://www.cnblogs.com/arnoldlu/p/11630979.html
- 5. https://xuleilx.github.io/2020/11/27/经验分享：gcc编译参数stack-protector/
- 6. https://xuleilx.github.io/2020/11/27/关于StackSmashingDetected问题调查/
- 7. https://stackoverflow.com/questions/1629685/when-and-how-to-use-gccs-stack-protection-feature
- 8. https://www.cnblogs.com/leo0000/p/5719186.html
- 9. https://xuleilx.github.io/2020/11/27/经验分享：gcc编译参数stack-protector/
- 10. https://mudongliang.github.io/2016/05/24/stack-protector.html
- 11. https://outflux.net/blog/archives/2014/01/27/fstack-protector-strong/
- 12. https://developers.redhat.com/articles/2022/06/02/use-compiler-flags-stack-protection-gcc-and-clang