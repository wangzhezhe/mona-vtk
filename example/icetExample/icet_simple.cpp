/* -*- c -*- *****************************************************************
** Copyright (C) 2007 Sandia Corporation
** Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
** license for use of this work by or on behalf of the U.S. Government.
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that this Notice and any statement
** of authorship are reproduced on all copies.
**
** This is a simple example of using the IceT library.  It demonstrates the
** techniques described in the Tutorial chapter of the IceT User’s Guide.
*****************************************************************************/
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>

#include <IceT.h>
#include <IceTGL.h>
#include <IceTMPI.h>

#include <IceTDevCommunication.h>
#include <IceTDevContext.h>
#include <IceTDevImage.h>
#include <IceTDevMatrix.h>
#include <IceTDevPorting.h>

#include <IceTDevDiagnostics.h>
#include <IceTDevState.h>

#include <sys/time.h>
#include <iostream>

#include <icet/mona.hpp>


static int winId;
static IceTContext icetContext;
IceTInt rank;
IceTInt num_proc;

static int SimpleTimingDoRender();
/* Program arguments. */
static IceTInt g_num_tiles_x;
static IceTInt g_num_tiles_y;
static IceTInt g_num_frames;
static IceTInt g_seed;
static IceTFloat g_zoom;
static IceTBoolean g_transparent;
static IceTBoolean g_colored_background;
static IceTBoolean g_no_interlace;
static IceTBoolean g_no_collect;
static IceTBoolean g_use_callback;
static IceTBoolean g_dense_images;
static IceTBoolean g_write_image;
static IceTEnum g_strategy;
static IceTEnum g_single_image_strategy;
static IceTBoolean g_do_magic_k_study;
static IceTInt g_max_magic_k;
static IceTBoolean g_do_image_split_study;
static IceTInt g_min_image_split;
static IceTBoolean g_do_scaling_study_factor_2;
static IceTBoolean g_do_scaling_study_factor_2_3;
static IceTInt g_num_scaling_study_random;
static IceTBoolean g_first_render;
static IceTBoolean g_sync_render;

static float g_color[4];

/* Array for quick opacity lookups. */
#define OPACITY_LOOKUP_SIZE 4096
#define OPACITY_MAX_DT 4
#define OPACITY_COMPUTE_VALUE(dt) (1.0 - pow(M_E, -(dt)))
#define OPACITY_DT_2_INDEX(dt)                                                                     \
  (((dt) < OPACITY_MAX_DT) ? (int)((dt) * (OPACITY_LOOKUP_SIZE / OPACITY_MAX_DT))                  \
                           : OPACITY_LOOKUP_SIZE)
#define OPACITY_INDEX_2_DT(index) ((index) * ((double)OPACITY_MAX_DT / OPACITY_LOOKUP_SIZE))
static IceTDouble g_opacity_lookup[OPACITY_LOOKUP_SIZE + 1];
#define QUICK_OPACITY(dt) (g_opacity_lookup[OPACITY_DT_2_INDEX(dt)])

static FILE* realstdout;

/* Structure used to capture the recursive division of space. */
struct region_divide_struct
{
  int axis;           /* x = 0, y = 1, z = 2: the index to a vector array. */
  float cut;          /* Coordinate where cut occurs. */
  int my_side;        /* -1 on the negative side, 1 on the positive side. */
  int num_other_side; /* Number of partitions on other side. */
  struct region_divide_struct* next;
};

typedef struct region_divide_struct* region_divide;

void initParameters()
{
  g_num_tiles_x = 1;
  g_num_tiles_y = 1;
  g_num_frames = 2;
  g_seed = (IceTInt)time(NULL);
  g_zoom = (IceTFloat)1.0;
  g_transparent = ICET_FALSE;
  g_colored_background = ICET_FALSE;
  g_no_interlace = ICET_FALSE;
  g_no_collect = ICET_FALSE;
  g_use_callback = ICET_FALSE;
  g_dense_images = ICET_FALSE;
  g_sync_render = ICET_FALSE;
  g_write_image = ICET_FALSE;
  g_strategy = ICET_STRATEGY_REDUCE;
  g_single_image_strategy = ICET_SINGLE_IMAGE_STRATEGY_AUTOMATIC;
  g_do_magic_k_study = ICET_FALSE;
  g_max_magic_k = 0;
  g_do_image_split_study = ICET_FALSE;
  g_min_image_split = 0;
  g_do_scaling_study_factor_2 = ICET_FALSE;
  g_do_scaling_study_factor_2_3 = ICET_FALSE;
  g_num_scaling_study_random = 0;
}

int main(int argc, char** argv)
{
  IceTCommunicator icetComm;
  // Setup MPI
  MPI_Init(&argc, &argv);
  initParameters();

  // Setup IceT Context by MPI (ok to generate figures)
  /*
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &num_proc);
  if(rank==0){
    std::cout << "MPI communicator is used"<< std::endl;
  }
  icetComm = icetCreateMPICommunicator(MPI_COMM_WORLD);
  icetContext = icetCreateContext(icetComm);
  icetGLInitialize();
  */

  /* Setup IceT Context by Mona*/
  //-------------------
  // init mona comm
  ABT_init(0, NULL);
  mona_instance_t mona = mona_init("ofi+tcp", NA_TRUE, NULL);

  // caculate num_procs and other addr
  // mona need init all the communicators based on MPI
  int ret;
  na_addr_t self_addr;
  ret = mona_addr_self(mona, &self_addr);
  if (ret != 0)
  {
    throw std::runtime_error("failed to get mona self addr");
    return 0;
  }
  char self_addr_str[128];
  na_size_t self_addr_size = 128;
  ret = mona_addr_to_string(mona, self_addr_str, &self_addr_size, self_addr);
  if (ret != 0)
  {
    throw std::runtime_error("failed to execute mona_addr_to_string");
    return 0;
  }

  MPI_Comm_size(MPI_COMM_WORLD, &num_proc);

  char* other_addr_str = (char*)malloc(128 * num_proc);

  MPI_Allgather(self_addr_str, 128, MPI_BYTE, other_addr_str, 128, MPI_BYTE, MPI_COMM_WORLD);

  na_addr_t* other_addr = (na_addr_t*)malloc(num_proc * sizeof(*other_addr));

  int i;
  for (i = 0; i < num_proc; i++)
  {
    ret = mona_addr_lookup(mona, other_addr_str + 128 * i, other_addr + i);
    if (ret != 0)
    {
      throw std::runtime_error("failed to execute mona_addr_lookup");
      return 0;
    }
  }
  free(other_addr_str);

  // create the mona_comm based on the other_addr
  mona_comm_t mona_comm;
  ret = mona_comm_create(mona, num_proc, other_addr, &mona_comm);
  if (ret != 0)
  {
    throw std::runtime_error("failed to init mona");
    return 0;
  }

  mona_comm_rank(mona_comm,&rank);

  //-------------------

  // set the icet envs
  icetComm = icetCreateMonaCommunicator(mona_comm);
  icetContext = icetCreateContext(icetComm);
  // Attention! the rank assigned by colza might different with the id assigned by the mpi
  if (rank == 0)
  {
    std::cout << "Mona communicator is used" << std::endl;
  }

  icetGLInitialize();

  // do sth based on icet
  std::cout << "rank " << rank << " start" << std::endl;
  SimpleTimingDoRender();
  std::cout << "rank " << rank << " finish" << std::endl;

  MPI_Finalize();
  return 0;
}

void write_ppm(const char* filename, const IceTUByte* image, int width, int height)
{
  FILE* fd;
  int x, y;
  const unsigned char* color;

#ifndef _WIN32
  fd = fopen(filename, "wb");
#else  /*_WIN32*/
  fopen_s(&fd, filename, "wb");
#endif /*_WIN32*/

  fprintf(fd, "P6\n");
  fprintf(fd, "# %s generated by IceT test suite.\n", filename);
  fprintf(fd, "%d %d\n", width, height);
  fprintf(fd, "255\n");

  for (y = height - 1; y >= 0; y--)
  {
    color = image + y * width * 4;
    for (x = 0; x < width; x++)
    {
      fwrite(color, 1, 3, fd);
      color += 4;
    }
  }

  fclose(fd);
}

/* Finds the viewport of the bounds of the locally rendered geometry. */
/* This code is stolen from drawFindContainedViewport in draw.c. */
static void find_contained_viewport(IceTInt contained_viewport[4],
  const IceTDouble projection_matrix[16], const IceTDouble modelview_matrix[16])
{
  IceTDouble total_transform[16];
  IceTDouble left, right, bottom, top;
  IceTDouble* transformed_verts;
  IceTInt global_viewport[4];
  IceTInt num_bounding_verts;
  int i;

  icetGetIntegerv(ICET_GLOBAL_VIEWPORT, global_viewport);

  {
    IceTDouble viewport_matrix[16];
    IceTDouble tmp_matrix[16];

    /* Strange projection matrix that transforms the x and y of normalized
       screen coordinates into viewport coordinates that may be cast to
       integers. */
    viewport_matrix[0] = global_viewport[2];
    viewport_matrix[1] = 0.0;
    viewport_matrix[2] = 0.0;
    viewport_matrix[3] = 0.0;

    viewport_matrix[4] = 0.0;
    viewport_matrix[5] = global_viewport[3];
    viewport_matrix[6] = 0.0;
    viewport_matrix[7] = 0.0;

    viewport_matrix[8] = 0.0;
    viewport_matrix[9] = 0.0;
    viewport_matrix[10] = 2.0;
    viewport_matrix[11] = 0.0;

    viewport_matrix[12] = global_viewport[2] + global_viewport[0] * 2.0;
    viewport_matrix[13] = global_viewport[3] + global_viewport[1] * 2.0;
    viewport_matrix[14] = 0.0;
    viewport_matrix[15] = 2.0;

    icetMatrixMultiply(
      tmp_matrix, (const IceTDouble*)projection_matrix, (const IceTDouble*)modelview_matrix);
    icetMatrixMultiply(
      total_transform, (const IceTDouble*)viewport_matrix, (const IceTDouble*)tmp_matrix);
  }

  icetGetIntegerv(ICET_NUM_BOUNDING_VERTS, &num_bounding_verts);
  transformed_verts = (IceTDouble*)icetGetStateBuffer(
    ICET_TRANSFORMED_BOUNDS, sizeof(IceTDouble) * num_bounding_verts * 4);

  /* Transform each vertex to find where it lies in the global viewport and
     normalized z.  Leave the results in homogeneous coordinates for now. */
  {
    const IceTDouble* bound_vert = icetUnsafeStateGetDouble(ICET_GEOMETRY_BOUNDS);
    for (i = 0; i < num_bounding_verts; i++)
    {
      IceTDouble bound_vert_4vec[4];
      bound_vert_4vec[0] = bound_vert[3 * i + 0];
      bound_vert_4vec[1] = bound_vert[3 * i + 1];
      bound_vert_4vec[2] = bound_vert[3 * i + 2];
      bound_vert_4vec[3] = 1.0;
      icetMatrixVectorMultiply(transformed_verts + 4 * i, (const IceTDouble*)total_transform,
        (const IceTDouble*)bound_vert_4vec);
    }
  }

  /* Set absolute mins and maxes. */
  left = global_viewport[0] + global_viewport[2];
  right = global_viewport[0];
  bottom = global_viewport[1] + global_viewport[3];
  top = global_viewport[1];

  /* Now iterate over all the transformed verts and adjust the absolute mins
     and maxs to include them all. */
  for (i = 0; i < num_bounding_verts; i++)
  {
    IceTDouble* vert = transformed_verts + 4 * i;

    /* Check to see if the vertex is in front of the near cut plane.  This
       is true when z/w >= -1 or z + w >= 0.  The second form is better just
       in case w is 0. */
    if (vert[2] + vert[3] >= 0.0)
    {
      /* Normalize homogeneous coordinates. */
      IceTDouble invw = 1.0 / vert[3];
      IceTDouble x = vert[0] * invw;
      IceTDouble y = vert[1] * invw;

      /* Update contained region. */
      if (left > x)
        left = x;
      if (right < x)
        right = x;
      if (bottom > y)
        bottom = y;
      if (top < y)
        top = y;
    }
    else
    {
      /* The vertex is being clipped by the near plane.  In perspective
         mode, vertices behind the near clipping plane can sometimes give
         misleading projections.  Instead, find all the other vertices on
         the other side of the near plane, compute the intersection of the
         segment between the two points and the near plane (in homogeneous
         coordinates) and use that as the projection. */
      int j;
      for (j = 0; j < num_bounding_verts; j++)
      {
        IceTDouble* vert2 = transformed_verts + 4 * j;
        double t;
        IceTDouble x, y, invw;
        if (vert2[2] + vert2[3] < 0.0)
        {
          /* Ignore other points behind near plane. */
          continue;
        }
        /* Let the two points in question be v_i and v_j.  Define the
           segment between them with the parametric equation
           p(t) = (vert - vert2)t + vert2.  First, find t where the z and
           w coordinates of p(t) sum to zero. */
        t = (vert2[2] + vert2[3]) / (vert2[2] - vert[2] + vert2[3] - vert[3]);
        /* Use t to find the intersection point.  While we are at it,
           normalize the resulting coordinates.  We don't need z because
           we know it is going to be -1. */
        invw = 1.0 / ((vert[3] - vert2[3]) * t + vert2[3]);
        x = ((vert[0] - vert2[0]) * t + vert2[0]) * invw;
        y = ((vert[1] - vert2[1]) * t + vert2[1]) * invw;

        /* Update contained region. */
        if (left > x)
          left = x;
        if (right < x)
          right = x;
        if (bottom > y)
          bottom = y;
        if (top < y)
          top = y;
      }
    }
  }

  left = floor(left);
  right = ceil(right);
  bottom = floor(bottom);
  top = ceil(top);

  /* Clip bounds to global viewport. */
  if (left < global_viewport[0])
    left = global_viewport[0];
  if (right > global_viewport[0] + global_viewport[2])
    right = global_viewport[0] + global_viewport[2];
  if (bottom < global_viewport[1])
    bottom = global_viewport[1];
  if (top > global_viewport[1] + global_viewport[3])
    top = global_viewport[1] + global_viewport[3];

  /* Use this information to build a containing viewport. */
  contained_viewport[0] = (IceTInt)left;
  contained_viewport[1] = (IceTInt)bottom;
  contained_viewport[2] = (IceTInt)(right - left);
  contained_viewport[3] = (IceTInt)(top - bottom);
}

void printrank(const char* fmt, ...)
{
  va_list ap;
  IceTInt rank;

  icetGetIntegerv(ICET_RANK, &rank);

  printf("%d> ", rank);
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
  fflush(stdout);
}
#define NUM_HEX_PLANES 6
struct hexahedron
{
  IceTDouble planes[NUM_HEX_PLANES][4];
};

/* Plane equations for unit box on origin. */
struct hexahedron unit_box = { { { -1.0, 0.0, 0.0, -0.5 }, { 1.0, 0.0, 0.0, -0.5 },
  { 0.0, -1.0, 0.0, -0.5 }, { 0.0, 1.0, 0.0, -0.5 }, { 0.0, 0.0, -1.0, -0.5 },
  { 0.0, 0.0, 1.0, -0.5 } } };

static void intersect_ray_plane(const IceTDouble* ray_origin, const IceTDouble* ray_direction,
  const IceTDouble* plane, IceTDouble* distance, IceTBoolean* front_facing, IceTBoolean* parallel)
{
  IceTDouble distance_numerator = icetDot3(plane, ray_origin) + plane[3];
  IceTDouble distance_denominator = icetDot3(plane, ray_direction);

  if (distance_denominator == 0.0)
  {
    *parallel = ICET_TRUE;
    *front_facing = (distance_numerator > 0);
  }
  else
  {
    *parallel = ICET_FALSE;
    *distance = -distance_numerator / distance_denominator;
    *front_facing = (distance_denominator < 0);
  }
}

/* This algorithm (and associated intersect_ray_plane) come from Graphics Gems
 * II, Fast Ray-Convex Polyhedron Intersection by Eric Haines. */
static void intersect_ray_hexahedron(const IceTDouble* ray_origin, const IceTDouble* ray_direction,
  const struct hexahedron hexahedron, IceTDouble* near_distance, IceTDouble* far_distance,
  IceTInt* near_plane_index, IceTBoolean* intersection_happened)
{
  int planeIdx;

  *near_distance = 0.0;
  *far_distance = 2.0;
  *near_plane_index = -1;

  for (planeIdx = 0; planeIdx < NUM_HEX_PLANES; planeIdx++)
  {
    IceTDouble distance;
    IceTBoolean front_facing;
    IceTBoolean parallel;

    intersect_ray_plane(
      ray_origin, ray_direction, hexahedron.planes[planeIdx], &distance, &front_facing, &parallel);

    if (!parallel)
    {
      if (front_facing)
      {
        if (*near_distance < distance)
        {
          *near_distance = distance;
          *near_plane_index = planeIdx;
        }
      }
      else
      {
        if (distance < *far_distance)
        {
          *far_distance = distance;
        }
      }
    }
    else
    { /*parallel*/
      if (front_facing)
      {
        /* Ray missed parallel plane.  No intersection. */
        *intersection_happened = ICET_FALSE;
        return;
      }
    }
  }

  *intersection_happened = (*near_distance < *far_distance);
}

static void draw(const IceTDouble* projection_matrix, const IceTDouble* modelview_matrix,
  const IceTFloat* background_color, const IceTInt* readback_viewport, IceTImage result)
{
  IceTDouble transform[16];
  IceTDouble inverse_transpose_transform[16];
  IceTBoolean success;
  int planeIdx;
  struct hexahedron transformed_box;
  IceTInt width;
  IceTInt height;
  IceTFloat* colors_float = NULL;
  IceTUByte* colors_byte = NULL;
  IceTFloat* depths = NULL;
  IceTInt pixel_x;
  IceTInt pixel_y;
  IceTDouble ray_origin[3];
  IceTDouble ray_direction[3];
  IceTFloat background_depth;
  IceTFloat background_alpha;

  icetMatrixMultiply(transform, projection_matrix, modelview_matrix);

  success = icetMatrixInverseTranspose((const IceTDouble*)transform, inverse_transpose_transform);

  if (!success)
  {
    printrank("ERROR: Inverse failed.\n");
  }

  for (planeIdx = 0; planeIdx < NUM_HEX_PLANES; planeIdx++)
  {
    const IceTDouble* original_plane = unit_box.planes[planeIdx];
    IceTDouble* transformed_plane = transformed_box.planes[planeIdx];

    icetMatrixVectorMultiply(transformed_plane, inverse_transpose_transform, original_plane);
  }

  width = icetImageGetWidth(result);
  height = icetImageGetHeight(result);
  g_transparent = ICET_FALSE;
  if (g_transparent)
  {
    colors_float = icetImageGetColorf(result);
  }
  else
  {
    colors_byte = icetImageGetColorub(result);
    depths = icetImageGetDepthf(result);
  }

  g_dense_images = ICET_FALSE;
  if (!g_dense_images)
  {
    background_depth = 1.0f;
    background_alpha = background_color[3];
  }
  else
  {
    IceTSizeType pixel_index;

    /* To fake dense images, use a depth and alpha for the background that
     * IceT will not recognize as background. */
    background_depth = 0.999f;
    background_alpha = (background_color[3] == 0) ? 0.001f : background_color[3];

    /* Clear out the the images to background so that pixels outside of
     * the contained viewport have valid values. */
    for (pixel_index = 0; pixel_index < width * height; pixel_index++)
    {
      if (g_transparent)
      {
        IceTFloat* color_dest = colors_float + 4 * pixel_index;
        color_dest[0] = background_color[0];
        color_dest[1] = background_color[1];
        color_dest[2] = background_color[2];
        color_dest[3] = background_alpha;
      }
      else
      {
        IceTUByte* color_dest = colors_byte + 4 * pixel_index;
        IceTFloat* depth_dest = depths + pixel_index;
        color_dest[0] = (IceTUByte)(background_color[0] * 255);
        color_dest[1] = (IceTUByte)(background_color[1] * 255);
        color_dest[2] = (IceTUByte)(background_color[2] * 255);
        color_dest[3] = (IceTUByte)(background_alpha * 255);
        depth_dest[0] = background_depth;
      }
    }
  }

  ray_direction[0] = ray_direction[1] = 0.0;
  ray_direction[2] = 1.0;
  ray_origin[2] = -1.0;
  for (pixel_y = readback_viewport[1]; pixel_y < readback_viewport[1] + readback_viewport[3];
       pixel_y++)
  {
    ray_origin[1] = (2.0 * pixel_y) / height - 1.0;
    for (pixel_x = readback_viewport[0]; pixel_x < readback_viewport[0] + readback_viewport[2];
         pixel_x++)
    {
      IceTDouble near_distance;
      IceTDouble far_distance;
      IceTInt near_plane_index;
      IceTBoolean intersection_happened;
      IceTFloat color[4];
      IceTFloat depth;

      ray_origin[0] = (2.0 * pixel_x) / width - 1.0;

      intersect_ray_hexahedron(ray_origin, ray_direction, transformed_box, &near_distance,
        &far_distance, &near_plane_index, &intersection_happened);

      if (intersection_happened)
      {
        const IceTDouble* near_plane;
        IceTDouble shading;

        near_plane = transformed_box.planes[near_plane_index];
        shading = -near_plane[2] / sqrt(icetDot3(near_plane, near_plane));

        color[0] = g_color[0] * (IceTFloat)shading;
        color[1] = g_color[1] * (IceTFloat)shading;
        color[2] = g_color[2] * (IceTFloat)shading;
        color[3] = g_color[3];
        depth = (IceTFloat)(0.5 * near_distance);
        if (g_transparent)
        {
          /* Modify color by an opacity determined by thickness. */
          IceTDouble thickness = far_distance - near_distance;
          IceTDouble opacity = QUICK_OPACITY(4.0 * thickness);
          if (opacity < 0.001)
          {
            opacity = 0.001;
          }
          color[0] *= (IceTFloat)opacity;
          color[1] *= (IceTFloat)opacity;
          color[2] *= (IceTFloat)opacity;
          color[3] *= (IceTFloat)opacity;
        }
      }
      else
      {
        color[0] = background_color[0];
        color[1] = background_color[1];
        color[2] = background_color[2];
        color[3] = background_alpha;
        depth = background_depth;
      }

      if (g_transparent)
      {
        IceTFloat* color_dest = colors_float + 4 * (pixel_y * width + pixel_x);
        color_dest[0] = color[0];
        color_dest[1] = color[1];
        color_dest[2] = color[2];
        color_dest[3] = color[3];
      }
      else
      {
        IceTUByte* color_dest = colors_byte + 4 * (pixel_y * width + pixel_x);
        IceTFloat* depth_dest = depths + pixel_y * width + pixel_x;
        color_dest[0] = (IceTUByte)(color[0] * 255);
        color_dest[1] = (IceTUByte)(color[1] * 255);
        color_dest[2] = (IceTUByte)(color[2] * 255);
        color_dest[3] = (IceTUByte)(color[3] * 255);
        depth_dest[0] = depth;
      }
    }
  }

  if (g_first_render)
  {
    if (g_sync_render)
    {
      /* The rendering we are using here is pretty crummy.  It is not
         meant to be practical but to create reasonable images to
         composite.  One problem with it is that the render times are not
         well balanced even though everyone renders roughly the same sized
         object.  If you want to time the composite performance, this can
         interfere with the measurements.  To get around this problem, do
         a barrier that makes it look as if all rendering finishes at the
         same time.  Note that there is a remote possibility that not
         every process will render something, in which case this will
         deadlock.  Note that we make sure only to sync once to get around
         the less remote possibility that some, but not all, processes
         render more than once. */
      icetCommBarrier();
    }
    g_first_render = ICET_FALSE;
  }
}

void printstat(const char* fmt, ...)
{
  va_list ap;
  IceTInt rank;

  icetGetIntegerv(ICET_RANK, &rank);

  if ((rank == 0) || (realstdout == NULL))
  {
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);
  }
}

/* Given the rank of this process in all of them, divides the unit box
 * centered on the origin evenly (w.r.t. volume) amongst all processes.  The
 * region for this process, characterized by the min and max corners, is
 * returned in the bounds_min and bounds_max parameters. */
static void find_region(
  int rank, int num_proc, float* bounds_min, float* bounds_max, region_divide* divisions)
{
  int axis = 0;
  int start_rank = 0;      /* The first rank. */
  int end_rank = num_proc; /* One after the last rank. */
  region_divide current_division = NULL;

  bounds_min[0] = bounds_min[1] = bounds_min[2] = -0.5f;
  bounds_max[0] = bounds_max[1] = bounds_max[2] = 0.5f;

  *divisions = NULL;

  /* Recursively split each axis, dividing the number of processes in my group
     in half each time. */
  while (1 < (end_rank - start_rank))
  {
    float length = bounds_max[axis] - bounds_min[axis];
    int middle_rank = (start_rank + end_rank) / 2;
    float region_cut;
    region_divide new_divide = (region_divide)malloc(sizeof(struct region_divide_struct));

    /* Skew the place where we cut the region based on the relative size
     * of the group size on each side, which may be different if the
     * group cannot be divided evenly. */
    region_cut = (bounds_min[axis] + length * (middle_rank - start_rank) / (end_rank - start_rank));

    new_divide->axis = axis;
    new_divide->cut = region_cut;
    new_divide->next = NULL;

    if (rank < middle_rank)
    {
      /* My rank is in the lower region. */
      new_divide->my_side = -1;
      new_divide->num_other_side = end_rank - middle_rank;
      bounds_max[axis] = region_cut;
      end_rank = middle_rank;
    }
    else
    {
      /* My rank is in the upper region. */
      new_divide->my_side = 1;
      new_divide->num_other_side = middle_rank - start_rank;
      bounds_min[axis] = region_cut;
      start_rank = middle_rank;
    }

    if (current_division != NULL)
    {
      current_division->next = new_divide;
    }
    else
    {
      *divisions = new_divide;
    }
    current_division = new_divide;

    axis = (axis + 1) % 3;
  }
}

/* Free a region divide structure. */
static void free_region_divide(region_divide divisions)
{
  region_divide current_division = divisions;
  while (current_division != NULL)
  {
    region_divide next_division = current_division->next;
    free(current_division);
    current_division = next_division;
  }
}

static int SimpleTimingDoRender()
{
  const char* strategy_name;
  const char* si_strategy_name;
  IceTInt max_image_split;

  // init parameters
  // TODO check the number of the processes
  IceTInt g_num_tiles_x = 2;
  IceTInt g_num_tiles_y = 2;

  int width = 1024;
  int height = 768;

  IceTSizeType SCREEN_WIDTH = width;
  IceTSizeType SCREEN_HEIGHT = height;

  float aspect = ((float)(g_num_tiles_x * SCREEN_WIDTH) / (float)(g_num_tiles_y * SCREEN_HEIGHT));
  int frame;
  float bounds_min[3];
  float bounds_max[3];

  region_divide region_divisions;

  IceTDouble projection_matrix[16];
  IceTFloat background_color[4];

  IceTImage pre_rendered_image = icetImageNull();
  void* pre_rendered_image_buffer = NULL;

  // timings_type *timing_array;

  /* Normally, the first thing that you do is set up your communication and
   * then create at least one IceT context.  This has already been done in the
   * calling function (i.e. icetTests_mpi.c).  See the init_mpi in
   * test_mpi.h for an example.
   */

  // init_opacity_lookup();

  /* If we had set up the communication layer ourselves, we could have gotten
   * these parameters directly from it.  Since we did not, this provides an
   * alternate way. */

  g_colored_background = ICET_FALSE;
  if (g_colored_background)
  {
    background_color[0] = 0.2f;
    background_color[1] = 0.5f;
    background_color[2] = 0.7f;
    background_color[3] = 1.0f;
  }
  else
  {
    background_color[0] = 0.0f;
    background_color[1] = 0.0f;
    background_color[2] = 0.0f;
    background_color[3] = 0.0f;
  }

  /* Give IceT a function that will issue the drawing commands. */
  icetDrawCallback(draw);

  IceTBoolean g_colored_background = ICET_FALSE;
  /* Other IceT state. */
  if (g_transparent)
  {
    icetCompositeMode(ICET_COMPOSITE_MODE_BLEND);
    icetSetColorFormat(ICET_IMAGE_COLOR_RGBA_FLOAT);
    icetSetDepthFormat(ICET_IMAGE_DEPTH_NONE);
    icetEnable(ICET_CORRECT_COLORED_BACKGROUND);
  }
  else
  {
    icetCompositeMode(ICET_COMPOSITE_MODE_Z_BUFFER);
    icetSetColorFormat(ICET_IMAGE_COLOR_RGBA_UBYTE);
    icetSetDepthFormat(ICET_IMAGE_DEPTH_FLOAT);
  }

  IceTBoolean g_no_interlace = ICET_FALSE;
  if (g_no_interlace)
  {
    icetDisable(ICET_INTERLACE_IMAGES);
  }
  else
  {
    icetEnable(ICET_INTERLACE_IMAGES);
  }

  IceTBoolean g_no_collect = ICET_FALSE;
  if (g_no_collect)
  {
    icetDisable(ICET_COLLECT_IMAGES);
  }
  else
  {
    icetEnable(ICET_COLLECT_IMAGES);
  }

  /* Give IceT the bounds of the polygons that will be drawn.  Note that
   * IceT will take care of any transformation that gets passed to
   * icetDrawFrame. */
  icetBoundingBoxd(-0.5f, 0.5f, -0.5, 0.5, -0.5, 0.5);

  /* Determine the region we want the local geometry to be in.  This will be
   * used for the modelview transformation later. */
  find_region(rank, num_proc, bounds_min, bounds_max, &region_divisions);
  /* Set up the tiled display.  The asignment of displays to processes is
   * arbitrary because, as this is a timing test, I am not too concerned
   * about who shows what. */
  if (g_num_tiles_x * g_num_tiles_y <= num_proc)
  {
    int x, y, display_rank;
    icetResetTiles();
    display_rank = 0;
    for (y = 0; y < g_num_tiles_y; y++)
    {
      for (x = 0; x < g_num_tiles_x; x++)
      {
        icetAddTile(x * (IceTInt)SCREEN_WIDTH, y * (IceTInt)SCREEN_HEIGHT, SCREEN_WIDTH,
          SCREEN_HEIGHT, display_rank);
        display_rank++;
      }
    }
  }
  else
  {
    printstat("Not enough processes to %dx%d tiles.\n", g_num_tiles_x, g_num_tiles_y);
    return -1;
  }

  if (!g_use_callback)
  {
    IceTInt global_viewport[4];
    IceTInt width, height;
    IceTInt buffer_size;

    icetGetIntegerv(ICET_GLOBAL_VIEWPORT, global_viewport);
    width = global_viewport[2];
    height = global_viewport[3];

    buffer_size = icetImageBufferSize(width, height);
    pre_rendered_image_buffer = malloc(buffer_size);
    pre_rendered_image = icetImageAssignBuffer(pre_rendered_image_buffer, width, height);
  }
  g_strategy = ICET_STRATEGY_REDUCE;
  g_single_image_strategy = ICET_SINGLE_IMAGE_STRATEGY_AUTOMATIC;
  icetStrategy(g_strategy);
  icetSingleImageStrategy(g_single_image_strategy);

  /* Set up the projection matrix. */
  icetMatrixFrustum(-0.65 * aspect / g_zoom, 0.65 * aspect / g_zoom, -0.65 / g_zoom, 0.65 / g_zoom,
    3.0, 5.0, projection_matrix);

  if (rank % 10 < 7)
  {
    IceTInt color_bits = rank % 10 + 1;
    g_color[0] = (float)(color_bits % 2);
    g_color[1] = (float)((color_bits / 2) % 2);
    g_color[2] = (float)((color_bits / 4) % 2);
    g_color[3] = 1.0f;
  }
  else
  {
    g_color[0] = g_color[1] = g_color[2] = 0.5f;
    g_color[rank % 10 - 7] = 0.0f;
    g_color[3] = 1.0f;
  }

  /* Initialize randomness. */
  if (rank == 0)
  {
    int i;
    printstat("Seed = %d\n", g_seed);
    for (i = 1; i < num_proc; i++)
    {
      icetCommSend(&g_seed, 1, ICET_INT, i, 33);
    }
  }
  else
  {
    icetCommRecv(&g_seed, 1, ICET_INT, 0, 33);
  }

  srand(g_seed);

  strategy_name = icetGetStrategyName();
  if (g_single_image_strategy == ICET_SINGLE_IMAGE_STRATEGY_RADIXK)
  {
    static char name_buffer[256];
    IceTInt magic_k;

    icetGetIntegerv(ICET_MAGIC_K, &magic_k);
    icetSnprintf(name_buffer, 256, "radix-k %d", (int)magic_k);
    si_strategy_name = name_buffer;
  }
  else if (g_single_image_strategy == ICET_SINGLE_IMAGE_STRATEGY_RADIXKR)
  {
    static char name_buffer[256];
    IceTInt magic_k;

    icetGetIntegerv(ICET_MAGIC_K, &magic_k);
    icetSnprintf(name_buffer, 256, "radix-kr %d", (int)magic_k);
    si_strategy_name = name_buffer;
  }
  else
  {
    si_strategy_name = icetGetSingleImageStrategyName();
  }

  icetGetIntegerv(ICET_MAX_IMAGE_SPLIT, &max_image_split);

  for (frame = 0; frame < g_num_frames; frame++)
  {
    IceTDouble elapsed_time;
    IceTDouble modelview_matrix[16];
    IceTImage image;

    /* We can set up a modelview matrix here and IceT will factor this in
     * determining the screen projection of the geometry. */
    icetMatrixIdentity(modelview_matrix);

    /* Move geometry back so that it can be seen by the camera. */
    icetMatrixMultiplyTranslate(modelview_matrix, 0.0, 0.0, -4.0);

    /* Rotate to some random view.
    icetMatrixMultiplyRotate(modelview_matrix, (360.0 * rand()) / RAND_MAX, 1.0, 0.0, 0.0);
    icetMatrixMultiplyRotate(modelview_matrix, (360.0 * rand()) / RAND_MAX, 0.0, 1.0, 0.0);
    icetMatrixMultiplyRotate(modelview_matrix, (360.0 * rand()) / RAND_MAX, 0.0, 0.0, 1.0);
    */
    /* Rotate to fixed angle. */
    double test_angle = 0.3;
    icetMatrixMultiplyRotate(modelview_matrix, (360.0 * test_angle), 1.0, 0.0, 0.0);
    icetMatrixMultiplyRotate(modelview_matrix, (360.0 * test_angle), 0.0, 1.0, 0.0);
    icetMatrixMultiplyRotate(modelview_matrix, (360.0 * test_angle), 0.0, 0.0, 1.0);

    /* Determine view ordering of geometry based on camera position
       (represented by the current projection and modelview matrices). */
    // the transparent is set as false in this test case
    // if (g_transparent)
    //{
    //  find_composite_order(projection_matrix, modelview_matrix, region_divisions);
    //}

    /* Translate the unit box centered on the origin to the region specified
     * by bounds_min and bounds_max. */
    icetMatrixMultiplyTranslate(modelview_matrix, bounds_min[0], bounds_min[1], bounds_min[2]);
    icetMatrixMultiplyScale(modelview_matrix, bounds_max[0] - bounds_min[0],
      bounds_max[1] - bounds_min[1], bounds_max[2] - bounds_min[2]);
    icetMatrixMultiplyTranslate(modelview_matrix, 0.5, 0.5, 0.5);

    if (!g_use_callback)
    {
      /* Draw the image for the frame. */
      IceTInt contained_viewport[4];
      find_contained_viewport(contained_viewport, projection_matrix, modelview_matrix);
      if (g_transparent)
      {
        IceTFloat black[4] = { 0.0, 0.0, 0.0, 0.0 };
        draw(projection_matrix, modelview_matrix, black, contained_viewport, pre_rendered_image);
      }
      else
      {
        draw(projection_matrix, modelview_matrix, background_color, contained_viewport,
          pre_rendered_image);
      }
    }

    if (g_dense_images)
    {
      /* With dense images, we want IceT to load in all pixels, so clear
       * out the bounding box/vertices. */
      icetBoundingVertices(0, ICET_VOID, 0, 0, NULL);
    }

    /* Get everyone to start at the same time. need to wait when there is timer */
    // icetCommBarrier();

    if (g_use_callback)
    {
      /* Instead of calling draw() directly, call it indirectly through
       * icetDrawFrame().  IceT will automatically handle image
       * compositing. */
      g_first_render = ICET_TRUE;
      image = icetDrawFrame(projection_matrix, modelview_matrix, background_color);
    }
    else
    {
      image = icetCompositeImage(icetImageGetColorConstVoid(pre_rendered_image, NULL),
        g_transparent ? NULL : icetImageGetDepthConstVoid(pre_rendered_image, NULL), NULL,
        projection_matrix, modelview_matrix, background_color);
    }

    /* Let everyone catch up before finishing the frame. */
    icetCommBarrier();

    /* Write out image to verify rendering occurred correctly. */
    IceTBoolean g_write_image = ICET_TRUE;
    if (g_write_image && (rank < (g_num_tiles_x * g_num_tiles_y)) && (frame == 0))
    {
      IceTUByte* buffer = (IceTUByte*)malloc(SCREEN_WIDTH * SCREEN_HEIGHT * 4);
      char filename[256];
      icetImageCopyColorub(image, buffer, ICET_IMAGE_COLOR_RGBA_UBYTE);
      icetSnprintf(filename, 256, "SimpleTiming%02d.ppm", rank);
      write_ppm(filename, buffer, (int)SCREEN_WIDTH, (int)SCREEN_HEIGHT);
      free(buffer);
    }
  }

  free_region_divide(region_divisions);
  // free(timing_array);

  pre_rendered_image = icetImageNull();
  if (pre_rendered_image_buffer != NULL)
  {
    free(pre_rendered_image_buffer);
  }

  return 0;
}