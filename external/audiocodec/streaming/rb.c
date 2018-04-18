/******************************************************************
 *
 * Copyright 2018 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************/

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "internal_defs.h"
#include "debug.h"
#include <audiocodec/streaming/rb.h>

#define IS_EMPTY(rbp) (rbp->rd_idx == rbp->wr_idx)
#define IS_FULL(rbp) ((rbp->rd_idx & IDX_MASK) == (rbp->wr_idx & IDX_MASK) && (rbp->rd_idx & MSB_MASK) != (rbp->wr_idx & MSB_MASK))

/**
 * @brief  Increase the buffer index while writing or reading the ring-buffer.
 *         This is implemented according to the 'mirroring' solution:
 *         http://en.wikipedia.org/wiki/Circular_buffer#Mirroring
 *
 * @param  rbp: Pointer to the ring-buffer
 * @param  p_idx: Pointer to the buffer index to be increased
 * @param  len: length of the size the index be increased
 */
static void _incr(rb_p rbp, volatile size_t *p_idx, size_t len);


bool rb_init(rb_p rbp, size_t size)
{
    RETURN_VAL_IF_FAIL(rbp != NULL, false);
    RETURN_VAL_IF_FAIL(size < MSB_MASK, false);

    rbp->buf    = calloc(size, 1);
    rbp->depth  = size;
    rbp->rd_idx = 0;
    rbp->wr_idx = 0;

    return (rbp->buf != NULL);
}

void rb_free(rb_p rbp)
{
    RETURN_IF_FAIL(rbp != NULL);

    RETURN_IF_FAIL(rbp->buf != NULL);
    free(rbp->buf);
    rbp->buf = NULL;
}

#if 0
void rb_clear(rb_p rbp)
{
    RETURN_IF_FAIL(rbp != NULL);
    rbp->rd_idx = 0;
    rbp->wr_idx = 0;
}
#endif

size_t rb_used(rb_p rbp)
{
    RETURN_VAL_IF_FAIL(rbp != NULL, 0);

    if (IS_EMPTY(rbp))
        return 0;

    size_t wr_idx = (rbp->wr_idx & IDX_MASK);
    size_t rd_idx = (rbp->rd_idx & IDX_MASK);

	if (wr_idx > rd_idx)
		return (wr_idx - rd_idx);

	return (rbp->depth - (rd_idx - wr_idx));
}

size_t rb_avail(rb_p rbp)
{
    RETURN_VAL_IF_FAIL(rbp != NULL, 0);
	return (rbp->depth - rb_used(rbp));
}

size_t rb_write(rb_p rbp, const void *ptr, size_t len)
{
	RETURN_VAL_IF_FAIL(rbp != NULL, 0);
	RETURN_VAL_IF_FAIL(ptr != NULL, 0);

	size_t avail = rb_avail(rbp);
	if (len > avail)
		len = avail;

    size_t wr_idx = (rbp->wr_idx & IDX_MASK);
	size_t len_contiguous = rbp->depth - wr_idx;

	if (len > len_contiguous)
	{
		memcpy((void*)((uint8_t*)rbp->buf + wr_idx), ptr, len_contiguous);
		memcpy(rbp->buf, (const void*)((const uint8_t*)ptr + len_contiguous), (len - len_contiguous));
	}
	else
		memcpy((void*)((uint8_t*)rbp->buf + wr_idx), ptr, len);

    _incr(rbp, &rbp->wr_idx, len);
    return len;
}

size_t rb_read(rb_p rbp, void *ptr, size_t len)
{
	RETURN_VAL_IF_FAIL(rbp != NULL, 0);

	len = rb_read_ext(rbp, ptr, len, 0);
	_incr(rbp, &rbp->rd_idx, len);
    return len;
}

size_t rb_read_ext(rb_p rbp, void *ptr, size_t len, size_t offset)
{
	RETURN_VAL_IF_FAIL(rbp != NULL, 0);

	size_t used = rb_used(rbp);
	RETURN_VAL_IF_FAIL(offset < used, 0);

	if (len > used - offset)
		len = used - offset;
	RETURN_VAL_IF_FAIL(len != 0, 0);

	if (ptr != NULL)
	{
		// increase temp rd_idx
		size_t rd_idx = rbp->rd_idx;
		_incr(rbp, &rd_idx, offset);

		// start to copy data
	    rd_idx = (rd_idx & IDX_MASK);
		size_t len_contiguous = rbp->depth - rd_idx;

		if (len > len_contiguous)
		{
			memcpy(ptr, (void*)((uint8_t*)rbp->buf + rd_idx), len_contiguous);
			memcpy((void*)((uint8_t*)ptr + len_contiguous), rbp->buf, (len - len_contiguous));
		}
		else
			memcpy(ptr, (void*)((uint8_t*)rbp->buf + rd_idx), len);
	}

    return len;
}

static void _incr(rb_p rbp, volatile size_t *p_idx, size_t len)
{
    size_t idx = *p_idx & IDX_MASK;
    size_t msb = *p_idx & MSB_MASK;

	idx += len;
	if (idx >= rbp->depth)
	{
        msb ^= MSB_MASK;
        idx -= rbp->depth;
	}

    *p_idx = msb | idx;
}


