# Mirage-Compiler

简单实现的 C 语言编译器，用于本人学习，以能编译真实世界项目为目标。

目标平台为 Windows，输出汇编为 x86 intel，最高支持 C99 标准。

## C 语言扩展

1. 语句表达式

```c
int a = ({ int b = 3; b; }); // 3
```

2. 自动注册类名
```c
struct t { int x; }; t y = {};
```

## References

- [chibicc:](https://github.com/rui314/chibicc) A small C compiler
