#include <stdio.h>

struct Unpacked {
  char a;
  int b;
  char c;
};

struct Packed {
  char a;
  char c;
  int b;
};

int
main() {
  printf("sizeof(struct Unpacked) = %lu\n", sizeof(struct Unpacked));
  printf("sizeof(struct Packed) = %lu\n", sizeof(struct Packed));
  return 0;
}
