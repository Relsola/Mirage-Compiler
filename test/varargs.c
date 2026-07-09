#include "test.h"
#include <stdarg.h>

int sum1(int x, ...) {
  va_list ap;
  va_start(ap, x);

  for (;;) {
    int y = va_arg(ap, int);
    if (y == 0)
      return x;
    x += y;
  }
}

int sum2(double x, ...) {
  va_list ap;
  va_start(ap, x);

  for (;;) {
    double y = va_arg(ap, double);
    x += y;

    int z = va_arg(ap, int);
    if (z == 0)
      return x;
    x += z;
  }
}

int sum3(int x, ...) {
  va_list ap;
  va_start(ap, x);

  va_list ap2;
  va_copy(ap2, ap);

  for (;;) {
    int y = va_arg(ap, int);
    int z = va_arg(ap2, int);
    if (y == 0 || z == 0)
      return x;
    x = x + y + z;
  }
}


int main() {
  ASSERT(6, sum1(1, 2, 3, 0));
  ASSERT(22, sum2(1, 2.4, 3, 4.4, 5, 6.2, 0));
  ASSERT(11, sum3(1, 2, 3, 0));

  printf("OK\n");
  return 0;
}
