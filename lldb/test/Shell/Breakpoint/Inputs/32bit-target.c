
void foo(int *x) { *x = 1; }

int main() {
  int x = 0;
  foo(&x);
}
