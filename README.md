# Mirage-Compiler

简单实现的 C 语言编译器，用于本人学习，以能编译真实世界项目为目标。

目标平台为 Windows，输出汇编为 x86 intel，支持 C23 标准。

## C 语言扩展

1. 语句表达式

```c
// int b = 3; int a = b;

int a = ({ int b = 3; b; });
```

2. [GNU] 允许`_Alignof`接受变量为操作数  

## References

- [chibicc:](https://github.com/rui314/chibicc) A small C compiler
