/* This file is an image processing operation for GEGL
 *
 * GEGL is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * GEGL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GEGL; if not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright 2018 Øyvind Kolås <pippin@gimp.org>
 *
 */

#include <stdio.h>
#include "config.h"
#include <glib/gi18n-lib.h>

//retire propes after given set of completed re-runs


#ifdef GEGL_PROPERTIES

property_int (seek_distance, "seek radius", 30)
  value_range (4, 512)

property_int (min_neigh, "min neigh", 4)
  value_range (1, 4)

property_int (min_iter, "min iter", 100)
  value_range (1, 512)

property_int (improvement_iters, "improvement iters", 1)
  value_range (1, 30)

property_double (chance_try, "try chance", 0.75)
  value_range (0.0, 1.0)
  ui_steps    (0.01, 0.1)
property_double (chance_retry, "retry chance", 0.8)
  value_range (0.0, 1.0)
  ui_steps    (0.01, 0.1)

property_double (ring_gap,    "ring gap", 1.25)
  value_range (0.0, 4.0)
  ui_steps    (0.1, 0.2)
property_double (ring_gamma, "ring gamma", 1.2)
  value_range (0.0, 4.0)
  ui_steps    (0.1, 0.2)
property_double (ring_twist, "ring twist", 0.040)
  value_range (0.0, 1.0)
  ui_steps    (0.01, 0.2)

property_double (metric_dist_powk, "metric dist powk", 1.35)
  value_range (0.0, 10.0)
  ui_steps    (0.1, 1.0)

property_double (metric_empty_score, "metric empty score", 0.25)
  value_range (0.0, 100.0)
  ui_steps    (0.25, 1.0)

#else

#define GEGL_OP_FILTER
#define GEGL_OP_NAME      inpaint
#define GEGL_OP_C_SOURCE  inpaint.c

#include "gegl-op.h"
#include <stdio.h>
#include "pixel-duster.h"

static GeglRectangle
get_required_for_output (GeglOperation       *operation,
                         const gchar         *input_pad,
                         const GeglRectangle *roi)
{
  GeglRectangle result = *gegl_operation_source_get_bounding_box (operation, "input");
  if (gegl_rectangle_is_infinite_plane (&result))
    return *roi;
  return result;
}

static void
prepare (GeglOperation *operation)
{
  const Babl *format = babl_format ("RGBA float");

  gegl_operation_set_format (operation, "input",  format);
  gegl_operation_set_format (operation, "output", format);
}


static gboolean
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *output,
         const GeglRectangle *result,
         gint                 level)
{
  GeglProperties *o      = GEGL_PROPERTIES (operation);
  GeglRectangle in_rect = *gegl_buffer_get_extent (input);
  GeglRectangle out_rect = *gegl_buffer_get_extent (output);
  PixelDuster    *duster = pixel_duster_new (input, input, output, &in_rect, &out_rect,
                                             o->seek_distance,
                                             1, // max_k
                                             o->min_neigh,
                                             o->min_iter,
                                             o->chance_try,
                                             o->chance_retry,
                                             1.0, // scale_x
                                             1.0, // scale_y
                                             o->improvement_iters,
                                             o->ring_gap,
                                             o->ring_gamma,
                                             o->ring_twist,
                                             o->metric_dist_powk,
                                             o->metric_empty_score,
                                             operation);
  gegl_buffer_copy (input, NULL, GEGL_ABYSS_NONE, output, NULL);
  pixel_duster_add_probes_for_transparent (duster);

  seed_db (duster);
  pixel_duster_fill (duster);
  pixel_duster_destroy (duster);

  return TRUE;
}

static GeglRectangle
get_cached_region (GeglOperation       *operation,
                   const GeglRectangle *roi)
{
  GeglRectangle result = *gegl_operation_source_get_bounding_box (operation, "input");

  if (gegl_rectangle_is_infinite_plane (&result))
    return *roi;

  return result;
}

static gboolean
operation_process (GeglOperation        *operation,
                   GeglOperationContext *context,
                   const gchar          *output_prop,
                   const GeglRectangle  *result,
                   gint                  level)
{
  GeglOperationClass  *operation_class;

  const GeglRectangle *in_rect =
    gegl_operation_source_get_bounding_box (operation, "input");

  operation_class = GEGL_OPERATION_CLASS (gegl_op_parent_class);

  if ((in_rect && gegl_rectangle_is_infinite_plane (in_rect)))
    {
      gpointer in = gegl_operation_context_get_object (context, "input");
      gegl_operation_context_take_object (context, "output",
                                          g_object_ref (G_OBJECT (in)));
      return TRUE;
    }

  return operation_class->process (operation, context, output_prop, result,
                                   gegl_operation_context_get_level (context));
}

static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass       *operation_class;
  GeglOperationFilterClass *filter_class;

  operation_class = GEGL_OPERATION_CLASS (klass);
  filter_class    = GEGL_OPERATION_FILTER_CLASS (klass);

  filter_class->process                    = process;
  operation_class->prepare                 = prepare;
  operation_class->process                 = operation_process;
  operation_class->get_required_for_output = get_required_for_output;
  operation_class->get_cached_region       = get_cached_region;
  operation_class->opencl_support          = FALSE;
  operation_class->threaded                = FALSE;

  gegl_operation_class_set_keys (operation_class,
      "name",        "gegl:alpha-inpaint",
      "title",       "Heal transparent",
      "categories",  "heal",
      "description", "Replaces fully transparent pixels with good candidate pixels found in the whole image",
      NULL);
}

#endif 
