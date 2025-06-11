# stack smashing

## 1. 最小复现: Linux-x64 GCC

程序运行挂了，临终前输出一句：

> *** stack smashing detected ***: terminated

检查 GCC 编译选项， 使用了如下三个之一：

```bash
-fstack-protector-all
-fstack-protector-explicit
-fstack-protector-strong
```

典型情况是，访问局部数组时发生越界, e.g. (参考链接[1]):

```c
#include <stdio.h>

// $ gcc a.c -o a -fstack-protector-all
// $ ./a 1 2 3 4 5 6 7 8
// *** stack smashing detected ***: terminated
int main(int argc, char * argv[]) {
    volatile long v[8];
    v[argc] = 42;

    printf("Hello! Is my stack OK?\n");

    return v[argc+1];
}
```

## 2. GCC 默认开启 -fstack-protector-xxx 编译选项吗？

查询方法：
```bash
gcc -Q --help=common | grep 'stack-protector'
```

一些参考结果：

| flags                      | gcc 7.5    | gcc 9.4    | gcc 11.4   |
| -------------------------- | ---------- | ---------- | ---------- |
| -Wstack-protector          | [disabled] | [disabled] | [disabled] |
| -fstack-protector          | [disabled] | [disabled] | [disabled] |
| -fstack-protector-all      | [disabled] | [disabled] | [disabled] |
| -fstack-protector-explicit | [disabled] | [disabled] | [disabled] |
| -fstack-protector-strong   | [disabled] | [disabled] | [enabled]  |

TODO: clang 结果？
TODO: apple clang 结果?

## 3. MSVC 有对应选项吗？

MSVC 有类似的编译选项， `/RTCs`: (参考链接[2])

> Enables stack frame run-time error checking, as follows:

> Detection of overruns and underruns of local variables such as arrays

## 4. qnx710 现象

QNX SDK 7.1 是基于 GCC 的编译器。
TODO:

## References

- [1] [gentoo wiki - Stack_smashing_debugging_guide](https://wiki.gentoo.org/wiki/Stack_smashing_debugging_guide)
- [2] [MSVC - run time error checks](https://learn.microsoft.com/en-us/cpp/build/reference/rtc-run-time-error-checks?view=msvc-170)
- [3] [Arnold Lu@南京 - gcc栈溢出保护机制：stack-protector](https://www.cnblogs.com/arnoldlu/p/11630979.html)

## Remain

- [](https://xuleilx.github.io/2020/11/27/经验分享：gcc编译参数stack-protector/)
- [](https://xuleilx.github.io/2020/11/27/关于StackSmashingDetected问题调查/)

---

- [](https://stackoverflow.com/questions/1629685/when-and-how-to-use-gccs-stack-protection-feature)

-Wstack-protector

This flag warns about functions that will not be protected against stack smashing even though -fstack-protector is enabled. GCC emits some warnings when building the project:

> not protecting function: no buffer at least 8 bytes long
> not protecting local variables: variable length buffer

---

https://developers.redhat.com/articles/2022/06/02/use-compiler-flags-stack-protection-gcc-and-clang


stack smashing 的 smashing 什么意思？ 粉碎。

---

https://www.qnx.com/developers/docs/8.0/com.qnx.doc.security.system/topic/manual/stack_protection.html

> By default, qcc implicitly uses the -fstack-protector-strong option. However, when you directly invoke either ntoaarch64-gcc or ntox86_64-gcc, -fstack-protector-strong is not implicitly used.

---

https://www.qnx.com/developers/docs/7.1/index.html#com.qnx.doc.security.system/topic/manual/compiler_defences.html

[QNX SDP 7.1 - Stack protection](https://www.qnx.com/developers/docs/7.1/index.html#com.qnx.doc.security.system/topic/manual/stack_protection.html)

> By default, qcc implicitly uses the -fstack-protector-strong option. However, when you directly invoke either ntoaarch64-gcc or ntox86_64-gcc, -fstack-protector-strong is not implicitly used.

qcc 自动提供 -fstack-protector-strong 选项， ntoaarch64-gcc 则不会提供这一选项。

https://github.com/conan-io/conan/issues/15752

```cmake
set(CMAKE_C_COMPILER ${QNX_HOST_BIN}/qcc)
set(CMAKE_CXX_COMPILER ${QNX_HOST_BIN}/q++)
```