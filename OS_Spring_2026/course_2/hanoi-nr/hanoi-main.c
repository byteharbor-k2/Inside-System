#include <stdio.h>
#include <assert.h>

int hanoi(int n, char from, char to, char via);

int main() {
    int n = 4;
    char from = 'A';
    char to = 'B';
    char via = 'C';

    int step_count = hanoi(n, from, to, via);
    // int step_count = nHanoi(n, form, to ,via);
    // int step_count = hanoi_for_loop(n, form, to ,via);
    printf("\nHanoi(%d, %c, %c, %c) = %d\n",
        n, from, to, via, step_count);
}
