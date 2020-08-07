/*
   american fuzzy lop++ - queue relates routines
   ---------------------------------------------

   Originally written by Michal Zalewski

   Now maintained by Marc Heuse <mh@mh-sec.de>,
                        Heiko Eißfeldt <heiko.eissfeldt@hexco.de> and
                        Andrea Fioraldi <andreafioraldi@gmail.com>

   Copyright 2016, 2017 Google Inc. All rights reserved.
   Copyright 2019-2020 AFLplusplus Project. All rights reserved.
   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

   This is the actual code for the library framework.

 */

#include "libaflpp.h"
#include "list.h"
#include "stdbool.h"
#include "afl-returns.h"

void _afl_executor_init_(executor_t *executor) {

  executor->current_input = NULL;

  // Default implementations of the functions
  executor->funcs.add_observation_channel = add_observation_channel_default;
  executor->funcs.get_observation_channels = get_observation_channels_default;
  executor->funcs.get_current_input = get_current_input_default;
  executor->funcs.reset_observation_channels =
      reset_observation_channel_default;

}

// Default implementations for executor vtable
void afl_executor_deinit(executor_t *executor) {

  // if (!executor) ;

  free(executor);

}

u8 add_observation_channel_default(executor_t *           executor,
                                   observation_channel_t *obs_channel) {

  executor->observors[executor->observors_num] = obs_channel;

  executor->observors_num++;

  return 0;

}

observation_channel_t *get_observation_channels_default(executor_t *executor,
                                                        size_t      idx) {

  if (executor->observors_num <= idx) { return NULL; }

  return executor->observors[idx];

}

raw_input_t *get_current_input_default(executor_t *executor) {

  return executor->current_input;

}

void reset_observation_channel_default(executor_t *executor) {

  for (size_t i = 0; i < executor->observors_num; ++i) {

    observation_channel_t *obs_channel = executor->observors[i];
    if (obs_channel->funcs.post_exec) {

      obs_channel->funcs.post_exec(executor->observors[i]);

    }

  }

}

