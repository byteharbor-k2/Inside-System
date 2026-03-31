int return_1() {
  int x;
  for (int i = 0; i < 100; i++) {
    // Compiler will assign [%0] an assembly operand
    asm("movl $1, %0" : "=g"(x));  // "x = 1;"
  }
  return x;
}
