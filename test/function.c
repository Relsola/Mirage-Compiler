#include "test.h"

int ret3() {
  return 3;
  return 5;
}

int add2(int x, int y) {
  return x + y;
}

int sub2(int x, int y) {
  return x - y;
}

int add4(int a, int b, int c, int d) {
  return a + b + c + d;
}

int addx(int *x, int y) {
  return *x + y;
}

int sub_char(char a, char b, char c) {
  return a - b - c;
}

int fib(int x) {
  if (x<=1)
    return 1;
  return fib(x-1) + fib(x-2);
}

int sub_long(long a, long b, long c) {
  return a - b - c;
}

int sub_short(short a, short b, short c) {
  return a - b - c;
}

int main() {
  ASSERT(3, ret3());
  ASSERT(8, add2(3, 5));
  ASSERT(2, sub2(5, 3));
  ASSERT(10, add4(1,2,3,4));
  ASSERT(28, add4(1,2,add4(3,4,5,6),7));
  ASSERT(55, add4(1,2,add4(3,add4(4,5,6,7),8,9),10));
  ASSERT(3, ({ int x = 1; addx(&x, 2); }));

  ASSERT(7, add2(3,4));
  ASSERT(1, sub2(4,3));
  ASSERT(55, fib(9));

  ASSERT(1, ({ sub_char(7, 3, 3); }));

  ASSERT(1, sub_long(7, 3, 3));
  ASSERT(1, sub_short(7, 3, 3));

  printf("OK\n");
  return 0;
}
