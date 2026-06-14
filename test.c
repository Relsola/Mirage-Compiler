#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>

int assert_expr(int expected, const char *input)
{
    char cmd[1024];
    int actual;

    snprintf(cmd, sizeof(cmd), "mirage.exe \"%s\" > tmp.s", input);
    if (system(cmd) != 0) {
        exit(1);
    }

    if (system("clang -static -o tmp.exe tmp.s tmp2.o") != 0) {
        exit(1);
    }

    actual = system("tmp.exe");
    if (actual == expected) {
        printf("%s => %d\n", input, actual);
        fflush(stdout);
    } else {
        printf("%s => %d expected, but got %d\n", input, expected, actual);
        fflush(stdout);
        exit(1);
    }

    return 0;
}

#define assert(expected, ...) assert_expr(expected, #__VA_ARGS__)

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    FILE *fp = fopen("tmp2.c", "w");
    if (fp == NULL) {
        exit(1);
    }
    fputs(
        "int ret3() { return 3; }\n"
        "int ret5() { return 5; }\n"
        "int add(int x, int y) { return x+y; }\n"
        "int sub(int x, int y) { return x-y; }\n"
        "int add4(int a, int b, int c, int d) {\n"
        "  return a+b+c+d;\n"
        "}\n",
        fp);
    fclose(fp);

    if (system("clang -xc -c -o tmp2.o tmp2.c") != 0) {
        exit(1);
    }

    assert(0, int main() { return 0; });
    assert(42, int main() { return 42; });
    assert(21, int main() { return 5+20-4; });
    assert(41, int main() { return  12 + 34 - 5 ; });
    assert(47, int main() { return 5+6*7; });
    assert(15, int main() { return 5*(9-6); });
    assert(4, int main() { return (3+5)/2; });
    assert(10, int main() { return -10+20; });
    assert(10, int main() { return - -10; });
    assert(10, int main() { return - - +10; });

    assert(0, int main() { return 0==1; });
    assert(1, int main() { return 42==42; });
    assert(1, int main() { return 0!=1; });
    assert(0, int main() { return 42!=42; });

    assert(1, int main() { return 0<1; });
    assert(0, int main() { return 1<1; });
    assert(0, int main() { return 2<1; });
    assert(1, int main() { return 0<=1; });
    assert(1, int main() { return 1<=1; });
    assert(0, int main() { return 2<=1; });

    assert(1, int main() { return 1>0; });
    assert(0, int main() { return 1>1; });
    assert(0, int main() { return 1>2; });
    assert(1, int main() { return 1>=0; });
    assert(1, int main() { return 1>=1; });
    assert(0, int main() { return 1>=2; });

    assert(3, int main() { int a; a=3; return a; });
    assert(3, int main() { int a=3; return a; });
    assert(8, int main() { int a=3; int z=5; return a+z; });

    assert(3, int main() { int a=3; return a; });
    assert(8, int main() { int a=3; int z=5; return a+z; });
    assert(6, int main() { int a; int b; a=b=3; return a+b; });
    assert(3, int main() { int foo=3; return foo; });
    assert(8, int main() { int foo123=3; int bar=5; return foo123+bar; });

    assert(1, int main() { return 1; 2; 3; });
    assert(2, int main() { 1; return 2; 3; });
    assert(3, int main() { 1; 2; return 3; });

    assert(3, int main() { {1; {2;} return 3;} });
    assert(5, int main() { ;;; return 5; });

    assert(3, int main() { if (0) return 2; return 3; });
    assert(3, int main() { if (1-1) return 2; return 3; });
    assert(2, int main() { if (1) return 2; return 3; });
    assert(2, int main() { if (2-1) return 2; return 3; });
    assert(4, int main() { if (0) { 1; 2; return 3; } else { return 4; } });
    assert(3, int main() { if (1) { 1; 2; return 3; } else { return 4; } });

    assert(55, int main() { int i=0; int j=0; for (i=0; i<=10; i=i+1) j=i+j; return j; });
    assert(3, int main() { for (;;) return 3; return 5; });

    assert(10, int main() { int i=0; while(i<10) i=i+1; return i; });

    assert(3, int main() { {1; {2;} return 3;} });

    assert(10, int main() { int i=0; while(i<10) i=i+1; return i; });
    assert(55, int main() { int i=0; int j=0; while(i<=10) {j=i+j; i=i+1;} return j; });

    assert(3, int main() { int x=3; return *&x; });
    assert(3, int main() { int x=3; int *y=&x; int **z=&y; return **z; });
    assert(5, int main() { int x=3; int y=5; return *(&x+1); });
    assert(3, int main() { int x=3; int y=5; return *(&y-1); });
    assert(5, int main() { int x=3; int y=5; return *(&x-(-1)); });
    assert(5, int main() { int x=3; int *y=&x; *y=5; return x; });
    assert(7, int main() { int x=3; int y=5; *(&x+1)=7; return y; });
    assert(7, int main() { int x=3; int y=5; *(&y-2+1)=7; return x; });
    assert(5, int main() { int x=3; return (&x+2)-&x+3; });
    assert(8, int main() { int x, y; x=3; y=5; return x+y; });
    assert(8, int main() { int x=3, y=5; return x+y; });

    assert(3, int main() { return ret3(); });
    assert(5, int main() { return ret5(); });
    assert(8, int main() { return add(3, 5); });
    assert(2, int main() { return sub(5, 3); });
    assert(10, int main() { return add4(1,2,3,4); });
    assert(28, int main() { return add4(1,2,add4(3,4,5,6),7); });
    assert(55, int main() { return add4(1,2,add4(3,add4(4,5,6,7),8,9),10); });

    assert(32, int main() { return ret32(); } int ret32() { return 32; });
    assert(7, int main() { return add2(3,4); } int add2(int x, int y) { return x+y; });
    assert(1, int main() { return sub2(4,3); } int sub2(int x, int y) { return x-y; });
    assert(55, int main() { return fib(9); } int fib(int x) { if (x<=1) return 1; return fib(x-1) + fib(x-2); });

    assert(3, int main() { int x[2]; int *y=&x; *y=3; return *x; });

    assert(3, int main() { int x[3]; *x=3; *(x+1)=4; *(x+2)=5; return *x; });
    assert(4, int main() { int x[3]; *x=3; *(x+1)=4; *(x+2)=5; return *(x+1); });
    assert(5, int main() { int x[3]; *x=3; *(x+1)=4; *(x+2)=5; return *(x+2); });

    assert(0, int main() { int x[2][3]; int *y=x; *y=0; return **x; });
    assert(1, int main() { int x[2][3]; int *y=x; *(y+1)=1; return *(*x+1); });
    assert(2, int main() { int x[2][3]; int *y=x; *(y+2)=2; return *(*x+2); });
    assert(3, int main() { int x[2][3]; int *y=x; *(y+3)=3; return **(x+1); });
    assert(4, int main() { int x[2][3]; int *y=x; *(y+4)=4; return *(*(x+1)+1); });
    assert(5, int main() { int x[2][3]; int *y=x; *(y+5)=5; return *(*(x+1)+2); });


    assert(3, int main() { int x[3]; *x=3; x[1]=4; x[2]=5; return *x; });
    assert(4, int main() { int x[3]; *x=3; x[1]=4; x[2]=5; return *(x+1); });
    assert(5, int main() { int x[3]; *x=3; x[1]=4; x[2]=5; return *(x+2); });
    assert(5, int main() { int x[3]; *x=3; x[1]=4; x[2]=5; return *(x+2); });
    assert(5, int main() { int x[3]; *x=3; x[1]=4; 2[x]=5; return *(x+2); });

    assert(0, int main() { int x[2][3]; int *y=x; y[0]=0; return x[0][0]; });
    assert(1, int main() { int x[2][3]; int *y=x; y[1]=1; return x[0][1]; });
    assert(2, int main() { int x[2][3]; int *y=x; y[2]=2; return x[0][2]; });
    assert(3, int main() { int x[2][3]; int *y=x; y[3]=3; return x[1][0]; });
    assert(4, int main() { int x[2][3]; int *y=x; y[4]=4; return x[1][1]; });
    assert(5, int main() { int x[2][3]; int *y=x; y[5]=5; return x[1][2]; });

    assert(8, int main() { int x; return sizeof(x); });
    assert(8, int main() { int x; return sizeof x; });
    assert(8, int main() { int *x; return sizeof(x); });
    assert(32, int main() { int x[4]; return sizeof(x); });
    assert(96, int main() { int x[3][4]; return sizeof(x); });
    assert(32, int main() { int x[3][4]; return sizeof(*x); });
    assert(8, int main() { int x[3][4]; return sizeof(**x); });
    assert(9, int main() { int x[3][4]; return sizeof(**x) + 1; });
    assert(9, int main() { int x[3][4]; return sizeof **x + 1; });
    assert(8, int main() { int x[3][4]; return sizeof(**x + 1); });
    assert(8, int main() { int x=1; return sizeof(x=2); });
    assert(1, int main() { int x=1; sizeof(x=2); return x; });

    printf("OK\n");
    return 0;
}
