//if 0
/* pixel-duster
 *
 * the pixel duster data structures and functions are used by multiple ops,
 * but kept in one place since they share so much implementation
 *
 * a context aware pixel inpainting framework..

   avoid building a database of puzzle pieces - since the puzzle pieces are
   pixels samples we condense search space by rectifying rotation - only keep a
   cache.

   (the puzzle pieces are stored by prefix match in a large continuous
   memory region,)

   seed database with used spots and immediate neighbors, for faster
   subsequent lookup

   keep bloom filter - or perhaps even bitmap in GeglBuffer!! for knowing
   contained in db or not.

 * 2018 (c) Øyvind Kolås pippin@gimp.org
 */

/*
    todo:

         threading
           create list of hashtables and to hashtable list per thread

         adjust precision of matching

         replace hashtables with just lists - and include coords in element - perhaps with count..
         for identical entries - thus not losing accurate median computation capabilitiy..
 */
#include <math.h>

#define POW2(x) ((x)*(x))

#define INITIAL_SCORE 1200000000

typedef struct
{
  GeglOperation *op;
  GeglBuffer    *reference;
  GeglBuffer    *input;
  GeglBuffer    *output;
  GeglRectangle  in_rect;
  GeglRectangle  out_rect;
  GeglSampler   *in_sampler_f;
  GeglSampler   *ref_sampler_f;
  GeglSampler   *out_sampler_f;
  int            seek_radius;
  int            minimum_iterations;
  int            maximum_iterations;
  int            max_age;
  float          try_chance;
  float          retry_chance;

  float          ring_gaps[8];

  float          ring_twist;

  float          metric_dist_powk;
  float          metric_empty_hay_score;
  float          metric_empty_needle_score;
  float          metric_cohesion;

  GHashTable    *ht[1];

  GHashTable    *probes_ht;

  float          min_x;
  float          min_y;
  float          max_x;
  float          max_y;

  float          order[512][3];
} PixelDuster;


#define RINGS                   4   // increments works up to 7-8 with no adver
#define RAYS                    6  // good values for testing 6 8 10 12 16
#define NEIGHBORHOOD            (RINGS*RAYS+1)

#define N_SCALE_NEEDLES         3

#define DIRECTION_INVARIANT // uncomment to make search be direction dependent

typedef struct _Probe Probe;


typedef float   needles_t[N_SCALE_NEEDLES][4 * NEIGHBORHOOD ];// should be on stack


struct _Probe {
  int     target_x;
  int     target_y;
  Probe  *neighbors[8];     // cached from coords and iterating all
  int     age;              // not really needed
  float   old_score;        // should be on stack
  float   score;
  int     source_x;
  int     source_y;
};

/* used for hash-table keys */
#define xy2offset(x,y)   GINT_TO_POINTER(((y) * 65536 + (x)))


/* when going through the image preparing the index - only look at the subset
 * needed pixels - and later when fetching out hashed pixels - investigate
 * these ones in particular. would only be win for limited inpainting..
 *
 * making the pixel duster scale invariant on a subpixel level would be neat
 * especially for supersampling, taking the reverse jacobian into account
 * would be even neater.
 *
 */


static void init_order(PixelDuster *duster)
{
  int i;
  duster->order[0][0] = 0;
  duster->order[0][1] = 0;
  duster->order[0][2] = 1.0;
  i = 1;
{

  for (int circleno = 1; circleno < RINGS; circleno++)
  for (float angleno = 0; angleno < RAYS; angleno++)
  {
    float mag = duster->ring_gaps[circleno];
    float x, y;

    x = cosf ((angleno / RAYS + duster->ring_twist*circleno) * M_PI * 2) * mag;
    y = sinf ((angleno / RAYS + duster->ring_twist*circleno) * M_PI * 2) * mag;
    duster->order[i][0] = x;
    duster->order[i][1] = y;
    duster->order[i][2] = powf (1.0 / (POW2(x)+POW2(y)), duster->metric_dist_powk);
    i++;
  }
}
}

static void duster_idx_to_x_y (PixelDuster *duster, int index, float *x, float *y)
{
  *x =  duster->order[index][0];
  *y =  duster->order[index][1];
}


static PixelDuster * pixel_duster_new (GeglBuffer *reference,
                                       GeglBuffer *input,
                                       GeglBuffer *output,
                                       const GeglRectangle *in_rect,
                                       const GeglRectangle *out_rect,
                                       int         seek_radius,
                                       int         minimum_iterations,
                                       int         maximum_iterations,
                                       float       try_chance,
                                       float       retry_chance,
                                       int         improvement_iterations,
                                       float       ring_gap1,
                                       float       ring_gap2,
                                       float       ring_gap3,
                                       float       ring_gap4,
                                       float       ring_twist,
                                       float       metric_dist_powk,
                                       float       metric_empty_hay_score,
                                       float       metric_empty_needle_score,
                                       float       metric_cohesion,

                                       GeglOperation *op)
{
  PixelDuster *ret = g_malloc0 (sizeof (PixelDuster));
  ret->reference   = reference;
  ret->input       = input;
  ret->output      = output;
  ret->seek_radius = seek_radius;
  ret->minimum_iterations = minimum_iterations;
  ret->maximum_iterations = maximum_iterations;
  ret->try_chance   = try_chance;
  ret->retry_chance = retry_chance;
  ret->op = op;
  ret->ring_gaps[1] = ring_gap1;
  ret->ring_gaps[2] = ring_gap2;
  ret->ring_gaps[3] = ring_gap3;
  ret->ring_gaps[4] = ring_gap4;
  ret->max_x = 0;
  ret->max_y = 0;
  ret->min_x = 10000;
  ret->min_y = 10000;
  ret->max_age = improvement_iterations;
  ret->ring_twist = ring_twist;

  ret->in_rect  = *in_rect;
  ret->out_rect = *out_rect;
  ret->metric_dist_powk = metric_dist_powk;
  ret->metric_empty_hay_score = metric_empty_hay_score;
  ret->metric_empty_needle_score = metric_empty_needle_score;
  ret->metric_cohesion = metric_cohesion;

  ret->in_sampler_f = gegl_buffer_sampler_new (input,
                                               babl_format ("RGBA float"),
                                               GEGL_SAMPLER_CUBIC);

  ret->ref_sampler_f = gegl_buffer_sampler_new (reference,
                                               babl_format ("RGBA float"),
                                               GEGL_SAMPLER_CUBIC);

  ret->out_sampler_f = gegl_buffer_sampler_new (output,
                                               babl_format ("RGBA float"),
                                               GEGL_SAMPLER_CUBIC);

  for (int i = 0; i < 1; i++)
    ret->ht[i] = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
  ret->probes_ht = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
  init_order (ret);
  return ret;
}

static inline void pixel_duster_remove_probes (PixelDuster *duster)
{
  if (duster->probes_ht)
  {
    g_hash_table_destroy (duster->probes_ht);
    duster->probes_ht = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
  }
}

static void pixel_duster_destroy (PixelDuster *duster)
{
  pixel_duster_remove_probes (duster);
  for (int i = 0; i < 1; i++)
  {
    g_hash_table_destroy (duster->ht[i]);
  }

  g_object_unref (duster->ref_sampler_f);
  g_object_unref (duster->in_sampler_f);
  g_object_unref (duster->out_sampler_f);

  g_free (duster);
}


void gegl_sampler_prepare (GeglSampler *sampler);

/*  extend with scale factor/matrix
 *
 */
static void extract_site (PixelDuster *duster, GeglBuffer *buffer, double x, double y, float scale, gfloat *dst)
{
  static const Babl *format = NULL;
  GeglSampler *sampler_f;
  if (buffer == duster->output)
  {
    sampler_f = duster->out_sampler_f;
    gegl_sampler_prepare (sampler_f);
  }
  else if (buffer == duster->reference)
  {
    sampler_f   = duster->ref_sampler_f;
  }
  else if (buffer == duster->input)
  {
    sampler_f   = duster->in_sampler_f;
  }

  if (!format){
    format = babl_format ("RGBA float");
  }

  for (int i = 0; i < NEIGHBORHOOD; i++)
  {
    float dx, dy;
    duster_idx_to_x_y (duster, i, &dx, &dy);
    gegl_sampler_get (sampler_f, x + dx * scale, y + dy * scale, NULL, &dst[i*4], 0);
  }

#ifdef DIRECTION_INVARIANT
  {
    int warmest_ray = 0;
    float warmest_ray_energy = 0;
    gfloat tmp[4 * NEIGHBORHOOD];

    for (int ray = 0; ray < RAYS; ray ++)
    {
      float energy = 0.0;
      int count = 0;
      for (int circle = 0; circle < RINGS; circle++)
        if (dst[ ( circle * RAYS  + ray )*4 + 3] > 0.01)
        {
          for (int c = 0; c < 3; c++)
            energy += dst[ ( circle * RAYS  + ray )*4 + c];
          count ++;
        }
      if (count)
        energy/=count;
      if (energy > warmest_ray_energy)
      {
        warmest_ray = ray;
        warmest_ray_energy = energy;
      }
    }

    if (warmest_ray)
    {
      for (int i = 0; i < NEIGHBORHOOD*4; i++)
        tmp[i] = dst[i];

      for (int ray = 0; ray < RAYS; ray ++)
      {
        int swapped_ray = ray + warmest_ray;
        while (swapped_ray >= RAYS) swapped_ray -= RAYS;

        for (int circle = 0; circle < RINGS; circle++)
        for (int c = 0; c < 4; c++)
          dst[ ( circle * RAYS  + ray )*4 + c] =
          tmp[ ( circle * RAYS  + swapped_ray )*4 + c];
       }
    }
  }
#endif
}

static inline int u8_rgb_diff (guchar *a, guchar *b)
{
  return POW2(a[0]-b[0]) * 2 + POW2(a[1]-b[1]) * 3 + POW2(a[2]-b[2]);
}


static inline float f_rgb_diff (float *a, float *b)
{
  return POW2(a[0]-b[0]) + POW2(a[1]-b[1]) + POW2(a[2]-b[2]);
}

static float inline
score_site (PixelDuster *duster,
            Probe       *probe,
            int          x,
            int          y,
            gfloat      *needle,
            gfloat      *hay,
            float        bail)
{
  int i;
  float score = 0;
  /* bail early with really bad score - the target site doesnt have opacity */

  if (hay[3] < 0.001f)
  {
    return INITIAL_SCORE;
  }

  for (i = 1; i < NEIGHBORHOOD /* && score < bail */; i++)
  {
    if (needle[i*4 + 3]>0.001f)
    {
      if (hay[i*4 + 3]>0.001f)
      {
        score += f_rgb_diff (&needle[i*4 + 0], &hay[i*4 + 0]) * duster->order[i][2];
      }
      else
      {
        score += duster->metric_empty_hay_score * duster->order[i][2];
      }
    }
    else
    {
      score += duster->metric_empty_needle_score * duster->order[i][2];
    }

    {
      /* both empty in needle and empty in hay get a similar badness score */
    }
  }

  {
    float sum_x = probe->source_x;
    float sum_y = probe->source_y;
    int count = 1;
    for (int i = 0; i < 8; i++)
    if (probe->neighbors[i])
    {
      sum_x += probe->neighbors[i]->source_x;
      sum_y += probe->neighbors[i]->source_y;
      count++;
    }
    sum_x /= count;
    sum_y /= count;

    score += (POW2(sum_x - probe->source_x) +
             POW2(sum_y - probe->source_y)) * duster->metric_cohesion;

  }
  score = sqrtf (score);
  return score;
}

static Probe *
add_probe (PixelDuster *duster, int target_x, int target_y)
{
  Probe *probe    = g_malloc0 (sizeof (Probe));
  if (target_x < duster->min_x)
    duster->min_x = target_x;
  if (target_y < duster->min_y)
    duster->min_y = target_y;
  if (target_x > duster->max_x)
    duster->max_x = target_x;
  if (target_y > duster->max_y)
    duster->max_y = target_y;

  probe->target_x = target_x;
  probe->target_y = target_y;
  probe->source_x = target_x;
  probe->source_y = target_y;
  probe->score   = INITIAL_SCORE;
  g_hash_table_insert (duster->probes_ht,
                       xy2offset(target_x, target_y), probe);
  return probe;
}

static inline void probe_push (PixelDuster *duster, Probe *probe)
{
}


static gfloat *ensure_hay (PixelDuster *duster, int x, int y)
{
  gfloat *hay = NULL;

  hay = g_hash_table_lookup (duster->ht[0], xy2offset(x, y));

  if (!hay)
    {
      hay = g_malloc ((4 * NEIGHBORHOOD) * 4);
      extract_site (duster, duster->reference, x, y, 1.0, hay);
      g_hash_table_insert (duster->ht[0], xy2offset(x, y), hay);
    }

  return hay;
}


static float
probe_score (PixelDuster *duster,
             Probe       *probe,
             needles_t    needles,
             int          x,
             int          y,
             gfloat      *hay,
             float        bail);

static inline void
probe_prep (PixelDuster *duster,
            Probe *probe,
            needles_t    needles)
{
  gint  dst_x  = probe->target_x;
  gint  dst_y  = probe->target_y;
  extract_site (duster, duster->output, dst_x, dst_y, 1.0, &needles[0][0]);
  if (N_SCALE_NEEDLES > 1)
    extract_site (duster, duster->output, dst_x, dst_y, 0.82, &needles[1][0]);
  if (N_SCALE_NEEDLES > 2)
    extract_site (duster, duster->output, dst_x, dst_y, 1.2, &needles[2][0]);
  if (N_SCALE_NEEDLES > 3)
    extract_site (duster, duster->output, dst_x, dst_y, 0.66, &needles[3][0]);
  if (N_SCALE_NEEDLES > 4)
    extract_site (duster, duster->output, dst_x, dst_y, 1.5, &needles[4][0]);
  if (N_SCALE_NEEDLES > 5)
    extract_site (duster, duster->output, dst_x, dst_y, 2.0,&needles[5][0]);
  if (N_SCALE_NEEDLES > 6)
    extract_site (duster, duster->output, dst_x, dst_y, 0.5, &needles[6][0]);
  probe->old_score = probe->score;

  {
  int neighbours = 0;
  for (GList *p= g_hash_table_get_values (duster->probes_ht); p; p= p->next)
  {
    Probe *oprobe = p->data;
    if (oprobe != probe)
    {
      if ( (probe->target_x == oprobe->target_x - 1) &&
           (probe->target_y == oprobe->target_y))
        probe->neighbors[neighbours++] = oprobe;
      if ( (probe->target_x == oprobe->target_x + 1) &&
           (probe->target_y == oprobe->target_y))
        probe->neighbors[neighbours++] = oprobe;
      if ( (probe->target_x == oprobe->target_x) &&
           (probe->target_y == oprobe->target_y - 1))
        probe->neighbors[neighbours++] = oprobe;
      if ( (probe->target_x == oprobe->target_x) &&
           (probe->target_y == oprobe->target_y + 1))
        probe->neighbors[neighbours++] = oprobe;
      if ( (probe->target_x == oprobe->target_x + 1) &&
           (probe->target_y == oprobe->target_y + 1))
        probe->neighbors[neighbours++] = oprobe;
      if ( (probe->target_x == oprobe->target_x + 1) &&
           (probe->target_y == oprobe->target_y - 1))
        probe->neighbors[neighbours++] = oprobe;
      if ( (probe->target_x == oprobe->target_x - 1) &&
           (probe->target_y == oprobe->target_y + 1))
        probe->neighbors[neighbours++] = oprobe;
      if ( (probe->target_x == oprobe->target_x - 1) &&
           (probe->target_y == oprobe->target_y - 1))
        probe->neighbors[neighbours++] = oprobe;
    }
  }
    for (;neighbours < 8; neighbours++)
      probe->neighbors[neighbours] = NULL;
  }


    if(1)for (int i = 0; i < 4; i++)
    {
      if (probe->neighbors[i])
      {
        Probe *oprobe = probe->neighbors[i];
        int coords[8][2]={{-1,0},
                          {1,0},
                          {0,1},
                          {0,-1},
                          {-1,-1},
                          {1,1},
                          {-1,1},
                          {1,-1} };
        for (int c = 0; c < 8; c++)
        {
          float test_x = oprobe->source_x + coords[c][0];
          float test_y = oprobe->source_y + coords[c][1];
          float *hay = ensure_hay (duster, test_x, test_y);
          float score = probe_score (duster, probe, needles, test_x, test_y, hay, probe->score);
          if (score <= probe->score)
          {
            probe_push (duster, probe);
            probe->source_x = test_x;
            probe->source_y = test_y;
            probe->score = score;
            //probe_update_target (duster, probe);
          }
        }
      }
    }
}

static void
probe_post_search (PixelDuster *duster, Probe *probe)
{
  static const Babl *format = NULL;
  if (!format)
    format = babl_format ("RGBA float");
  probe->age++;

  if (probe->score != probe->old_score)
  {
    gfloat rgba[4];

    gegl_sampler_get (duster->in_sampler_f,
                      probe->source_x, probe->source_y, NULL,
                      &rgba[0], 0);

    gegl_buffer_set (duster->output,
                     GEGL_RECTANGLE(probe->target_x, probe->target_y, 1, 1),
                     0, format, &rgba[0], 0);
  }
}


static float
probe_score (PixelDuster *duster,
             Probe       *probe,
             needles_t    needles,
             int          x,
             int          y,
             gfloat      *hay,
             float        bail)
{
  float best_score = 10000000.0;
  /* bail early with really bad score - the target site doesnt have opacity */

  if (hay[3] < 0.001f)
  {
    return INITIAL_SCORE;
  }

  for (int n = 0; n < N_SCALE_NEEDLES; n++)
  {
    float score = score_site (duster, probe, x, y, &needles[n][0], hay, bail);
    if (score < best_score)
      best_score = score;
  }

  return best_score;
}


static int probe_improve (PixelDuster *duster,
                          Probe       *probe)
{
  needles_t needles;
  //void *ptr[2] = {duster, probe};
  static const Babl *format = NULL;

  if (probe->age >= duster->max_age)
    {
      g_hash_table_remove (duster->probes_ht,
                           xy2offset(probe->target_x, probe->target_y));
      return -1;
    }

  if (!format)
    format = babl_format ("RGBA float");

  probe_prep (duster, probe, needles);

  {
    float mag = duster->seek_radius;
    for (int i = 0; i < 32; i++)
    {
      int dx = g_random_int_range (-mag, mag);
      int dy = g_random_int_range (-mag, mag);
      mag *= 0.8; // reduce seek radius for each iteration
      if (mag < 3)
        mag = 3;
      if (!(dx == 0 && dy == 0))
      {
        int test_x = probe->source_x + dx;
        int test_y = probe->source_y + dy;
        float *hay = ensure_hay (duster, test_x, test_y);
        float score = probe_score (duster, probe, needles, test_x, test_y, hay, probe->score);
        if (score < probe->score)
        {
          probe_push (duster, probe);

          probe->source_x = test_x;
          probe->source_y = test_y;
          probe->score = score;
        }
      }
    }
  }

  probe_post_search (duster, probe);

  return 0;
}

static inline int probes_improve (PixelDuster *duster,
                                  Probe       **probes,
                                  int           n_probes)
{
  for (int i = 0; i < n_probes; i++)
  {
    if (probes[i])
      probe_improve (duster, probes[i]);
  }
  return 0;
}

static inline void pixel_duster_add_probes_for_transparent (PixelDuster *duster)
{
  const Babl *format = babl_format ("RGBA float");
  GeglBufferIterator *i = gegl_buffer_iterator_new (duster->output,
                                                    &duster->out_rect,
                                                    0,
                                                    format,
                                                    GEGL_ACCESS_WRITE,
                                                    GEGL_ABYSS_NONE, 1);
  while (gegl_buffer_iterator_next (i))
  {
    gint x = i->items[0].roi.x;
    gint y = i->items[0].roi.y;
    gint n_pixels  = i->items[0].roi.width * i->items[0].roi.height;
    float *out_pix = i->items[0].data;
    while (n_pixels--)
    {
      if (out_pix[3] < 1.0f /* ||
          (out_pix[0] <= 0.1 &&
           out_pix[1] <= 0.1 &&
           out_pix[2] <= 0.1) */)
      {
        /* we process all - also partially transparent pixels, making the op work well in conjuction with a small hard eraser brush. And improvement could be to re-composite partially transparent pixels back on top as a final step, making the alpha values continuously rather than binary meaningful.
         */
        add_probe (duster, x, y);
      }
      out_pix += 4;

      x++;
      if (x >= i->items[0].roi.x + i->items[0].roi.width)
      {
        x = i->items[0].roi.x;
        y++;
      }
    }
  }
}

static inline void pixel_duster_fill (PixelDuster *duster)
{
  gint missing = 1;
  gint total = 0;
  gint runs = 0;

  while (  (((missing >0) /* && (missing != old_missing) */) ||
           (runs < duster->minimum_iterations)) &&
           runs < duster->maximum_iterations)
  {

    runs++;
    total = 0;
    missing = 0;

  for (GList *p= g_hash_table_get_values (duster->probes_ht); p; p= p->next)
  {
    Probe *probe = p->data;
    gint try_replace;

    if (probe->score == INITIAL_SCORE)
    {
      missing ++;
      try_replace =  0;
    }
    else
    {
      try_replace = ((rand()%100)/100.0) < duster->retry_chance;
    }
    total ++;

#if 0 // can be useful for enlarge ? then needs scale factor
    if ((probe->source_x[0] == probe->target_x &&
         probe->source_y[0] == probe->target_y))
      try_replace = 0;
#endif

    if (probe->score == INITIAL_SCORE || try_replace)
    {
      if ((rand()%100)/100.0 < duster->try_chance)
      {
        probe_improve (duster, probe);
      }
    }
  }

  if (duster->op)
    gegl_operation_progress (duster->op, (total-missing) * 1.0 / total,
                             "finding suitable pixels");
#if 0

  fprintf (stderr, "\r%i/%i %2.2f run#:%i  ", total-missing, total, (total-missing) * 100.0 / total, runs);
#endif
  }
#if 1
  fprintf (stderr, "\n");
#endif
}
