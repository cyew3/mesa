/*
 * Copyright © Microsoft Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "d3d12_video_buffer.h"
#include "d3d12_resource.h"
#include "d3d12_video_dec.h"

#include "util/format/u_format.h"
#include "util/u_inlines.h"
#include "util/u_memory.h"
#include "util/u_video.h"
#include "vl/vl_video_buffer.h"
#include "util/u_sampler.h"

bool d3d12_video_buffer_is_format_supported(struct pipe_screen *screen,
                                    enum pipe_format format,
                                    enum pipe_video_profile profile,
                                    enum pipe_video_entrypoint entrypoint)
{
   return (format == PIPE_FORMAT_NV12);
}

/**
* creates a video buffer
*/
struct pipe_video_buffer *d3d12_video_buffer_create(struct pipe_context *pipe,
                                                 const struct pipe_video_buffer *tmpl)
{
   assert(pipe);
   assert(tmpl);

   ///
   /// Initialize d3d12_video_buffer
   ///

   
   if( !(tmpl->buffer_format == PIPE_FORMAT_NV12))
   {
      D3D12_LOG_DBG("assert(>buffer_format == PIPE_FORMAT_NV12) failed, skipping");
      return nullptr;
   }
   
   if( !(pipe_format_to_chroma_format(tmpl->buffer_format) == PIPE_VIDEO_CHROMA_FORMAT_420))
   {
      D3D12_LOG_DBG("assert((pipe_format_to_chroma_format(tmpl->buffer_format) == PIPE_VIDEO_CHROMA_FORMAT_420) failed, skipping");
      return nullptr;
   }

   struct d3d12_video_buffer* pD3D12VideoBuffer = CALLOC_STRUCT(d3d12_video_buffer);
   if (!pD3D12VideoBuffer)
   {
      D3D12_LOG_ERROR("[D3D12 Video Driver Error] d3d12_video_buffer_create - Could not allocate memory for d3d12_video_buffer\n");
      return nullptr;
   }

	// Fill base template
   pD3D12VideoBuffer->base = *tmpl;
   pD3D12VideoBuffer->base.buffer_format = tmpl->buffer_format;
   pD3D12VideoBuffer->base.context = pipe;
   pD3D12VideoBuffer->base.width = tmpl->width;
   pD3D12VideoBuffer->base.height = tmpl->height;
   pD3D12VideoBuffer->base.interlaced = tmpl->interlaced; // This is later used by d3d12_video_decoder to use progressive/interlaced decode settings when reading the output texture properties...
   pD3D12VideoBuffer->base.associated_data = nullptr;   
   pD3D12VideoBuffer->base.bind = PIPE_BIND_SAMPLER_VIEW | PIPE_BIND_RENDER_TARGET | PIPE_BIND_DISPLAY_TARGET;

   // Fill vtable
   pD3D12VideoBuffer->base.destroy = d3d12_video_buffer_destroy;
   pD3D12VideoBuffer->base.get_sampler_view_planes = d3d12_video_buffer_get_sampler_view_planes;
   pD3D12VideoBuffer->base.get_sampler_view_components = d3d12_video_buffer_get_sampler_view_components;
   pD3D12VideoBuffer->base.get_surfaces = d3d12_video_buffer_get_surfaces;
   pD3D12VideoBuffer->base.destroy_associated_data = d3d12_video_buffer_destroy_associated_data;

   struct pipe_resource templ;   
   memset(&templ, 0, sizeof(templ));
   templ.target = PIPE_TEXTURE_2D;
   templ.bind = pD3D12VideoBuffer->base.bind;
   templ.format = pD3D12VideoBuffer->base.buffer_format;
   templ.width0 = pD3D12VideoBuffer->base.width;
   templ.height0 = pD3D12VideoBuffer->base.height;
   templ.depth0 = 1;
   templ.array_size = 1;
   templ.flags = 0;

   // This calls d3d12_create_resource as the function ptr is set in d3d12_screen.resource_create
   pD3D12VideoBuffer->m_pD3D12Resource = (struct d3d12_resource*) pipe->screen->resource_create(pipe->screen, &templ);
   
   if(pD3D12VideoBuffer->m_pD3D12Resource == nullptr)
   {
      D3D12_LOG_ERROR("[D3D12 Video Driver Error] d3d12_video_buffer_create - Call to resource_create() to create d3d12_resource failed\n");
      goto failed;
   }

   pD3D12VideoBuffer->m_NumPlanes = util_format_get_num_planes(pD3D12VideoBuffer->m_pD3D12Resource->overall_format);
   assert(pD3D12VideoBuffer->m_NumPlanes == 2);
   return &pD3D12VideoBuffer->base;

failed:
   if (pD3D12VideoBuffer != nullptr)
   {
      d3d12_video_buffer_destroy((struct pipe_video_buffer*) pD3D12VideoBuffer);
   }

   return nullptr;
}

/**
* destroy this video buffer
*/
void  d3d12_video_buffer_destroy(struct pipe_video_buffer *buffer)
{
   struct d3d12_video_buffer* pD3D12VideoBuffer = (struct d3d12_video_buffer*) buffer;
   
   // Destroy pD3D12VideoBuffer->m_pD3D12Resource (if any)
   if(pD3D12VideoBuffer->m_pD3D12Resource)
   {      
      pipe_resource* pBaseResource = &pD3D12VideoBuffer->m_pD3D12Resource->base.b;
      pipe_resource_reference(&pBaseResource, NULL);
   }   

   // Destroy associated data (if any)
   if(pD3D12VideoBuffer->base.associated_data != nullptr)
   {
      d3d12_video_buffer_destroy_associated_data(pD3D12VideoBuffer->base.associated_data);
      // Set to nullptr after cleanup, no dangling pointers
      pD3D12VideoBuffer->base.associated_data = nullptr;
   }

   // Destroy (if any) codec where the associated data came from
   if(pD3D12VideoBuffer->base.codec != nullptr)
   {
      d3d12_video_destroy(pD3D12VideoBuffer->base.codec);
      // Set to nullptr after cleanup, no dangling pointers
      pD3D12VideoBuffer->base.codec = nullptr;
   }   

   for (uint i = 0; i < pD3D12VideoBuffer->m_SurfacePlanes.size(); ++i)
   {
      if(pD3D12VideoBuffer->m_SurfacePlanes[i] != NULL)
      {
         pipe_surface_reference(&pD3D12VideoBuffer->m_SurfacePlanes[i], NULL);
      }            
   }

   for (uint i = 0; i < pD3D12VideoBuffer->m_SurfacePlaneSamplerViews.size(); ++i)
   {
      if(pD3D12VideoBuffer->m_SurfacePlaneSamplerViews[i] != NULL)
      {
         pipe_sampler_view_reference(&pD3D12VideoBuffer->m_SurfacePlaneSamplerViews[i], NULL);
      }
   }      

   for (uint i = 0; i < pD3D12VideoBuffer->m_SurfaceComponentSamplerViews.size(); ++i)
   {
      if(pD3D12VideoBuffer->m_SurfaceComponentSamplerViews[i] != NULL)
      {
         pipe_sampler_view_reference(&pD3D12VideoBuffer->m_SurfaceComponentSamplerViews[i], NULL);
      }      
   }

   // Call dtor to make ComPtr work
   pD3D12VideoBuffer->~d3d12_video_buffer();
   FREE(pD3D12VideoBuffer);   
}

/*
* destroy the associated data
*/
void  d3d12_video_buffer_destroy_associated_data(void *associated_data)
{
   
}

/**
* get an individual surfaces for each plane
*/
struct pipe_surface ** d3d12_video_buffer_get_surfaces(struct pipe_video_buffer *buffer)
{
   assert(buffer);
   struct d3d12_video_buffer* pD3D12VideoBuffer = (struct d3d12_video_buffer*) buffer;
   struct pipe_context *pipe = pD3D12VideoBuffer->base.context;   
   struct pipe_surface surface_template = { };
   
   // Some video frameworks iterate over [0..VL_MAX_SURFACES) and ignore the nullptr entries
   // So we have to null initialize the other surfaces not used from [m_NumPlanes..VL_MAX_SURFACES)
   // Like in src/gallium/frontends/va/surface.c
   pD3D12VideoBuffer->m_SurfacePlanes.resize(VL_MAX_SURFACES, nullptr);

   // pCurPlaneResource refers to the planar resource, not the overall resource.
   // in d3d12_resource this is handled by having a linked list of planes with 
   // d3dRes->base.next ptr to next plane resource
   // starting with the plane 0 being the overall resource
   struct pipe_resource* pBaseResource = &pD3D12VideoBuffer->m_pD3D12Resource->base.b; 

   for (uint PlaneSlice = 0; PlaneSlice < pD3D12VideoBuffer->m_NumPlanes; ++PlaneSlice ) {
      if (!pD3D12VideoBuffer->m_SurfacePlanes[PlaneSlice])
      {
         memset(&surface_template, 0, sizeof(surface_template));
         surface_template.format = util_format_get_plane_format(pD3D12VideoBuffer->m_pD3D12Resource->overall_format, PlaneSlice);         

         pD3D12VideoBuffer->m_SurfacePlanes[PlaneSlice] = pipe->create_surface(
            pipe,
            pBaseResource, 
            &surface_template
         );
         
         if (!pD3D12VideoBuffer->m_SurfacePlanes[PlaneSlice])
         {
            goto error;
         }
      }
   }

   return pD3D12VideoBuffer->m_SurfacePlanes.data();

error:
   for (uint PlaneSlice = 0; PlaneSlice < pD3D12VideoBuffer->m_NumPlanes; ++PlaneSlice )
   {
      pipe_surface_reference(&pD3D12VideoBuffer->m_SurfacePlanes[PlaneSlice], NULL);
   }

   return nullptr;
}

/**
* get an individual sampler view for each plane
*/
struct pipe_sampler_view ** d3d12_video_buffer_get_sampler_view_planes(struct pipe_video_buffer *buffer)
{
   assert(buffer);
   struct d3d12_video_buffer* pD3D12VideoBuffer = (struct d3d12_video_buffer*) buffer;
   struct pipe_context *pipe = pD3D12VideoBuffer->base.context;      
   struct pipe_sampler_view samplerViewTemplate;   

   pD3D12VideoBuffer->m_SurfacePlaneSamplerViews.resize(pD3D12VideoBuffer->m_NumPlanes, nullptr);

   // pCurPlaneResource refers to the planar resource, not the overall resource.
   // in d3d12_resource this is handled by having a linked list of planes with 
   // d3dRes->base.next ptr to next plane resource
   // starting with the plane 0 being the overall resource
   struct pipe_resource* pCurPlaneResource = &pD3D12VideoBuffer->m_pD3D12Resource->base.b; 

   for (uint i = 0; i < pD3D12VideoBuffer->m_NumPlanes; ++i ) {
      if (!pD3D12VideoBuffer->m_SurfacePlaneSamplerViews[i])
      {
         assert(pCurPlaneResource); // the d3d12_resource has a linked list with the exact name of number of elements as planes

         memset(&samplerViewTemplate, 0, sizeof(samplerViewTemplate));
         u_sampler_view_default_template(&samplerViewTemplate, pCurPlaneResource, pCurPlaneResource->format);

         pD3D12VideoBuffer->m_SurfacePlaneSamplerViews[i] = pipe->create_sampler_view(pipe, pCurPlaneResource, &samplerViewTemplate);

         if (!pD3D12VideoBuffer->m_SurfacePlaneSamplerViews[i])
         {
            goto error;
         }
      }

      pCurPlaneResource = pCurPlaneResource->next;
   }

   return pD3D12VideoBuffer->m_SurfacePlaneSamplerViews.data();

error:
   for (uint i = 0; i < pD3D12VideoBuffer->m_NumPlanes; ++i )
   {
      pipe_sampler_view_reference(&pD3D12VideoBuffer->m_SurfacePlaneSamplerViews[i], NULL);
   }

   return nullptr;
}

/**
* get an individual sampler view for each component
*/
struct pipe_sampler_view ** d3d12_video_buffer_get_sampler_view_components(struct pipe_video_buffer *buffer)
{
   assert(buffer);
   struct d3d12_video_buffer* pD3D12VideoBuffer = (struct d3d12_video_buffer*) buffer;
   struct pipe_context *pipe = pD3D12VideoBuffer->base.context;      
   struct pipe_sampler_view samplerViewTemplate;

   // pCurPlaneResource refers to the planar resource, not the overall resource.
   // in d3d12_resource this is handled by having a linked list of planes with 
   // d3dRes->base.next ptr to next plane resource
   // starting with the plane 0 being the overall resource
   struct pipe_resource* pCurPlaneResource = &pD3D12VideoBuffer->m_pD3D12Resource->base.b; 

   // At the end of the loop, "component" will have the total number of items valid in m_SurfaceComponentSamplerViews
   // since component can end up being <= VL_NUM_COMPONENTS, we assume VL_NUM_COMPONENTS first and then resize/adjust to fit the container size 
   // pD3D12VideoBuffer->m_SurfaceComponentSamplerViews to the actual components number
   pD3D12VideoBuffer->m_SurfaceComponentSamplerViews.resize(VL_NUM_COMPONENTS, nullptr);
   uint component = 0; 

   for (uint i = 0; i < pD3D12VideoBuffer->m_NumPlanes; ++i ) 
   {
      // For example num_components would be 1 for the Y plane (R8 in NV12), 2 for the UV plane (R8G8 in NV12)
      unsigned num_components = util_format_get_nr_components(pCurPlaneResource->format);

      for (uint j = 0; j < num_components; ++j, ++component) {
         assert(component < VL_NUM_COMPONENTS);

         if (!pD3D12VideoBuffer->m_SurfaceComponentSamplerViews[component]) {
            memset(&samplerViewTemplate, 0, sizeof(samplerViewTemplate));
            u_sampler_view_default_template(&samplerViewTemplate, pCurPlaneResource, pCurPlaneResource->format);
            
            pD3D12VideoBuffer->m_SurfaceComponentSamplerViews[component] = pipe->create_sampler_view(pipe, pCurPlaneResource, &samplerViewTemplate);
            if (!pD3D12VideoBuffer->m_SurfaceComponentSamplerViews[component])
            {
               goto error;
            }
         }
      }

      pCurPlaneResource = pCurPlaneResource->next;
   }

   // Adjust size to fit component <= VL_NUM_COMPONENTS
   pD3D12VideoBuffer->m_SurfaceComponentSamplerViews.resize(component);

   return pD3D12VideoBuffer->m_SurfaceComponentSamplerViews.data();

error:
   for (uint i = 0; i < pD3D12VideoBuffer->m_NumPlanes; ++i )
   {
      pipe_sampler_view_reference(&pD3D12VideoBuffer->m_SurfaceComponentSamplerViews[i], NULL);
   }

   return nullptr;
}