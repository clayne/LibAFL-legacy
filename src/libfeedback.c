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

#include "libfeedback.h"

feedback_t *afl_feedback_init() {

  feedback_t *feedback = ck_alloc(sizeof(feedback_t));
  feedback->operations = ck_alloc(sizeof(struct feedback_operations));

  feedback->operations->set_feedback_queue = _set_feedback_queue_;
  feedback->operations->get_feedback_queue = _get_feedback_queue_;

  return feedback;

}

void afl_feedback_deinit(feedback_t *feedback) {

  ck_free(feedback->operations);
  if (feedback->metadata) ck_free(feedback->metadata);

  ck_free(feedback);

}

void _set_feedback_queue_(feedback_t *feedback, feedback_queue_t *queue) {

  feedback->queue = queue;

}

feedback_queue_t *_get_feedback_queue_(feedback_t *feedback) {

  return feedback->queue;

}
