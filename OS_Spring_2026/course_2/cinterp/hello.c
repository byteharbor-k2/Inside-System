#include <stdio.h>
#include <stdarg.h>

void print_str(const char *s) {
    char c = *s;
    while (c != '\0') {
        putchar(c);
        s = s + 1;
        c = *s;
    }
}

void print_num(int n) {
    int q, r, digit;
    if (n < 0) {
        putchar('-');
        n = -n;
    }
    q = n / 10;
    if (q != 0) {
        print_num(q);
    }
    r = n % 10;
    digit = '0' + r;
    putchar(digit);
}

void myprintf(const char *fmt, ...) {
    va_list ap;
    char c;
    const char *s;
    int d;

    va_start(ap, fmt);

    while (1) {
        c = *fmt;
        if (c == '\0') {
            break;
        }
        if (c != '%') {
            putchar(c);
            fmt = fmt + 1;
            continue;
        }
        fmt = fmt + 1;
        c = *fmt;
        if (c == '\0') {
            putchar('%');
            break;
        }
        if (c == 's') {
            s = va_arg(ap, const char *);
            print_str(s);
            fmt = fmt + 1;
            continue;
        }
        if (c == 'd') {
            d = va_arg(ap, int);
            print_num(d);
            fmt = fmt + 1;
            continue;
        }
        putchar('%');
        putchar(c);
        fmt = fmt + 1;
    }

    va_end(ap);
}

int main(void) {
    myprintf("The answer is %d\n", 42);
    return 0;
}
