#include "../include/profiler.h"

uint64_t add(int x, int y) {
  uint64_t res = 0;
  for (int i = 0; i < 1e5; ++i) {
    res = x * y;
  }
  return res;
}

int main() {
  MiniProfiler pProfiler;

  for (int i = 0, j = 1; i < 1e3 && j < 1e3; ++i, j += 2) {
    add(i, j);
  }

  return 0;
}