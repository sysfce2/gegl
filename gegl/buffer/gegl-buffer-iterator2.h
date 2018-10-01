/* This file is part of GEGL.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright 2008 Øyvind Kolås <pippin@gimp.org>
 *           2013 Daniel Sabo
 */

#ifndef __GEGL_BUFFER_ITERATOR2_H__
#define __GEGL_BUFFER_ITERATOR2_H__

#include "gegl-buffer.h"

typedef struct _GeglBufferIterator2Priv GeglBufferIterator2Priv;

/***
 * GeglBufferIterator2:
 *
 * GeglBufferIterator allows to iterate over one or more GeglBuffers.
 * In each iteration the new data is available as a linear chunk of
 * memory. See gegl_buffer_iterator_new() and gegl_buffer_iterator_next()
 */

typedef struct GeglBufferIterator2Item
{
  gpointer      data;
  GeglRectangle roi;
} GeglBufferIterator2Item;

typedef struct GeglBufferIterator2
{
  gint           length;
  GeglBufferIterator2Priv *priv;
  GeglBufferIterator2Item  items[];
} GeglBufferIterator2;


/**
 * gegl_buffer_iterator2_empty_new: (skip)
 * Create a new buffer iterator without adding any buffers.
 *
 * Returns: a new buffer iterator.
 */
GeglBufferIterator2 *gegl_buffer_iterator2_empty_new (int max_slots);

/**
 * gegl_buffer_iterator2_new: (skip)
 * @buffer: a #GeglBuffer
 * @roi: the rectangle to iterate over
 * @level: the level at which we are iterating, the roi will indicate the
 * extent at 1:1, x,y,width and height are/(2^level)
 * @format: the format we want to process this buffers data in, pass 0 to use the buffers format.
 * @access_mode: whether we need reading or writing to this buffer one of GEGL_BUFFER_READ, GEGL_BUFFER_WRITE and GEGL_BUFFER_READWRITE.
 * @abyss_policy: how request outside the buffer extent are handled.
 *
 * Create a new buffer iterator, this buffer will be iterated through
 * in linear chunks, some chunks might be full tiles the coordinates, see
 * the documentation of gegl_buffer_iterator_next for how to use it and
 * destroy it.
 *
 * Returns: a new buffer iterator that can be used to iterate through the
 * buffers pixels.
 */
GeglBufferIterator2 * gegl_buffer_iterator2_new  (
                                                 GeglBuffer          *buffer,
                                                 const GeglRectangle *roi,
                                                 gint                 level,
                                                 const Babl          *format,
                                                 GeglAccessMode       access_mode,
                                                 GeglAbyssPolicy      abyss_policy,
                                                 gint                 max_slots);


/**
 * gegl_buffer_iterator2_add: (skip)
 * @iterator: a #GeglBufferIterator
 * @buffer: a #GeglBuffer
 * @roi: the rectangle to iterate over
 * @level: the level at which we are iterating, the roi will indicate the
 * extent at 1:1, x,y,width and height are/(2^level)
 * @format: the format we want to process this buffers data in, pass 0 to use the buffers format.
 * @access_mode: whether we need reading or writing to this buffer.
 * @abyss_policy: how request outside the buffer extent are handled.
 *
 * Adds an additional buffer iterator that will be processed in sync with
 * the original one, if the buffer doesn't align with the other for tile access
 * the corresponding scans and regions will be serialized automatically using
 * gegl_buffer_get.
 *
 * If the buffer shares its tiles with a previously-added buffer (in
 * particular, if the same buffer is added more than once), and at least one of
 * the buffers is accessed for writing, the corresponding iterated-over areas
 * should either completely overlap, or not overlap at all, in the coordinate-
 * system of the underlying tile storage (that is, after shifting each area by
 * the corresponding buffer's shift-x and shift-y properties).  If the areas
 * overlap, at most one of the buffers may be accessed for writing, and the
 * data pointers of the corresponding iterator items may refer to the same
 * data.
 *
 * Returns: an integer handle refering to the indice in the iterator structure
 * of the added buffer.
 */
gint                 gegl_buffer_iterator2_add  (GeglBufferIterator2  *iterator,
                                                GeglBuffer          *buffer,
                                                const GeglRectangle *roi,
                                                gint                 level,
                                                const Babl          *format,
                                                GeglAccessMode       access_mode,
                                                GeglAbyssPolicy      abyss_policy);

/**
 * gegl_buffer_iterator2_stop: (skip)
 * @iterator: a GeglBufferIterator2
 *
 * Cancels the current iteration, freeing up any temporary resources. The
 * iterator handle is no longer valid after invoking this function.
 */
void                 gegl_buffer_iterator2_stop  (GeglBufferIterator2 *iterator);

/**
 * gegl_buffer_iterator2_next: (skip)
 * @iterator: a #GeglBufferIterator
 *
 * Do an iteration, this causes a new set of iterator->data[] to become
 * available if there is more data to process. Changed data from a previous
 * iteration step will also be saved now. When there is no more data to
 * be processed FALSE will be returned (and the iterator handle is no longer
 * valid).
 *
 * Returns: TRUE if there is more work FALSE if iteration is complete.
 */
gboolean             gegl_buffer_iterator2_next (GeglBufferIterator2 *iterator);



#endif
