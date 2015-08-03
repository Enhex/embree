// ======================================================================== //
// Copyright 2009-2015 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDI &TIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "grid_soa.h"
#include "../../common/ray.h"
#include "triangle_intersector_pluecker.h"

namespace embree
{
  namespace isa
  {
    class GridSOAIntersector1
    {
    public:
      typedef SubdivPatch1Cached Primitive;
      
      /*! Precalculations for subdiv patch intersection */
      class Precalculations 
      { 
      public:
        __forceinline Precalculations (Ray& ray, const void* ptr) 
          : patch(nullptr) {}
        
        __forceinline ~Precalculations() 
        {
	  if (patch)
            SharedLazyTessellationCache::sharedLazyTessellationCache.unlock();
        }
        
      public:
        SubdivPatch1Cached* patch;
      };
      
      struct Gather2x3
      {
        typedef bool4 vbool;
        typedef int4 vint;
        typedef float4 vfloat;
        
        static __forceinline const Vec3<float4> gather(const float* const grid, const size_t line_offset)
        {
          const float4 r0 = loadu4f(grid + 0*line_offset); 
          const float4 r1 = loadu4f(grid + 1*line_offset); // FIXME: this accesses 1 element too much
          return Vec3<float4>(unpacklo(r0,r1),       // r00, r10, r01, r11  
                              shuffle<1,1,2,2>(r0),  // r01, r01, r02, r02
                              shuffle<0,1,1,2>(r1)); // r10, r11, r11, r12
        }
      };
      
      static __forceinline Vec2<float4> decodeUV(const float4& uv)
      {
	const int4 iu  = cast(uv) & 0xffff;
	const int4 iv  = srl(cast(uv),16);
	const float4 u = (float4)iu * float4(1.0f/0xFFFF);
	const float4 v = (float4)iv * float4(1.0f/0xFFFF);
	return Vec2<float4>(u,v);
      }
      
#if defined (__AVX__)
      
      struct Gather3x3
      {
        typedef bool8 vbool;
        typedef int8 vint;
        typedef float8 vfloat;
        
        static __forceinline const Vec3<float8> gather(const float* const grid, const size_t line_offset)
        {
          const float4 ra = loadu4f(grid + 0*line_offset);
          const float4 rb = loadu4f(grid + 1*line_offset);
          const float4 rc = loadu4f(grid + 2*line_offset); // FIXME: this accesses 1 element too much
          const float8 r0 = float8(ra,rb);
          const float8 r1 = float8(rb,rc);
          return Vec3<float8>(unpacklo(r0,r1),         // r00, r10, r01, r11, r10, r20, r11, r21   
                              shuffle<1,1,2,2>(r0),    // r01, r01, r02, r02, r11, r11, r12, r12
                              shuffle<0,1,1,2>(r1));   // r10, r11, r11, r12, r20, r21, r21, r22
        }
      };
      
      static __forceinline Vec2<float8> decodeUV(const float8& uv)
      {
	const int8 i_uv = cast(uv);
	const int8 i_u  = i_uv & 0xffff;
	const int8 i_v  = srl(i_uv,16);
	const float8 u    = (float8)i_u * float8(1.0f/0xFFFF);
	const float8 v    = (float8)i_v * float8(1.0f/0xFFFF);
	return Vec2<float8>(u,v);
      }
      
#endif
      
      template<typename Loader>
        static __forceinline void intersect(Ray& ray,
                                            const float* const grid_x,
                                            const float* const grid_y,
                                            const float* const grid_z,
                                            const float* const grid_uv,
                                            const size_t line_offset,
                                            Precalculations& pre,
                                            Scene* scene)
      {
        typedef typename Loader::vbool vbool;
        typedef typename Loader::vfloat vfloat;
	const Vec3<vfloat> tri_v012_x = Loader::gather(grid_x,line_offset);
	const Vec3<vfloat> tri_v012_y = Loader::gather(grid_y,line_offset);
	const Vec3<vfloat> tri_v012_z = Loader::gather(grid_z,line_offset);
        
	const Vec3<vfloat> v0(tri_v012_x[0],tri_v012_y[0],tri_v012_z[0]);
	const Vec3<vfloat> v1(tri_v012_x[1],tri_v012_y[1],tri_v012_z[1]);
	const Vec3<vfloat> v2(tri_v012_x[2],tri_v012_y[2],tri_v012_z[2]);
        
        triangle_intersect_pluecker<vbool>(ray,v0,v1,v2,pre.patch->geom,pre.patch->prim,scene,[&](vfloat& u, vfloat& v) {
            const Vec3<vfloat> tri_v012_uv = Loader::gather(grid_uv,line_offset);	
            const Vec2<vfloat> uv0 = decodeUV(tri_v012_uv[0]);
            const Vec2<vfloat> uv1 = decodeUV(tri_v012_uv[1]);
            const Vec2<vfloat> uv2 = decodeUV(tri_v012_uv[2]);        
            const Vec2<vfloat> uv = u * uv1 + v * uv2 + (1.0f-u-v) * uv0;        
            u = uv[0];v = uv[1]; 
          });
      };
      
      template<typename Loader>
        static __forceinline bool occluded(Ray& ray,
                                           const float* const grid_x,
                                           const float* const grid_y,
                                           const float* const grid_z,
                                           const float* const grid_uv,
                                           const size_t line_offset,
                                           Precalculations& pre,
                                           Scene* scene)
      {
        typedef typename Loader::vbool vbool;
        typedef typename Loader::vfloat vfloat;
	const Vec3<vfloat> tri_v012_x = Loader::gather(grid_x,line_offset);
	const Vec3<vfloat> tri_v012_y = Loader::gather(grid_y,line_offset);
	const Vec3<vfloat> tri_v012_z = Loader::gather(grid_z,line_offset);
        
	const Vec3<vfloat> v0(tri_v012_x[0],tri_v012_y[0],tri_v012_z[0]);
	const Vec3<vfloat> v1(tri_v012_x[1],tri_v012_y[1],tri_v012_z[1]);
	const Vec3<vfloat> v2(tri_v012_x[2],tri_v012_y[2],tri_v012_z[2]);
        
        return triangle_occluded_pluecker<vbool>(ray,v0,v1,v2,pre.patch->geom,pre.patch->prim,scene,[&](vfloat& u, vfloat& v) {
            const Vec3<vfloat> tri_v012_uv = Loader::gather(grid_uv,line_offset);	
            const Vec2<vfloat> uv0 = decodeUV(tri_v012_uv[0]);
            const Vec2<vfloat> uv1 = decodeUV(tri_v012_uv[1]);
            const Vec2<vfloat> uv2 = decodeUV(tri_v012_uv[2]);        
            const Vec2<vfloat> uv = u * uv1 + v * uv2 + (1.0f-u-v) * uv0;        
            u = uv[0];v = uv[1]; 
          });
      }
      
      /*! Intersect a ray with the primitive. */
      static __forceinline void intersect(Precalculations& pre, Ray& ray, const Primitive* prim, size_t ty, Scene* scene, size_t& lazy_node) 
      {
        const size_t dim_offset    = pre.patch->grid_size_simd_blocks * vfloat::size;
        const size_t line_offset   = pre.patch->grid_u_res;
        const size_t offset_bytes  = ((size_t)prim  - (size_t)SharedLazyTessellationCache::sharedLazyTessellationCache.getDataPtr()) >> 2;   
        const float* const grid_x  = (float*)(offset_bytes + (size_t)SharedLazyTessellationCache::sharedLazyTessellationCache.getDataPtr());
        const float* const grid_y  = grid_x + 1 * dim_offset;
        const float* const grid_z  = grid_x + 2 * dim_offset;
        const float* const grid_uv = grid_x + 3 * dim_offset;
        
#if defined(__AVX__)
        intersect<Gather3x3>( ray, grid_x,grid_y,grid_z,grid_uv, line_offset, pre, scene);
#else
        intersect<Gather2x3>(ray, grid_x            ,grid_y            ,grid_z            ,grid_uv            , line_offset, pre, scene);
        intersect<Gather2x3>(ray, grid_x+line_offset,grid_y+line_offset,grid_z+line_offset,grid_uv+line_offset, line_offset, pre, scene);
#endif
      }
      
      /*! Test if the ray is occluded by the primitive */
      static __forceinline bool occluded(Precalculations& pre, Ray& ray, const Primitive* prim, size_t ty, Scene* scene, size_t& lazy_node) 
      {
        const size_t dim_offset    = pre.patch->grid_size_simd_blocks * vfloat::size;
        const size_t line_offset   = pre.patch->grid_u_res;
        const size_t offset_bytes  = ((size_t)prim  - (size_t)SharedLazyTessellationCache::sharedLazyTessellationCache.getDataPtr()) >> 2;   
        const float* const grid_x  = (float*)(offset_bytes + (size_t)SharedLazyTessellationCache::sharedLazyTessellationCache.getDataPtr());
        const float* const grid_y  = grid_x + 1 * dim_offset;
        const float* const grid_z  = grid_x + 2 * dim_offset;
        const float* const grid_uv = grid_x + 3 * dim_offset;
        
#if defined(__AVX__)
        return occluded<Gather3x3>( ray, grid_x,grid_y,grid_z,grid_uv, line_offset, pre, scene);
#else
        if (occluded<Gather2x3>(ray, grid_x            ,grid_y            ,grid_z            ,grid_uv            , line_offset, pre, scene)) return true;
        if (occluded<Gather2x3>(ray, grid_x+line_offset,grid_y+line_offset,grid_z+line_offset,grid_uv+line_offset, line_offset, pre, scene)) return true;
#endif
        return false;
      }      
    };
  }
}