/**************************************************************************
 *
 * Copyright 2015 Advanced Micro Devices, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include <assert.h>

#include "pipe/p_screen.h"
#include "pipe-loader/pipe_loader.h"
#include "frontend/drm_driver.h"

#include "util/u_memory.h"
#include "vl/vl_winsys.h"
#include <X11/Xlib.h>

#include "target-helpers/sw_helper_public.h"
#include "gallium/winsys/sw/null/null_sw_winsys.h"
#include "gallium/winsys/sw/kms-dri/kms_dri_sw_winsys.h"

static void
vl_swrast_screen_destroy(struct vl_screen *vscreen);

struct vl_screen *
vl_swrast_screen_create(int fd)
{
   struct vl_screen *vscreen;

   vscreen = CALLOC_STRUCT(vl_screen);
   if (!vscreen)
      return NULL;
   
   // Create a pipe_screen
   struct sw_winsys * winsysObj = NULL;
   if(fd < 0)
   {
      fprintf(stderr, "[vl_swrast_screen_create] - Invalid FD %d for kms_dri_winsys, creating NULL winsys instead.\n", fd);
      winsysObj = null_sw_create();
   }
   else
   {         
      winsysObj = kms_dri_create_winsys(fd);
      if(winsysObj == NULL)
      {
         fprintf(stderr, "[vl_swrast_screen_create] Creating DRM winsys with fd %d failed!\n", fd);
      }
   }
   
   struct pipe_screen* pScreen = sw_screen_create(winsysObj);

   if(!pScreen)
   {
      goto release_pipe;
   }

   // wrap it to create a software screen that can share resources
    if (pipe_loader_sw_probe_wrapped(&vscreen->dev, pScreen))
        vscreen->pscreen = pipe_loader_create_screen(vscreen->dev);

   if (!vscreen->pscreen)
      goto release_pipe;

   vscreen->destroy = vl_swrast_screen_destroy;
   vscreen->texture_from_drawable = NULL;
   vscreen->get_dirty_area = NULL;
   vscreen->get_timestamp = NULL;
   vscreen->set_next_timestamp = NULL;
   vscreen->get_private = NULL;
   return vscreen;

release_pipe:
   if (vscreen->dev)
      pipe_loader_release(&vscreen->dev, 1);

   FREE(vscreen);
   return NULL;
}

static void
vl_swrast_screen_destroy(struct vl_screen *vscreen)
{
   assert(vscreen);

   vscreen->pscreen->destroy(vscreen->pscreen);
   pipe_loader_release(&vscreen->dev, 1);
   /* CHECK: The VAAPI loader/user preserves ownership of the original fd */
   FREE(vscreen);
}