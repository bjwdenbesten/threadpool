#include "ws_threadpool.h"

_Thread_local worker_t *current_worker = NULL;



bool dequeue_push(worker_t *worker, work_t *work) {
  size_t bottom = atomic_load_explicit(&worker->bot, memory_order_relaxed);
  size_t top = atomic_load_explicit(&worker->top, memory_order_acquire);
  if (bottom - top >= DEQUEUE_CAP) return false;

  atomic_store_explicit(&worker->dequeue[bottom % DEQUEUE_CAP], work, memory_order_relaxed);

  atomic_store_explicit(&worker->bot, bottom + 1, memory_order_release);
  return true;
}

bool dequeue_pop(worker_t *worker, work_t **ret) {
  size_t bottom = atomic_load_explicit(&worker->bot, memory_order_relaxed);
  size_t top = atomic_load_explicit(&worker->top, memory_order_relaxed);

  if (bottom <= top) return false;
  bottom = bottom - 1;

  atomic_store_explicit(&worker->bot, bottom, memory_order_relaxed);
  atomic_thread_fence(memory_order_seq_cst);
  top = atomic_load_explicit(&worker->top, memory_order_acquire);


  if (top <= bottom) {
    *ret = atomic_load_explicit(&worker->dequeue[bottom % DEQUEUE_CAP], memory_order_relaxed);
    if (top == bottom) {
      if (!atomic_compare_exchange_strong_explicit(&worker->top, &top, top + 1, memory_order_seq_cst, memory_order_relaxed)) {
        atomic_store_explicit(&worker->bot, bottom + 1, memory_order_relaxed);
        return false;
      }
      atomic_store_explicit(&worker->bot, bottom + 1, memory_order_relaxed);
    }
    return true;
  }
  else {
    atomic_store_explicit(&worker->bot, bottom + 1, memory_order_relaxed);
    return false;
  }
}

bool dequeue_steal(worker_t *vic, work_t **ret) {
  size_t top = atomic_load_explicit(&vic->top, memory_order_acquire);
  size_t bot = atomic_load_explicit(&vic->bot, memory_order_acquire);

  if (top < bot) {
    *ret = atomic_load_explicit(&vic->dequeue[top % DEQUEUE_CAP], memory_order_relaxed);
    if (!atomic_compare_exchange_strong_explicit(&vic->top, &top, top + 1, memory_order_seq_cst, memory_order_relaxed)) {
      return false;
    }
    return true;
  }

  return false;
}

work_t *get_work(threadpool_t *tp) {
  if (!tp || !tp->first) return NULL;
  work_t *work = tp->first;

  tp->first = tp->first->next;
  if (tp->first == NULL) {
    tp->last = NULL;
  }
  return work;
}


/* main worker function */
void *work_func(void *arg) {
  worker_t *our_worker = (worker_t *) arg;
  threadpool_t *tp = our_worker->tp;

  current_worker = our_worker;

  if (our_worker == NULL) {
  fprintf(stderr, "work_func: our_worker == NULL! aborting\n");
  abort();
}
if (our_worker->dequeue == NULL) {
  fprintf(stderr, "work_func: our_worker->dequeue == NULL! aborting\n");
  abort();
}
if (our_worker->tp == NULL) {
  fprintf(stderr, "work_func: our_worker->tp == NULL! aborting\n");
  abort();
}

  unsigned int seed = time(NULL) ^ (unsigned long) pthread_self();

  while (true) {
    work_t *work = NULL;
    bool found_work = false;

    //first we try to search the local dequeue
    if (dequeue_pop(our_worker, &work)) {
      found_work = true;
    }
    
    //if there is nothing in the local dequque, then steal
    if (!found_work) {
      worker_t *victim = current_worker;
      for (size_t i = 0; i < tp->num_workers; i++) {
        int rand_index = rand_r(&seed) % tp->num_workers;
        victim = tp->workers[rand_index];
        if (victim == NULL) continue;
        if (victim != our_worker) {
          if (dequeue_steal(victim, &work)) {
            found_work = true;
            break;
          }
        }
      }
    }

    //lastly we check the global queue to see if work has arfived
    if (!found_work) {
      pthread_mutex_lock(&tp->mutex);


      if (tp->shutdown && tp->first == NULL) {
        pthread_mutex_unlock(&tp->mutex);
        break;
      }

      if (tp->first != NULL) {
        work = get_work(tp);
        if (work != NULL) {
          found_work = true;
        }
      }
      pthread_mutex_unlock(&tp->mutex);

      if (!found_work) {
        sched_yield();
      }
    }

    if (found_work && work != NULL) {
      work->func(work->arg);
      free(work);
      size_t prev = atomic_fetch_sub_explicit(&tp->pending, 1, memory_order_acq_rel);
      if (prev==1) {
        pthread_mutex_lock(&tp->pending_mutex);
        pthread_cond_broadcast(&tp->empty);
        pthread_mutex_unlock(&tp->pending_mutex);
      }
    }
  }
  return NULL;
}

void threadpool_await(threadpool_t *tp) {
  if (atomic_load_explicit(&tp->pending, memory_order_acquire) == 0) return;

  pthread_mutex_lock(&tp->pending_mutex);
  while (atomic_load_explicit(&tp->pending, memory_order_acquire) != 0) {
    pthread_cond_wait(&tp->empty, &tp->pending_mutex);
  }
  pthread_mutex_unlock(&tp->pending_mutex);
}


/* creates the threadpool */
threadpool_t *threadpool_create(size_t n) {
  threadpool_t *tp = malloc(sizeof(threadpool_t));
  if (tp == NULL) {
    return NULL;
  }

  tp->first = NULL;
  tp->last = NULL;
  tp->num_workers = n;
  tp->threads = calloc(n, sizeof(pthread_t));
  if (tp->threads == NULL) {
    free(tp);
    return NULL;
  }

  tp->workers = calloc(n, sizeof(worker_t*));
  if (tp->workers == NULL) {
    free(tp->threads);
    free(tp);
    return NULL;
  }

  tp->shutdown = false;

  /* initialize mutex and cond var */
  pthread_mutex_init(&(tp->mutex), NULL);
  pthread_cond_init(&(tp->has_work), NULL);

  atomic_init(&tp->pending, 0);
  pthread_cond_init(&(tp->empty), NULL);
  pthread_mutex_init(&(tp->pending_mutex), NULL);


  /* create all of the threads and workers*/
  for (size_t i = 0; i < n; i++) {
    worker_t *worker = calloc(1, sizeof(worker_t));
    if (worker == NULL) {
      free(tp->threads);
      free(tp->workers);
      free(tp);
      return NULL;
    }

    worker->dequeue = calloc(DEQUEUE_CAP, sizeof(_Atomic(work_t *)));
    worker->tp = tp;

    
    atomic_init(&worker->top, 0);
    atomic_init(&worker->bot, 0);
    tp->workers[i] = worker;

    worker->thread_id = tp->threads[i];
  }


  for (size_t i = 0; i < n; i++) {
    pthread_create(&tp->threads[i], NULL, work_func, tp->workers[i]);
  }

  return tp;
}

void threadpool_destroy(threadpool_t *tp) {
  if (tp == NULL) return;

  pthread_mutex_lock(&tp->mutex);
  tp->shutdown = true;
  pthread_cond_broadcast(&tp->has_work);
  pthread_mutex_unlock(&tp->mutex);


  for (size_t i = 0; i < tp->num_workers; i++) {
    pthread_join(tp->threads[i], NULL);
    worker_t *worker = tp->workers[i];
    size_t top = atomic_load(&worker->top);
    size_t bottom = atomic_load(&worker->bot);
    for (size_t j = top; j < bottom; j++) {
      free(worker->dequeue[j % DEQUEUE_CAP]);
    }
    free(worker->dequeue);
    free(worker);
  }


  work_t *curr_work = tp->first;
  while (curr_work != NULL) {
    work_t *next = curr_work->next;
    free(curr_work);
    curr_work = next;
  }


  free(tp->threads);
  free(tp->workers);
  pthread_cond_destroy(&tp->has_work);
  pthread_mutex_destroy(&tp->mutex);
  pthread_cond_destroy(&tp->empty);
  pthread_mutex_destroy(&tp->pending_mutex);
  free(tp);
}

void threadpool_submit(threadpool_t *tp, threadpool_func_t func, void *arg) {
  work_t *work = malloc(sizeof(work_t));
  work->func = func;
  work->arg = arg;
  work->next = NULL;

  atomic_fetch_add_explicit(&tp->pending, 1, memory_order_relaxed);

  pthread_mutex_lock(&tp->mutex);

  if (tp->first == NULL && tp->last == NULL) {
    tp->first = work;
    tp->last = work;
  }
  else {
    tp->last->next = work;
    tp->last = work;
  }

  pthread_cond_signal(&tp->has_work);

  pthread_mutex_unlock(&tp->mutex);
}

void threadpool_spawn(threadpool_func_t func, void *arg) {
  worker_t *local_worker = current_worker;
  if (!local_worker) {
    fprintf(stderr, "No Local Worker Found. TP spawn called outside thread\n");
    abort();
  }
  work_t *work = malloc(sizeof(work_t));
  work->func = func;
  work->arg = arg;
  work->next = NULL;

  atomic_fetch_add_explicit(&local_worker->tp->pending, 1, memory_order_relaxed);

  if (!dequeue_push(current_worker, work)) {
    //if we can't push to local dequeue, then push to global queue.
    threadpool_submit(local_worker->tp, func, arg);
    free(work);
  }
  sched_yield();
}


