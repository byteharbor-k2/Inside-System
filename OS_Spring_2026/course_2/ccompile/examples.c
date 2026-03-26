int f1() {
    int x = 2 * 3 * 4;
    if (0) return x;
    return x + 1;
}

int f2(int a, int b) {
    int x = a*b + a*b;
    return x;
}

int f3(int *p, int n) {
    int s = 0;
    for (int i = 0; i < n; i++) s += p[0];
    return s;
}

extern int x;
void f4() {
    x = 1;
    x = 1;
}

extern int x;
extern void g();
void f5() {
    x = 1;
    g();
    x = 1;
    g();
}
