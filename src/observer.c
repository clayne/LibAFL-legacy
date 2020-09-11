/*
   american fuzzy lop++ - fuzzer header
   ------------------------------------

   Originally written by Michal Zalewski

   Now maintained by Marc Heuse <mh@mh-sec.de>,
                     Heiko Eißfeldt <heiko.eissfeldt@hexco.de>,
                     Andrea Fioraldi <andreafioraldi@gmail.com>,
                     Dominik Maier <mail@dmnk.co>

   Copyright 2016, 2017 Google Inc. All rights reserved.
   Copyright 2019-2020 AFLplusplus Project. All rights reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

   This is the Library based on AFL++ which can be used to build
   customized fuzzers for a specific target while taking advantage of
   a lot of features that AFL++ already provides.

 */

#include "observer.h"
#include "afl-returns.h"

afl_ret_t afl_observer_init(afl_observer_t *channel, size_t channel_id) {

  (void)channel;

  channel->channel_id = channel_id;
  return AFL_RET_SUCCESS;

}

void afl_observer_deinit(afl_observer_t *channel) {

  (void)channel;

}

void afl_flush(afl_observer_t *channel) {

  (void)channel;

  /* TODO: Implementation */
  return;

}

void afl_reset(afl_observer_t *channel) {

  (void)channel;

  /* TODO: Implementation */
  return;

}

void afl_post_exec(afl_observer_t *channel) {

  (void)channel;

  /* TODO: Implementation */
  return;

}

afl_ret_t afl_map_channel_init(afl_map_based_channel_t *map_channel, size_t map_size, size_t channel_id) {

  afl_observer_init(&(map_channel->base), channel_id);

  if (!afl_shmem_init(&map_channel->shared_map, map_size)) { return AFL_RET_ERROR_INITIALIZE; }

  map_channel->base.funcs.reset = afl_map_channel_reset;

  map_channel->funcs.get_map_size = afl_get_map_size;
  map_channel->funcs.get_trace_bits = afl_get_trace_bits;

  return AFL_RET_SUCCESS;

}

void afl_map_channel_deinit(afl_map_based_channel_t *map_channel) {

  afl_shmem_deinit(&map_channel->shared_map);

  afl_observer_deinit(&map_channel->base);

}

void afl_map_channel_reset(afl_observer_t *channel) {

  afl_map_based_channel_t *map_channel = (afl_map_based_channel_t *)channel;

  memset(map_channel->shared_map.map, 0, map_channel->shared_map.map_size);

}

u8 *afl_get_trace_bits(afl_map_based_channel_t *obs_channel) {

  return obs_channel->shared_map.map;

}

size_t afl_get_map_size(afl_map_based_channel_t *obs_channel) {

  return obs_channel->shared_map.map_size;

}

