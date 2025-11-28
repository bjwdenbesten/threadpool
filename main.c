#include "threadpool.h"
#include <stdio.h>
#include <unistd.h>

void task1(void *arg) {
  int *num = arg;
  printf("Task %d from thread: %lu\n", *num, (unsigned long) pthread_self());
}

void task2(void *arg) {
  int sum = 0;
  for (int i = 0; i < 100; i++) {
    sum += i;
  }
  printf("Task %d from thread: %lu with sum: %d\n", *(int *)arg, (unsigned long) pthread_self(), sum);
}


struct args {
  int *counter;
  pthread_mutex_t *mutex;
};


void task3(void *arg) {
  struct args *a = arg;
  pthread_mutex_lock(a->mutex);
  *(a->counter) = *(a->counter) + 1;
  int val = *(a->counter);
  pthread_mutex_unlock(a->mutex);
  printf("Incremented counter to: %d\n", val);
}

void task4(void *arg) {
  int *id = arg;
  printf("Doing task: %d with thread %lu\n", *id, (unsigned long) pthread_self());
  sleep(2);
  printf("Finished doing task: %d with thread %lu\n", *id, (unsigned long) pthread_self());
}



int main() {
  const int threads = 4;
  const int tasks = 100;
  threadpool_t *tp = threadpool_create(threads);

  printf("=== TEST: 1 - SIMPLE PRINTS START ===\n");
  int nums[100] = {};
  for (int i = 0; i < tasks; i++) {
    nums[i] = i;
    add_work(tp, task1, &nums[i]); 
  }
  threadpool_await(tp);
  printf("=== TEST: 1 - SIMPLE PRINTS END ===\n");


  printf("=== TEST: 2 - SUMS START ===\n");
  for (int i = 0; i < tasks; i++) {
    add_work(tp, task2, &nums[i]);
  }

  threadpool_await(tp);
  printf("=== TEST: 2 - SUMS END ===\n");


  printf("=== TEST: 3 - CONCURRENCY START ===\n");
  pthread_mutex_t mutex;
  pthread_mutex_init(&mutex, NULL);
  int counter = 0;
  struct args arg = {&counter, &mutex};

  for (int i = 0; i < tasks; i++) {
    add_work(tp, task3, &arg);
  }

  threadpool_await(tp);
  printf("Counter at the end of Test 3: %d\n", *(arg.counter));
  printf("=== TEST: 3 - CONCURRENCY END ===\n");


  printf("=== TEST: 4 - SLOW TASK START ===\n");
  for (int i = 0; i < tasks; i++) {
    add_work(tp, task4, &nums[i]);
  }
  threadpool_await(tp);
  printf("=== TEST: 4 - SLOW TASK END ===\n");

  threadpool_destroy(tp);
  return 0;
}
