#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>

typedef void (*threadpool_func_t) (void *arg);
typedef struct work work_t;
typedef struct threadpool threadpool_t;


/* struct that will hold all threadpool related data*/
struct threadpool {
  work_t *head;
  work_t *tail;
  pthread_mutex_t mutex;
  pthread_cond_t work_cond;
  pthread_cond_t workers_cond;
  pthread_t *thread_ids;
  size_t working;
  size_t num_threads;
  bool shutdown;
};

/* represents a singular link in the work queue */
struct work {
  threadpool_func_t func;
  void *arg;
  work_t *next;
};

/* threadpool functions */
threadpool_t *threadpool_create(size_t num_threads);
void threadpool_destroy(threadpool_t *tp);
void threadpool_await(threadpool_t *tp);

/* work functions */
work_t *create_work(threadpool_func_t func, void *arg);
void destroy_work(work_t *work);
bool add_work(threadpool_t *tp, threadpool_func_t func, void *arg);

/* central work function */
void *worker(void *tp);

#endif /* __THREADPOOL_H__ */
