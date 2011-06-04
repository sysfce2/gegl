
/* !!!! AUTOGENERATED FILE generated by math.rb !!!!!
 *
 * This file is an image processing operation for GEGL
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
 * License along with GEGL; if not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright 2006 Øyvind Kolås <pippin@gimp.org>
 *
 * !!!! AUTOGENERATED FILE !!!!!
 */
#include "config.h"
#include <glib/gi18n-lib.h>


#ifdef GEGL_CHANT_PROPERTIES

gegl_chant_double (value, _("Value"), -G_MAXDOUBLE, G_MAXDOUBLE, 1.0, _("global value used if aux doesn't contain data"))

#else

#define GEGL_CHANT_TYPE_POINT_COMPOSER
#define GEGL_CHANT_C_FILE       "divide.c"

#include "gegl-chant.h"

#include <math.h>
#ifdef _MSC_VER
#define powf(a,b) ((gfloat)pow(a,b))
#endif


static void prepare (GeglOperation *operation)
{
  const Babl *format = babl_format ("RGBA float");

  gegl_operation_set_format (operation, "input", format);
  gegl_operation_set_format (operation, "aux", babl_format ("RGB float"));
  gegl_operation_set_format (operation, "output", format);
}

static gboolean
process (GeglOperation        *op,
          void                *in_buf,
          void                *aux_buf,
          void                *out_buf,
          glong                n_pixels,
          const GeglRectangle *roi,
          gint                 level)
{
  gfloat * GEGL_ALIGNED in = in_buf;
  gfloat * GEGL_ALIGNED out = out_buf;
  gfloat * GEGL_ALIGNED aux = aux_buf;
  gint    i;

  if (aux == NULL)
    {
      gfloat value = GEGL_CHANT_PROPERTIES (op)->value;
      for (i=0; i<n_pixels; i++)
        {
          gint   j;
          gfloat c;
          for (j=0; j<3; j++)
            {
              c=in[j];
              c = value==0.0f?0.0f:c/value;
              out[j]=c;
            }
          out[3]=in[3];
          in += 4;
          out+= 4;
        }
    }
  else
    {
      for (i=0; i<n_pixels; i++)
        {
          gint   j;
          gfloat c;
          gfloat value;
          for (j=0; j<3; j++)
            {
              c=in[j];
              value=aux[j];
              c = value==0.0f?0.0f:c/value;
              out[j]=c;
            }
          out[3]=in[3];
          in += 4;
          aux += 3;
          out+= 4;
        }
    }

  return TRUE;
}

static void
gegl_chant_class_init (GeglChantClass *klass)
{
  GeglOperationClass              *operation_class;
  GeglOperationPointComposerClass *point_composer_class;

  operation_class  = GEGL_OPERATION_CLASS (klass);
  point_composer_class     = GEGL_OPERATION_POINT_COMPOSER_CLASS (klass);

  point_composer_class->process = process;
  operation_class->prepare = prepare;

  operation_class->name        = "gegl:divide";
  operation_class->categories  = "compositors:math";
  operation_class->description =
       _("Math operation divide (c = value==0.0f?0.0f:c/value)");
}
#endif
