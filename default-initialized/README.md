# 默认初始化

## 1. 概要

变量定义时候没有赋值， 取值会是什么？
- 全局变量： zero-initialized
- 函数内的局部变量： 垃圾值
- 类的成员变量： 垃圾值

所谓垃圾值， 对于特定编译器或编译选项， 仍然是有规律可循的。

不同的整型变量（有无符号，长度）有不同的取值， 用字符串数组则更加直观感受取值：
- 烫烫烫烫 : 填充的是 0xcc
- 屯屯屯屯 : 填充的是 0xcd
- 揪揪揪揪 : 填充的是 0xbe

## 2. 代码

定义测试类， 成员不做初始化； 提供成员函数打印每个 data member 取值和取值的十六进制
```cpp
struct Info
{
	uint8_t u8;
	int8_t i8;

	uint16_t u16;
	int16_t i16;

	uint32_t u32;
	int32_t i32;

	uint64_t u64;
	int64_t i64;

	float f32;
	double f64;

	char name[10];
	Info()
	{
		name[10 - 1] = '\0';
	}

	void print()
	{
        std::cout << "u8: " << (int)u8 << " [" << get_hex_string(u8) << "]" << std::endl;
        std::cout << "i8: " << (int)i8 << " [" << get_hex_string(i8) << "]" << std::endl;
        std::cout << "u16: " << u16 << " [" << get_hex_string(u16) << "]" << std::endl;
        std::cout << "i16: " << i16 << " [" << get_hex_string(i16) << "]" << std::endl;
        std::cout << "u32: " << u32 << " [" << get_hex_string(u32) << "]" << std::endl;
        std::cout << "i32: " << i32 << " [" << get_hex_string(i32) << "]" << std::endl;
        std::cout << "u64: " << u64 << " [" << get_hex_string(u64) << "]" << std::endl;
        std::cout << "i64: " << i64 << " [" << get_hex_string(i64) << "]" << std::endl;
        std::cout << "f32: " << f32 << " [" << get_hex_string(f32) << "]" << std::endl;
        std::cout << "f64: " << f64 << " [" << get_hex_string(f64) << "]" << std::endl;
        std::cout << "name: " << name << " [" << get_hex_string(name) << "]" << std::endl;
	}
};
```

调用上述类： 实例分别位于栈和堆，观察取值
```cpp
int main()
{
	{
		Info info;
		std::cout << "----------------------------------------------------\n";
		std::cout << "struct is on stack. default value of data members:\n";
		info.print();
	}

	{
		Info* info = new Info();
		std::cout << "----------------------------------------------------\n";
		std::cout << "struct is on heap. default value of data members:\n";
		info->print();
		delete info;
	}

	return 0;
}
```

## Debug

```
----------------------------------------------------
struct is on stack. default value of data members:
u8: 204 [cc]
i8: -52 [cc]
u16: 52428 [cc cc]
i16: -13108 [cc cc]
u32: 3435973836 [cc cc cc cc]
i32: -858993460 [cc cc cc cc]
u64: 14757395258967641292 [cc cc cc cc cc cc cc cc]
i64: -3689348814741910324 [cc cc cc cc cc cc cc cc]
f32: -1.07374e+08 [cc cc cc cc]
f64: -9.25596e+61 [cc cc cc cc cc cc cc cc]
name: 烫烫烫烫?[00 cc cc cc cc cc cc cc cc cc]
----------------------------------------------------
struct is on heap. default value of data members:
u8: 205 [cd]
i8: -51 [cd]
u16: 52685 [cd cd]
i16: -12851 [cd cd]
u32: 3452816845 [cd cd cd cd]
i32: -842150451 [cd cd cd cd]
u64: 14829735431805717965 [cd cd cd cd cd cd cd cd]
i64: -3617008641903833651 [cd cd cd cd cd cd cd cd]
f32: -4.31602e+08 [cd cd cd cd]
f64: -6.27744e+66 [cd cd cd cd cd cd cd cd]
name: 屯屯屯屯?[00 cd cd cd cd cd cd cd cd cd]
```

## Release

on-heap values are random.

```
----------------------------------------------------
struct is on stack. default value of data members:
u8: 0 [00]
i8: 0 [00]
u16: 0 [00 00]
i16: 0 [00 00]
u32: 0 [00 00 00 00]
i32: 0 [00 00 00 00]
u64: 53 [00 00 00 00 00 00 00 35]
i64: 0 [00 00 00 00 00 00 00 00]
f32: -2.32559e+34 [f8 8f 53 58]
f64: 6.95165e-310 [00 00 7f f7 f8 8f 3e 85]
name:  [00 00 00 00 00 00 00 00 00 07]
----------------------------------------------------
struct is on heap. default value of data members:
u8: 84 [54]
i8: 0 [00]
u16: 82 [00 52]
i16: 73 [00 49]
u32: 3997767 [00 3d 00 47]
i32: 6619204 [00 65 00 44]
u64: 30399800002281574 [00 6c 00 75 00 61 00 66]
i64: 18859128382292084 [00 43 00 47 00 00 00 74]
f32: 1.10204e-38 [00 78 00 45]
f64: 1.11261e-306 [00 69 00 66 00 6e 00 6f]
name: g [00 64 00 65 00 73 00 55 00 67]
```

## Debug + ASAN

```
----------------------------------------------------
struct is on stack. default value of data members:
u8: 0 [00]
i8: 0 [00]
u16: 0 [00 00]
i16: 0 [00 00]
u32: 0 [00 00 00 00]
i32: 0 [00 00 00 00]
u64: 0 [00 00 00 00 00 00 00 00]
i64: 0 [00 00 00 00 00 00 00 00]
f32: 0 [00 00 00 00]
f64: 0 [00 00 00 00 00 00 00 00]
name:  [00 00 00 00 00 00 00 00 00 00]
----------------------------------------------------
struct is on heap. default value of data members:
u8: 190 [be]
i8: -66 [be]
u16: 48830 [be be]
i16: -16706 [be be]
u32: 3200171710 [be be be be]
i32: -1094795586 [be be be be]
u64: 13744632839234567870 [be be be be be be be be]
i64: -4702111234474983746 [be be be be be be be be]
f32: -0.372549 [be be be be]
f64: -1.83255e-06 [be be be be be be be be]
name: 揪揪揪揪?[00 be be be be be be be be be]
```

## Release + ASAN

```
----------------------------------------------------
struct is on stack. default value of data members:
u8: 0 [00]
i8: 0 [00]
u16: 0 [00 00]
i16: 0 [00 00]
u32: 0 [00 00 00 00]
i32: 0 [00 00 00 00]
u64: 0 [00 00 00 00 00 00 00 00]
i64: 140728076627688 [00 00 7f fd cf 04 6e e8]
f32: 1.41901e-22 [1b 2b 8c 60]
f64: 0 [00 00 00 00 00 00 00 00]
name:  [00 e8 00 00 00 00 00 00 00 00]
----------------------------------------------------
struct is on heap. default value of data members:
u8: 190 [be]
i8: -66 [be]
u16: 48830 [be be]
i16: -16706 [be be]
u32: 3200171710 [be be be be]
i32: -1094795586 [be be be be]
u64: 13744632839234567870 [be be be be be be be be]
i64: -4702111234474983746 [be be be be be be be be]
f32: -0.372549 [be be be be]
f64: -1.83255e-06 [be be be be be be be be]
name: 揪揪揪揪?[00 be be be be be be be be be]
```