#include "threadpool.h"
#include <stdio.h>

void task(void *arg) {
  int *num = arg;
  printf("Task %d from thread: %lu\n", *num, (unsigned long) pthread_self());
}


int main() {
  threadpool_t *tp = threadpool_create(4);
  int nums[100] = {};

  for (int i = 0; i < 100; i++) {
    nums[i] = i;
    add_work(tp, task, &nums[i]); 
  }
  threadpool_await(tp);
  threadpool_destroy(tp);
  return 0;
}
