# include <stdio.h>
# include <stdlib.h>

#define MAX_STACK_DEEP 1024

typedef struct {
    int n;
    char from;
    char to;
    char tmp;
    int stage;
} Frame;

// n 表示要搬多少个圆盘从from(char) 到 to(char), 借助辅助柱子 tmp(char), stage 表示在当前栈帧中处于什么状态
int hanoi_for_loop(int n, char from, char to ,char tmp) {
    int count = 0;
    int top = -1;
    int stage=0;
    if (n <= 0) {
        printf("No actions available. n <= 0");
        return count;
    }
    Frame stack[MAX_STACK_DEEP];
    //先压栈， 最顶层
    stack[++top] = (Frame){n, from, to, tmp, stage};
    while (top >= 0){
        //处理当前帧， 栈顶
        Frame *cur = &stack[top];
        //这是最简单的情况， 只有一个盘子， 直接从from 搬到 to ，操作数count++ 栈弹出（top--）
        if (cur->n == 1) {
            printf("from %c move to %c \n", cur->from, cur->to);
            count++;
            top--;//相当于递归返回
            continue;
        } 

        if (cur->stage == 0) {
            //刚开始的状态，类似左递归 先将n-1个盘子从from 借助to 搬到tmp(创建新的栈帧 压入栈，栈顶指针top++) 注意所有的压栈行为都是从stage 0开始的
            stack[++top] = (Frame){cur->n - 1, cur->from, cur->tmp, cur->to, 0};
            cur->stage = 1;
        } else if (cur->stage == 1) {
            //将最大的盘子从from 搬到 to， 操作数count++
            printf("from %c move to %c \n", cur->from, cur->to);
            count++;
            cur->stage = 2;
        } else if (cur->stage == 2) {
            //类似右递归 将n-1 个盘子从tmp 借助from 搬到to(创建新的栈帧 压入栈，栈顶指针top++)
            stack[++top] = (Frame){cur->n - 1, cur->tmp, cur->to, cur->from, 0};
            cur->stage = 3;
        } else if (cur->stage == 3) {
            //类似当前递归结束， 栈pop一帧
            top--;
        }
    }
    return count;
}
int main() {
    //目标是从A 搬三个圆盘到 B ，C是辅助柱子
    int op_count = hanoi_for_loop(3, 'A', 'B', 'C');
    printf("total op_count is %d \n", op_count);
    return 0;
}
