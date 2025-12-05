#include "../work_stealing/ws_threadpool.h"
#include <unistd.h>

void task1(void *arg) {
  int *num = (int *) arg;
  printf("Executing task %d on thread: %lu\n", *num, (unsigned long) pthread_self());
}

void task2(void *arg) {
  int *num = (int *) arg;
  printf("At depth %d on thread %lu\n", *num, (unsigned long) pthread_self());
  int cntr = 0;
  for (int i = 0; i < 100000; i++) {
    cntr++;
  }
  usleep(5000);
  if (*num > 0) {
    int *new_depth = malloc(sizeof(int));
    int *other_depth = malloc(sizeof(int));
    *new_depth = *num - 1;
    *other_depth = *num - 1;
    threadpool_spawn(task2, new_depth);
    threadpool_spawn(task2, other_depth);
  }
  free(arg);
}




int main() {
  const int threads = 4;
  const int tasks = 100;
  threadpool_t *tp = threadpool_create(threads);

  printf("===TEST: 1 - SIMPLE PRINTS START ===\n");
  for (int i = 0; i < tasks; i++) {
    int *nums = malloc(sizeof(int));
    *nums = i;

    threadpool_submit(tp, task1, nums);
  }

  threadpool_await(tp);

  printf("===TEST: 1 - SIMPLE PRINTS END ===\n");


  printf("===TEST: 2 - RECURSIVE TASK START ===\n");

  int *depth = malloc(sizeof(int));
  *depth = 10;
  threadpool_submit(tp, task2, depth);

  threadpool_await(tp);

  printf("===TEST: 2 -- RECURSIVE TASK END ===\n");
  threadpool_destroy(tp);
  return 0;
}
