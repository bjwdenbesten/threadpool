#ifndef WS_THREADPOOL
#define WS_THREADPOOL

#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>

#define DEQUEUE_CAP 4096


typedef void (*threadpool_func_t) (void *arg);
typedef struct work work_t;
typedef struct threadpool threadpool_t;
typedef struct worker worker_t;

struct work {
  threadpool_func_t func;
  void *arg;
  work_t *next;
};

// steal from top, worker takes from bottom
struct worker {
  _Atomic(work_t*) *dequeue;
  atomic_size_t top;
  atomic_size_t bot;
  threadpool_t *tp;
  pthread_t thread_id;
};

struct threadpool {
  work_t *first;
  work_t *last;
  pthread_t *threads;
  worker_t **workers;
  size_t num_workers;
  pthread_cond_t has_work;
  pthread_mutex_t mutex;
  bool shutdown;

  atomic_size_t pending;  
  pthread_cond_t empty;
  pthread_mutex_t pending_mutex;
};


/* local storage for current worker */
extern _Thread_local worker_t *current_worker;

/* thread pool functions */
threadpool_t *threadpool_create(size_t n);
void threadpool_destroy(threadpool_t *tp);
void threadpool_submit(threadpool_t *tp, threadpool_func_t, void *arg);
void threadpool_await(threadpool_t *tp);

/* function to spawn a sub task */
void threadpool_spawn(threadpool_func_t func, void *arg);

/*dequeue functions */
bool dequeue_push(worker_t *worker, work_t *work);
bool dequeue_pop(worker_t *worker, work_t **ret);
bool dequeue_steal(worker_t *vic, work_t **ret);

work_t *get_work(threadpool_t *tp);


/* worker function */
void *work_func(void *arg);



#endif //WS_THREADPOOL
