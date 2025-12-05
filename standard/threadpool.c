#include "threadpool.h"

/* small helper function to deallocate work */
void destroy_work(work_t *work) {
  if (work==NULL) return;
  free(work);
}


work_t *create_work(threadpool_func_t func, void *arg) {
  work_t *work = calloc(1, sizeof(work_t));
  if (!work) {
    return NULL;
  }
  work->func = func;
  work->arg = arg;
  work->next = NULL;
  return work;
}

bool add_work(threadpool_t *tp, threadpool_func_t func, void *arg) {
  if (!tp || !func) return false;

  pthread_mutex_lock(&(tp->mutex));

  if (tp->shutdown) {
    pthread_mutex_unlock(&(tp->mutex));
    return false;
  }

  work_t *work = create_work(func, arg);
  if (!work) {
    pthread_mutex_unlock(&(tp->mutex));
    return false;
  }

  /* after we've acquired the lock we can add work to linked list */
  if (tp->head == NULL && tp->tail == NULL) {
    tp->head = work;
    tp->tail = work;
  }
  else {
    tp->tail->next = work;
    tp->tail = work;
  }

  /* signal that work was added to the queue */
  pthread_cond_signal(&(tp->work_cond));
  pthread_mutex_unlock(&(tp->mutex));
  return true;
}

/* pops a work link from the queue */
static work_t *get_work(threadpool_t *tp) {
  if (!tp) return NULL;
  work_t *work = tp->head;
  if (work == NULL) return NULL;

  /* make the new head the next in the linked list*/
  tp->head = tp->head->next;
  /* if we've gone past the tail, set the tail to NULL*/
  if (tp->head == NULL) {
    tp->tail = NULL;
  }
  return work;
}

/* central worker function */
void *worker(void *arg) {
  threadpool_t *o_tp = arg;
  work_t *work;
  
  /* loop that keeps thread working */
  while (1) {
    pthread_mutex_lock(&(o_tp->mutex));
    while (o_tp->head == NULL && !o_tp->shutdown) {
      /* wait for the signal that something is the queue */
      pthread_cond_wait(&(o_tp->work_cond), &(o_tp->mutex));
    }
  
    /* the thread pool is ending */
    if (o_tp->shutdown) {
      pthread_mutex_unlock(&(o_tp->mutex));
      break;
    }
    work = get_work(o_tp);
    o_tp->working++;

    pthread_mutex_unlock(&(o_tp->mutex));

    if (work != NULL) {
      work->func(work->arg);
      destroy_work(work);
    }
    pthread_mutex_lock(&(o_tp->mutex));
    o_tp->working--;
    if (o_tp->working == 0 && o_tp->head == NULL) {
      pthread_cond_signal(&(o_tp->workers_cond));
    }
    pthread_mutex_unlock(&(o_tp->mutex));
  }
  /* when we get here we are in shutdown */
  return NULL;
}

/* function to initialize the threadpool */
threadpool_t *threadpool_create(size_t num_threads) {
  threadpool_t *tp;
  tp = calloc(1, sizeof(threadpool_t));
  if (!tp) return NULL;
  tp->head = NULL;
  tp->tail = NULL;
  tp->shutdown = false;
  tp->working = 0;
  tp->num_threads = num_threads;

  pthread_mutex_init(&(tp->mutex), NULL);
  pthread_cond_init(&(tp->work_cond), NULL);
  pthread_cond_init(&(tp->workers_cond), NULL);


  tp->thread_ids = malloc(num_threads * sizeof(pthread_t));
  if (!tp->thread_ids) {
    pthread_mutex_destroy(&(tp->mutex));
    pthread_cond_destroy(&(tp->work_cond));
    pthread_cond_destroy(&(tp->workers_cond));
    free(tp);
    return NULL;
  }

  /* loop to start threads */
  for (size_t i = 0; i < num_threads; i++) {
    pthread_create(&(tp->thread_ids[i]), NULL, worker, tp);
  }

  return tp;
}


/* waits until all threads are done processing*/
void threadpool_await(threadpool_t *tp) {
  pthread_mutex_lock(&(tp->mutex));
  while (tp->working > 0 || tp->head != NULL) {
    pthread_cond_wait(&(tp->workers_cond), &(tp->mutex));
  }
  pthread_mutex_unlock(&(tp->mutex));
}


/* deallocates the global threadpool */
void threadpool_destroy(threadpool_t *tp) {

  pthread_mutex_lock(&(tp->mutex));

  /* get rid of the linked list of work if still exists*/
  work_t *curr = tp->head;
  while (curr != NULL) {
    work_t *next = curr->next;
    destroy_work(curr);
    curr = next;
  }
  tp->head = NULL;
  tp->tail = NULL;
  tp->shutdown = true;

  /* awaken all threads */
  pthread_cond_broadcast(&(tp->work_cond));
  pthread_mutex_unlock(&(tp->mutex));

  /* in case a thread is currently processing, let's wait until workers go to 0 */
  threadpool_await(tp);

  for (size_t i = 0; i < tp->num_threads; i++) {
    pthread_join(tp->thread_ids[i], NULL);
  }
  free(tp->thread_ids);

  /* destroy mutex and cond variable */
  pthread_mutex_destroy(&(tp->mutex));
  pthread_cond_destroy(&(tp->work_cond));
  pthread_cond_destroy(&(tp->workers_cond));
  free(tp);
}
