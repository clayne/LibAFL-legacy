/* An in mmeory fuzzing example. Fuzzer for libpng library */

#include <stdio.h>
#include "aflpp.h"

extern u8 *__afl_area_ptr;

static llmp_broker_state_t *llmp_broker;
static int                  broker_port;
static int                  debug;

/* A global array of all the registered engines */
static pthread_mutex_t fuzz_worker_array_lock;
static engine_t *      registered_fuzz_workers[MAX_WORKERS];
static u64             fuzz_workers_count;

int                       LLVMFuzzerTestOneInput(const uint8_t *, size_t);
__attribute__((weak)) int LLVMFuzzerInitialize(int *argc, char ***argv);
// TODO: we still have to put LLVMFuzzerInitialize here

int debug_LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

  u32 i;
  fprintf(stderr, "Enter harness function %p %lu\n", data, size);
  for (i = 0; i < 65536; i++)
    if (__afl_area_ptr[i])
      fprintf(stderr, "Error: map unclean before harness: map[%04x]=0x%02x\n",
              i, __afl_area_ptr[i]);

  int ret = LLVMFuzzerTestOneInput(data, size);

  fprintf(stderr, "MAP:");
  for (i = 0; i < 65536; i++)
    if (__afl_area_ptr[i])
      fprintf(stderr, " map[%04x]=0x%02x", i, __afl_area_ptr[i]);
  fprintf(stderr, "\n");

  return ret;

}

static afl_ret_t in_memory_fuzzer_start(executor_t *executor) {

  in_memory_executor_t *in_memory_fuzzer = (in_memory_executor_t *)executor;

  if (LLVMFuzzerInitialize) {

    LLVMFuzzerInitialize(&in_memory_fuzzer->argc, &in_memory_fuzzer->argv);

  }

  return AFL_RET_SUCCESS;

}

/* Function to register/add a fuzz worker (engine). To avoid race condition, add
 * mutex here(Won't be performance problem). */
static inline afl_ret_t afl_register_fuzz_worker(engine_t *engine) {

  // Critical section. Needs a lock. Called very rarely, thus won't affect perf.
  pthread_mutex_lock(&fuzz_worker_array_lock);

  if (fuzz_workers_count >= MAX_WORKERS) {

    pthread_mutex_unlock(&fuzz_worker_array_lock);
    return AFL_RET_ARRAY_END;

  }

  registered_fuzz_workers[fuzz_workers_count] = engine;
  fuzz_workers_count++;
  // Unlock the mutex
  pthread_mutex_unlock(&fuzz_worker_array_lock);
  return AFL_RET_SUCCESS;

}

engine_t *initialize_fuzz_instance(int argc, char **argv, char *in_dir,
                                   char *queue_dirpath) {

  /* Let's create an in-memory executor */
  in_memory_executor_t *in_memory_executor =
      calloc(1, sizeof(in_memory_executor_t));
  if (!in_memory_executor) { FATAL("%s", afl_ret_stringify(AFL_RET_ALLOC)); }

  if (debug)
    in_memory_executor_init(
        in_memory_executor,
        (harness_function_type)debug_LLVMFuzzerTestOneInput);
  else
    in_memory_executor_init(in_memory_executor,
                            (harness_function_type)LLVMFuzzerTestOneInput);

  in_memory_executor->argc = argc;
  in_memory_executor->argv = afl_argv_cpy_dup(argc, argv);
  in_memory_executor->base.funcs.init_cb = in_memory_fuzzer_start;

  /* Observation channel, map based, we initialize this ourselves since we don't
   * actually create a shared map */
  map_based_channel_t *trace_bits_channel =
      calloc(1, sizeof(map_based_channel_t));
  afl_observation_channel_init(&trace_bits_channel->base, MAP_CHANNEL_ID);
  if (!trace_bits_channel) {

    FATAL("Trace bits channel error %s", afl_ret_stringify(AFL_RET_ALLOC));

  }

  /* Since we don't use map_channel_create function, we have to add reset
   * function manually */
  trace_bits_channel->base.funcs.reset = afl_map_channel_reset;

  trace_bits_channel->shared_map.map = __afl_area_ptr;  // Coverage map
  trace_bits_channel->shared_map.map_size = MAP_SIZE;
  trace_bits_channel->shared_map.shm_id =
      -1;  // Just a simple erronous value :)
  in_memory_executor->base.funcs.add_observation_channel(
      &in_memory_executor->base, &trace_bits_channel->base);

  /* We create a simple feedback queue for coverage here*/
  feedback_queue_t *coverage_feedback_queue =
      afl_feedback_queue_create(NULL, (char *)"Coverage feedback queue");
  if (!coverage_feedback_queue) { FATAL("Error initializing feedback queue"); }
  coverage_feedback_queue->base.funcs.set_dirpath(
      &coverage_feedback_queue->base, queue_dirpath);

  /* Global queue creation */
  global_queue_t *global_queue = afl_global_queue_create();
  if (!global_queue) { FATAL("Error initializing global queue"); }
  global_queue->extra_funcs.add_feedback_queue(global_queue,
                                               coverage_feedback_queue);
  global_queue->base.funcs.set_dirpath(&global_queue->base, queue_dirpath);

  /* Coverage Feedback initialization */
  maximize_map_feedback_t *coverage_feedback = map_feedback_init(
      coverage_feedback_queue, trace_bits_channel->shared_map.map_size,
      MAP_CHANNEL_ID);
  if (!coverage_feedback) { FATAL("Error initializing feedback"); }

  /* Let's build an engine now */
  engine_t *engine =
      afl_engine_create(&in_memory_executor->base, NULL, global_queue);
  if (!engine) { FATAL("Error initializing Engine"); }
  engine->funcs.add_feedback(engine, (feedback_t *)coverage_feedback);
  engine->funcs.set_global_queue(engine, global_queue);
  engine->in_dir = in_dir;

  fuzz_one_t *fuzz_one = afl_fuzz_one_create(engine);
  if (!fuzz_one) { FATAL("Error initializing fuzz_one"); }

  // We also add the fuzzone to the engine here.
  engine->funcs.set_fuzz_one(engine, fuzz_one);

  scheduled_mutator_t *mutators_havoc = afl_scheduled_mutator_create(NULL, 8);
  if (!mutators_havoc) { FATAL("Error initializing Mutators"); }

  mutators_havoc->extra_funcs.add_mutator(mutators_havoc, flip_byte_mutation);
  mutators_havoc->extra_funcs.add_mutator(mutators_havoc,
                                          flip_2_bytes_mutation);
  mutators_havoc->extra_funcs.add_mutator(mutators_havoc,
                                          flip_4_bytes_mutation);
  mutators_havoc->extra_funcs.add_mutator(mutators_havoc,
                                          delete_bytes_mutation);
  mutators_havoc->extra_funcs.add_mutator(mutators_havoc, clone_bytes_mutation);
  mutators_havoc->extra_funcs.add_mutator(mutators_havoc, flip_bit_mutation);
  mutators_havoc->extra_funcs.add_mutator(mutators_havoc, flip_2_bits_mutation);
  mutators_havoc->extra_funcs.add_mutator(mutators_havoc, flip_4_bits_mutation);
  mutators_havoc->extra_funcs.add_mutator(mutators_havoc,
                                          random_byte_add_sub_mutation);
  mutators_havoc->extra_funcs.add_mutator(mutators_havoc, random_byte_mutation);

  fuzzing_stage_t *stage = afl_fuzzing_stage_create(engine);
  if (!stage) { FATAL("Error creating fuzzing stage"); }
  stage->funcs.add_mutator_to_stage(stage, &mutators_havoc->base);

  return engine;

}

void thread_run_instance(llmp_client_state_t *llmp_client, void *data) {

  engine_t *engine = (engine_t *)data;
  engine->llmp_client = llmp_client;

  map_based_channel_t *trace_bits_channel =
      (map_based_channel_t *)engine->executor->observors[0];

  fuzzing_stage_t *    stage = (fuzzing_stage_t *)engine->fuzz_one->stages[0];
  scheduled_mutator_t *mutators_havoc =
      (scheduled_mutator_t *)stage->mutators[0];

  maximize_map_feedback_t *coverage_feedback =
      (maximize_map_feedback_t *)(engine->feedbacks[0]);

  /* Now we can simply load the testcases from the directory given */
  afl_ret_t ret =
      engine->funcs.load_testcases_from_dir(engine, engine->in_dir, NULL);
  if (ret != AFL_RET_SUCCESS) {

    PFATAL("Error loading testcase dir: %s", afl_ret_stringify(ret));

  }

  afl_ret_t fuzz_ret = engine->funcs.loop(engine);

  if (fuzz_ret != AFL_RET_SUCCESS) {

    PFATAL("Error fuzzing the target: %s", afl_ret_stringify(fuzz_ret));

  }

  SAYF(
      "Fuzzing ends with all the queue entries fuzzed. No of executions %llu\n",
      engine->executions);

  /* Let's free everything now. Note that if you've extended any structure,
   * which now contains pointers to any dynamically allocated region, you have
   * to free them yourselves, but the extended structure itself can be de
   * initialized using the deleted functions provided */

  afl_executor_delete(engine->executor);
  afl_map_channel_delete(trace_bits_channel);
  afl_scheduled_mutator_delete(mutators_havoc);
  afl_fuzz_stage_delete(stage);
  afl_fuzz_one_delete(engine->fuzz_one);
  free(coverage_feedback->virgin_bits);
  for (size_t i = 0; i < engine->feedbacks_num; ++i) {

    afl_feedback_delete((feedback_t *)engine->feedbacks[i]);

  }

  for (size_t i = 0; i < engine->global_queue->feedback_queues_num; ++i) {

    afl_feedback_queue_delete(engine->global_queue->feedback_queues[i]);

  }

  afl_global_queue_delete(engine->global_queue);
  afl_engine_delete(engine);

}

void *run_broker_thread(void *data) {

  (void)data;
  llmp_broker_run(llmp_broker);
  return 0;

}

int main(int argc, char **argv) {

  if (argc < 4) {

    FATAL(
        "Usage: %s number_of_threads /path/to/input/dir "
        "/path/to/queue/dir",
        argv[0]);

  }

  if (getenv("DEBUG") || getenv("AFL_DEBUG") || getenv("LIBAFL_DEBUG")) {

    debug = 1;
    fprintf(stderr, "Map ptr: %p\n", __afl_area_ptr);

  }

  char *in_dir = argv[2];
  int   thread_count = atoi(argv[1]);
  char *queue_dirpath = argv[3];

  if (thread_count <= 0) {

    FATAL("Number of threads should be greater than 0");

  }

  broker_port = 0xAF1;
  llmp_broker = llmp_broker_new();
  if (!llmp_broker) { FATAL("Broker creation failed"); }
  if (!llmp_broker_register_local_server(llmp_broker, broker_port)) {

    FATAL("Broker register failed");

  }

  OKF("Broker created now");

  for (int i = 0; i < thread_count; ++i) {

    engine_t *engine =
        initialize_fuzz_instance(argc, argv, in_dir, queue_dirpath);

    if (!llmp_broker_register_threaded_clientloop(
            llmp_broker, thread_run_instance, engine)) {

      FATAL("Error registering client");

    };

    if (afl_register_fuzz_worker(engine) != AFL_RET_SUCCESS) {

      FATAL("Error registering fuzzing instance");

    }

  }

  // Before we start the broker, we close the stderr file. Since the in-mem
  // fuzzer runs in the same process, this is necessary for stats collection.

  if (!debug) {

    s32 dev_null_fd = open("/dev/null", O_WRONLY);

    dup2(dev_null_fd, 2);

  }

  pthread_t p1;

  int s = pthread_create(&p1, NULL, run_broker_thread, NULL);

  if (!s) { OKF("Broker started running"); }

  u64 time_elapsed = 1;

  while (1) {

    sleep(1);
    u64 execs = 0;
    u64 crashes = 0;
    for (size_t i = 0; i < fuzz_workers_count; ++i) {

      execs += registered_fuzz_workers[i]->executions;
      crashes += registered_fuzz_workers[i]->crashes;

    }

    u64 paths = registered_fuzz_workers[0]->global_queue->feedback_queues_num;

    SAYF("execs=%llu  execs/s=%llu  paths=%llu  crashes=%llu  elapsed=%llu\r",
         execs, execs / time_elapsed, paths, crashes, time_elapsed);
    time_elapsed++;
    fflush(0);

  }

  return 0;

}
