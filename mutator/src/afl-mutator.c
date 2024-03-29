

#include "afl-fuzz.h"
#include "mutator.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DATA_SIZE (4096)
typedef struct custom_ir_mutator {
  afl_state_t *afl;
  uint8_t *mutator_buf;
} CustomIRMutator;

/**
 * Initialize this custom mutator
 *
 * @param[in] afl a pointer to the internal state object. Can be ignored for
 * now.
 * @param[in] seed A seed for this mutator - the same seed should always mutate
 * in the same way.
 * @return Pointer to the data object this custom mutator instance should use.
 *         There may be multiple instances of this mutator in one afl-fuzz run!
 *         Return NULL on error.
 */
CustomIRMutator *afl_custom_init(afl_state_t *afl, unsigned int seed) {

  CustomIRMutator *mutator =
      (CustomIRMutator *)calloc(1, sizeof(CustomIRMutator));
  if (!mutator) {
    perror("afl_custom_init alloc");
    return NULL;
  }

  mutator->afl = afl;
  // The mutator can be think of as a deterministic function where
  // new_M = Mutate(M, seed);
  srand(seed);

  if ((mutator->mutator_buf = (u8 *)malloc(MAX_FILE)) == NULL) {

    free(mutator);
    perror("mutator_buf alloc");
    return NULL;
  }
  createISelMutator();
  return mutator;
}

/**
 * Perform custom mutations on a given input
 *
 * (Optional for now. Required in the future)
 *
 * @param[in] data pointer returned in afl_custom_init for this fuzz case
 * @param[in] buf Pointer to input data to be mutated
 * @param[in] buf_size Size of input data
 * @param[out] out_buf the buffer we will work on. we can reuse *buf. NULL on
 * error.
 * @param[in] add_buf Buffer containing the additional test case
 * @param[in] add_buf_size Size of the additional test case
 * @param[in] max_size Maximum size of the mutated output. The mutation must not
 *     produce data larger than max_size.
 * @return Size of the mutated output.
 */
size_t afl_custom_fuzz(CustomIRMutator *mutator, uint8_t *buf, size_t buf_size,
                       u8 **out_buf, uint8_t *add_buf,
                       size_t add_buf_size, // add_buf can be NULL
                       size_t max_size) {

  memcpy(mutator->mutator_buf, buf, buf_size);
  size_t out_size =
      LLVMFuzzerCustomMutator(mutator->mutator_buf, buf_size, max_size, rand());

  /* return size of mutated data */
  *out_buf = mutator->mutator_buf;
  return out_size;
}

/**
 * A post-processing function to use right before AFL writes the test case to
 * disk in order to execute the target.
 *
 * (Optional) If this functionality is not needed, simply don't define this
 * function.
 *
 * @param[in] data pointer returned in afl_custom_init for this fuzz case
 * @param[in] buf Buffer containing the test case to be executed
 * @param[in] buf_size Size of the test case
 * @param[out] out_buf Pointer to the buffer containing the test case after
 *     processing. External library should allocate memory for out_buf.
 *     The buf pointer may be reused (up to the given buf_size);
 * @return Size of the output buffer after processing or the needed amount.
 *     A return of 0 indicates an error.
 */
/*
size_t afl_custom_post_process(CustomIRMutator *data, uint8_t *buf,
                               size_t buf_size, uint8_t **out_buf) {

  uint8_t *post_process_buf =
      maybe_grow(BUF_PARAMS(data, post_process), buf_size + 5);
  if (!post_process_buf) {

    perror("custom mutator realloc failed.");
    *out_buf = NULL;
    return 0;
  }

  memcpy(post_process_buf + 5, buf, buf_size);
  post_process_buf[0] = 'A';
  post_process_buf[1] = 'F';
  post_process_buf[2] = 'L';
  post_process_buf[3] = '+';
  post_process_buf[4] = '+';

  *out_buf = post_process_buf;

  return buf_size + 5;
}
*/

/**
 * This method is called at the start of each trimming operation and receives
 * the initial buffer. It should return the amount of iteration steps possible
 * on this input (e.g. if your input has n elements and you want to remove
 * them one by one, return n, if you do a binary search, return log(n),
 * and so on...).
 *
 * If your trimming algorithm doesn't allow you to determine the amount of
 * (remaining) steps easily (esp. while running), then you can alternatively
 * return 1 here and always return 0 in post_trim until you are finished and
 * no steps remain. In that case, returning 1 in post_trim will end the
 * trimming routine. The whole current index/max iterations stuff is only used
 * to show progress.
 *
 * (Optional)
 *
 * @param data pointer returned in afl_custom_init for this fuzz case
 * @param buf Buffer containing the test case
 * @param buf_size Size of the test case
 * @return The amount of possible iteration steps to trim the input.
 *        negative on error.
 */
/*
int32_t afl_custom_init_trim(CustomIRMutator *data, uint8_t *buf,
                             size_t buf_size) {

  // We simply trim once
  data->trimmming_steps = 1;

  data->cur_step = 0;

  if (!maybe_grow(BUF_PARAMS(data, trim), buf_size)) {

    perror("init_trim grow");
    return -1;
  }

  memcpy(data->trim_buf, buf, buf_size);

  data->trim_size_current = buf_size;

  return data->trimmming_steps;
}
*/

/**
 * This method is called for each trimming operation. It doesn't have any
 * arguments because we already have the initial buffer from init_trim and we
 * can memorize the current state in *data. This can also save
 * reparsing steps for each iteration. It should return the trimmed input
 * buffer, where the returned data must not exceed the initial input data in
 * length. Returning anything that is larger than the original data (passed
 * to init_trim) will result in a fatal abort of AFLFuzz.
 *
 * (Optional)
 *
 * @param[in] data pointer returned in afl_custom_init for this fuzz case
 * @param[out] out_buf Pointer to the buffer containing the trimmed test case.
 *     External library should allocate memory for out_buf.
 *     AFL++ will not release the memory after saving the test case.
 *     Keep a ref in *data.
 *     *out_buf = NULL is treated as error.
 * @return Pointer to the size of the trimmed test case
 */
/*
size_t afl_custom_trim(CustomIRMutator *data, uint8_t **out_buf) {

  *out_buf = data->trim_buf;

  // Remove the last byte of the trimming input
  return data->trim_size_current - 1;
}
*/

/**
 * This method is called after each trim operation to inform you if your
 * trimming step was successful or not (in terms of coverage). If you receive
 * a failure here, you should reset your input to the last known good state.
 *
 * (Optional)
 *
 * @param[in] data pointer returned in afl_custom_init for this fuzz case
 * @param success Indicates if the last trim operation was successful.
 * @return The next trim iteration index (from 0 to the maximum amount of
 *     steps returned in init_trim). negative ret on failure.
 */
/*
int32_t afl_custom_post_trim(CustomIRMutator *data, int success) {

  if (success) {

    ++data->cur_step;
    return data->cur_step;
  }

  return data->trimmming_steps;
}
*/

/**
 * Perform a single custom mutation on a given input.
 * This mutation is stacked with the other muatations in havoc.
 *
 * (Optional)
 *
 * @param[in] data pointer returned in afl_custom_init for this fuzz case
 * @param[in] buf Pointer to the input data to be mutated and the mutated
 *     output
 * @param[in] buf_size Size of input data
 * @param[out] out_buf The output buffer. buf can be reused, if the content
 * fits. *out_buf = NULL is treated as error.
 * @param[in] max_size Maximum size of the mutated output. The mutation must
 *     not produce data larger than max_size.
 * @return Size of the mutated output.
 */
/*
size_t afl_custom_havoc_mutation(CustomIRMutator *mutator, u8 *buf,
                                 size_t buf_size, u8 **out_buf,
                                 size_t max_size) {
  memcpy(mutator->mutator_buf, buf, buf_size);
  size_t out_size = LLVMFuzzerCustomMutator(mutator->mutator_buf, buf_size,
                                            max_size, mutator->seed);

  // return size of mutated data
  *out_buf = mutator->mutator_buf;
  return out_size;
}
*/

/**
 * Return the probability (in percentage) that afl_custom_havoc_mutation
 * is called in havoc. By default it is 6 %.
 *
 * (Optional)
 *
 * @param[in] data pointer returned in afl_custom_init for this fuzz case
 * @return The probability (0-100).
 */
/*
uint8_t afl_custom_havoc_mutation_probability(CustomIRMutator *data) {
  return 100; // 100 %
}
*/

/**
 * Determine whether the fuzzer should fuzz the queue entry or not.
 *
 * (Optional)
 *
 * @param[in] data pointer returned in afl_custom_init for this fuzz case
 * @param filename File name of the test case in the queue entry
 * @return Return True(1) if the fuzzer will fuzz the queue entry, and
 *     False(0) otherwise.
 */
/*
uint8_t afl_custom_queue_get(CustomIRMutator *data, const uint8_t *filename) {

  return 1;
}
*/

/**
 * Allow for additional analysis (e.g. calling a different tool that does a
 * different kind of coverage and saves this for the custom mutator).
 *
 * (Optional)
 *
 * @param data pointer returned in afl_custom_init for this fuzz case
 * @param filename_new_queue File name of the new queue entry
 * @param filename_orig_queue File name of the original queue entry
 * @return if the file contents was modified return 1 (True), 0 (False)
 *         otherwise
 */
/*
uint8_t afl_custom_queue_new_entry(CustomIRMutator *data,
                                   const uint8_t *filename_new_queue,
                                   const uint8_t *filename_orig_queue) {

  // Additional analysis on the original or new test case
  return 0;
}
*/

/**
 * Deinitialize everything
 *
 * @param data The data ptr from afl_custom_init
 */
void afl_custom_deinit(CustomIRMutator *mutator) {
  free(mutator->mutator_buf);
  free(mutator);
}
