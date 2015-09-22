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
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "../../common/geometry.h"

#include "../../common/ray.h"

#if defined(__SSE__)
#include "../../common/ray4.h"
#endif

#if defined(__AVX__)
#include "../../common/ray8.h"
#endif

#if defined(__AVX512F__)
#include "../../common/ray16.h"
#endif

namespace embree
{
  namespace isa
  {
#if defined(__SSE__)
    typedef void (*ISPCFilterFunc4)(void* ptr, RTCRay4& ray, __m128 valid);
#endif
    
#if defined(__AVX__)
  typedef void (*ISPCFilterFunc8)(void* ptr, RTCRay8& ray, __m256 valid);
#endif

#if defined(__AVX512F__)
  typedef void (*ISPCFilterFunc16)(void* ptr, RTCRay16& ray, __mmask16 valid);
#endif

    __forceinline bool runIntersectionFilter1(const Geometry* const geometry, Ray& ray, 
                                              const float& u, const float& v, const float& t, const Vec3fa& Ng, const int geomID, const int primID)
    {
      /* temporarily update hit information */
      const float  ray_tfar = ray.tfar;
      const Vec3fa ray_Ng   = ray.Ng;
      const vfloat4   ray_uv_ids = *(vfloat4*)&ray.u;
      ray.u = u;
      ray.v = v;
      ray.tfar = t;
      ray.geomID = geomID;
      ray.primID = primID;
      ray.Ng = Ng;
      
      /* invoke filter function */
      AVX_ZERO_UPPER();
      geometry->intersectionFilter1(geometry->userPtr,(RTCRay&)ray);
      
      /* restore hit if filter not passed */
      if (unlikely(ray.geomID == -1)) 
      {
        ray.tfar = ray_tfar;
        ray.Ng = ray_Ng;
        *(vfloat4*)&ray.u = ray_uv_ids;
        return false;
      }
      return true;
    }
    
    __forceinline bool runOcclusionFilter1(const Geometry* const geometry, Ray& ray, 
                                           const float& u, const float& v, const float& t, const Vec3fa& Ng, const int geomID, const int primID)
    {
      /* temporarily update hit information */
      const float ray_tfar = ray.tfar;
      const int   ray_geomID = ray.geomID;
      ray.u = u;
      ray.v = v;
      ray.tfar = t;
      ray.geomID = geomID;
      ray.primID = primID;
      ray.Ng = Ng;
      
      /* invoke filter function */
      AVX_ZERO_UPPER();
      geometry->occlusionFilter1(geometry->userPtr,(RTCRay&)ray);
      
      /* restore hit if filter not passed */
      if (unlikely(ray.geomID == -1)) 
      {
        ray.tfar = ray_tfar;
        ray.geomID = ray_geomID;
        return false;
      }
      return true;
    }

    __forceinline vbool4 runIntersectionFilter(const vbool4& valid, const Geometry* const geometry, Ray4& ray, 
                                             const vfloat4& u, const vfloat4& v, const vfloat4& t, const Vec3vf4& Ng, const int geomID, const int primID)
    {
      /* temporarily update hit information */
      const vfloat4 ray_u = ray.u;           store4f(valid,&ray.u,u);
      const vfloat4 ray_v = ray.v;           store4f(valid,&ray.v,v);
      const vfloat4 ray_tfar = ray.tfar;     store4f(valid,&ray.tfar,t);
      const vint4 ray_geomID = ray.geomID; store4i(valid,&ray.geomID,geomID);
      const vint4 ray_primID = ray.primID; store4i(valid,&ray.primID,primID);
      const vfloat4 ray_Ng_x = ray.Ng.x;     store4f(valid,&ray.Ng.x,Ng.x);
      const vfloat4 ray_Ng_y = ray.Ng.y;     store4f(valid,&ray.Ng.y,Ng.y);
      const vfloat4 ray_Ng_z = ray.Ng.z;     store4f(valid,&ray.Ng.z,Ng.z);
      
      /* invoke filter function */
      RTCFilterFunc4  filter4 = geometry->intersectionFilter4;
      AVX_ZERO_UPPER();
      if (geometry->ispcIntersectionFilter4) ((ISPCFilterFunc4)filter4)(geometry->userPtr,(RTCRay4&)ray,valid);
      else { const vbool4 valid_temp = valid; filter4(&valid_temp,geometry->userPtr,(RTCRay4&)ray); }
      const vbool4 valid_failed = valid & (ray.geomID == vint4(-1));
      const vbool4 valid_passed = valid & (ray.geomID != vint4(-1));
      
      /* restore hit if filter not passed */
      if (unlikely(any(valid_failed))) 
      {
        store4f(valid_failed,&ray.u,ray_u);
        store4f(valid_failed,&ray.v,ray_v);
        store4f(valid_failed,&ray.tfar,ray_tfar);
        store4i(valid_failed,&ray.geomID,ray_geomID);
        store4i(valid_failed,&ray.primID,ray_primID);
        store4f(valid_failed,&ray.Ng.x,ray_Ng_x);
        store4f(valid_failed,&ray.Ng.y,ray_Ng_y);
        store4f(valid_failed,&ray.Ng.z,ray_Ng_z);
      }
      return valid_passed;
    }
    
    __forceinline vbool4 runOcclusionFilter(const vbool4& valid, const Geometry* const geometry, Ray4& ray, 
                                          const vfloat4& u, const vfloat4& v, const vfloat4& t, const Vec3vf4& Ng, const int geomID, const int primID)
    {
      /* temporarily update hit information */
      const vfloat4 ray_tfar = ray.tfar; 
      const vint4 ray_geomID = ray.geomID;
      store4f(valid,&ray.u,u);
      store4f(valid,&ray.v,v);
      store4f(valid,&ray.tfar,t);
      store4i(valid,&ray.geomID,geomID);
      store4i(valid,&ray.primID,primID);
      store4f(valid,&ray.Ng.x,Ng.x);
      store4f(valid,&ray.Ng.y,Ng.y);
      store4f(valid,&ray.Ng.z,Ng.z);
      
      /* invoke filter function */
      RTCFilterFunc4 filter4 = geometry->occlusionFilter4;
      AVX_ZERO_UPPER();
      if (geometry->ispcOcclusionFilter4) ((ISPCFilterFunc4)filter4)(geometry->userPtr,(RTCRay4&)ray,valid);
      else { const vbool4 valid_temp = valid; filter4(&valid_temp,geometry->userPtr,(RTCRay4&)ray); }
      const vbool4 valid_failed = valid & (ray.geomID == vint4(-1));
      const vbool4 valid_passed = valid & (ray.geomID != vint4(-1));
      
      /* restore hit if filter not passed */
      store4f(valid_failed,&ray.tfar,ray_tfar);
      store4i(valid_failed,&ray.geomID,ray_geomID);
      return valid_passed;
    }
    
    __forceinline bool runIntersectionFilter(const Geometry* const geometry, Ray4& ray, const size_t k,
                                             const float& u, const float& v, const float& t, const Vec3fa& Ng, const int geomID, const int primID)
    {
      /* temporarily update hit information */
      const vfloat4 ray_u = ray.u;           ray.u[k] = u;
      const vfloat4 ray_v = ray.v;           ray.v[k] = v;
      const vfloat4 ray_tfar = ray.tfar;     ray.tfar[k] = t;
      const vint4 ray_geomID = ray.geomID; ray.geomID[k] = geomID;
      const vint4 ray_primID = ray.primID; ray.primID[k] = primID;
      const vfloat4 ray_Ng_x = ray.Ng.x;     ray.Ng.x[k] = Ng.x;
      const vfloat4 ray_Ng_y = ray.Ng.y;     ray.Ng.y[k] = Ng.y;
      const vfloat4 ray_Ng_z = ray.Ng.z;     ray.Ng.z[k] = Ng.z;
      
      /* invoke filter function */
      const vbool4 valid(1 << k);
      RTCFilterFunc4  filter4 = geometry->intersectionFilter4;
      AVX_ZERO_UPPER();
      if (geometry->ispcIntersectionFilter4) ((ISPCFilterFunc4)filter4)(geometry->userPtr,(RTCRay4&)ray,valid);
      else { const vbool4 valid_temp = valid; filter4(&valid_temp,geometry->userPtr,(RTCRay4&)ray); }
      const bool passed = ray.geomID[k] != -1;
      
      /* restore hit if filter not passed */
      if (unlikely(!passed)) {
        store4f(&ray.u,ray_u);
        store4f(&ray.v,ray_v);
        store4f(&ray.tfar,ray_tfar);
        store4i(&ray.geomID,ray_geomID);
        store4i(&ray.primID,ray_primID);
        store4f(&ray.Ng.x,ray_Ng_x);
        store4f(&ray.Ng.y,ray_Ng_y);
        store4f(&ray.Ng.z,ray_Ng_z);
      }
      return passed;
    }
    
    __forceinline bool runOcclusionFilter(const Geometry* const geometry, Ray4& ray, const size_t k,
                                          const float& u, const float& v, const float& t, const Vec3fa& Ng, const int geomID, const int primID)
    {
      /* temporarily update hit information */
      const vfloat4 ray_tfar = ray.tfar; 
      const vint4 ray_geomID = ray.geomID;
      ray.u[k] = u;
      ray.v[k] = v;
      ray.tfar[k] = t;
      ray.geomID[k] = geomID;
      ray.primID[k] = primID;
      ray.Ng.x[k] = Ng.x;
      ray.Ng.y[k] = Ng.y;
      ray.Ng.z[k] = Ng.z;
      
      /* invoke filter function */
      const vbool4 valid(1 << k);
      RTCFilterFunc4  filter4 = geometry->occlusionFilter4;
      AVX_ZERO_UPPER();
      if (geometry->ispcOcclusionFilter4) ((ISPCFilterFunc4)filter4)(geometry->userPtr,(RTCRay4&)ray,valid);
      else { const vbool4 valid_temp = valid; filter4(&valid_temp,geometry->userPtr,(RTCRay4&)ray); }
      const bool passed = ray.geomID[k] != -1;
      
      /* restore hit if filter not passed */
      if (unlikely(!passed)) {
        store4f(&ray.tfar,ray_tfar);
        store4i(&ray.geomID,ray_geomID);
      }
      return passed;
    }
    
#if defined(__AVX__)
    __forceinline vbool8 runIntersectionFilter(const vbool8& valid, const Geometry* const geometry, Ray8& ray, 
                                             const vfloat8& u, const vfloat8& v, const vfloat8& t, const Vec3vf8& Ng, const int geomID, const int primID)
    {
      /* temporarily update hit information */
      const vfloat8 ray_u = ray.u;           store8f(valid,&ray.u,u);
      const vfloat8 ray_v = ray.v;           store8f(valid,&ray.v,v);
      const vfloat8 ray_tfar = ray.tfar;     store8f(valid,&ray.tfar,t);
      const vint8 ray_geomID = ray.geomID; store8i(valid,&ray.geomID,geomID);
      const vint8 ray_primID = ray.primID; store8i(valid,&ray.primID,primID);
      const vfloat8 ray_Ng_x = ray.Ng.x;     store8f(valid,&ray.Ng.x,Ng.x);
      const vfloat8 ray_Ng_y = ray.Ng.y;     store8f(valid,&ray.Ng.y,Ng.y);
      const vfloat8 ray_Ng_z = ray.Ng.z;     store8f(valid,&ray.Ng.z,Ng.z);
      
      /* invoke filter function */
      RTCFilterFunc8  filter8 = geometry->intersectionFilter8;
      if (geometry->ispcIntersectionFilter8) ((ISPCFilterFunc8)filter8)(geometry->userPtr,(RTCRay8&)ray,valid);
      else { const vbool8 valid_temp = valid; filter8(&valid_temp,geometry->userPtr,(RTCRay8&)ray); }
      const vbool8 valid_failed = valid & (ray.geomID == vint8(-1));
      const vbool8 valid_passed = valid & (ray.geomID != vint8(-1));
      
      /* restore hit if filter not passed */
      if (unlikely(any(valid_failed))) 
      {
        store8f(valid_failed,&ray.u,ray_u);
        store8f(valid_failed,&ray.v,ray_v);
        store8f(valid_failed,&ray.tfar,ray_tfar);
        store8i(valid_failed,&ray.geomID,ray_geomID);
        store8i(valid_failed,&ray.primID,ray_primID);
        store8f(valid_failed,&ray.Ng.x,ray_Ng_x);
        store8f(valid_failed,&ray.Ng.y,ray_Ng_y);
        store8f(valid_failed,&ray.Ng.z,ray_Ng_z);
      }
      return valid_passed;
    }
    
    __forceinline vbool8 runOcclusionFilter(const vbool8& valid, const Geometry* const geometry, Ray8& ray, 
                                          const vfloat8& u, const vfloat8& v, const vfloat8& t, const Vec3vf8& Ng, const int geomID, const int primID)
    {
      /* temporarily update hit information */
      const vfloat8 ray_tfar = ray.tfar; 
      const vint8 ray_geomID = ray.geomID;
      store8f(valid,&ray.u,u);
      store8f(valid,&ray.v,v);
      store8f(valid,&ray.tfar,t);
      store8i(valid,&ray.geomID,geomID);
      store8i(valid,&ray.primID,primID);
      store8f(valid,&ray.Ng.x,Ng.x);
      store8f(valid,&ray.Ng.y,Ng.y);
      store8f(valid,&ray.Ng.z,Ng.z);
      
      /* invoke filter function */
      RTCFilterFunc8 filter8 = geometry->occlusionFilter8;
      if (geometry->ispcOcclusionFilter8) ((ISPCFilterFunc8)filter8)(geometry->userPtr,(RTCRay8&)ray,valid);
      else { const vbool8 valid_temp = valid; filter8(&valid_temp,geometry->userPtr,(RTCRay8&)ray); }
      const vbool8 valid_failed = valid & (ray.geomID == vint8(-1));
      const vbool8 valid_passed = valid & (ray.geomID != vint8(-1));
      
      /* restore hit if filter not passed */
      store8f(valid_failed,&ray.tfar,ray_tfar);
      store8i(valid_failed,&ray.geomID,ray_geomID);
      return valid_passed;
    }
    
    __forceinline bool runIntersectionFilter(const Geometry* const geometry, Ray8& ray, const size_t k,
                                             const float& u, const float& v, const float& t, const Vec3fa& Ng, const int geomID, const int primID)
    {
      /* temporarily update hit information */
      const vfloat8 ray_u = ray.u;           ray.u[k] = u;
      const vfloat8 ray_v = ray.v;           ray.v[k] = v;
      const vfloat8 ray_tfar = ray.tfar;     ray.tfar[k] = t;
      const vint8 ray_geomID = ray.geomID; ray.geomID[k] = geomID;
      const vint8 ray_primID = ray.primID; ray.primID[k] = primID;
      const vfloat8 ray_Ng_x = ray.Ng.x;     ray.Ng.x[k] = Ng.x;
      const vfloat8 ray_Ng_y = ray.Ng.y;     ray.Ng.y[k] = Ng.y;
      const vfloat8 ray_Ng_z = ray.Ng.z;     ray.Ng.z[k] = Ng.z;
      
      /* invoke filter function */
      const vbool8 valid(1 << k);
      RTCFilterFunc8  filter8 = geometry->intersectionFilter8;
      if (geometry->ispcIntersectionFilter8) ((ISPCFilterFunc8)filter8)(geometry->userPtr,(RTCRay8&)ray,valid);
      else filter8(&valid,geometry->userPtr,(RTCRay8&)ray);
      const bool passed = ray.geomID[k] != -1;
      
      /* restore hit if filter not passed */
      if (unlikely(!passed)) {
        store8f(&ray.u,ray_u);
        store8f(&ray.v,ray_v);
        store8f(&ray.tfar,ray_tfar);
        store8i(&ray.geomID,ray_geomID);
        store8i(&ray.primID,ray_primID);
        store8f(&ray.Ng.x,ray_Ng_x);
        store8f(&ray.Ng.y,ray_Ng_y);
        store8f(&ray.Ng.z,ray_Ng_z);
      }
      return passed;
    }
    
    __forceinline bool runOcclusionFilter(const Geometry* const geometry, Ray8& ray, const size_t k,
                                          const float& u, const float& v, const float& t, const Vec3fa& Ng, const int geomID, const int primID)
    {
      /* temporarily update hit information */
      const vfloat8 ray_tfar = ray.tfar; 
      const vint8 ray_geomID = ray.geomID;
      ray.u[k] = u;
      ray.v[k] = v;
      ray.tfar[k] = t;
      ray.geomID[k] = geomID;
      ray.primID[k] = primID;
      ray.Ng.x[k] = Ng.x;
      ray.Ng.y[k] = Ng.y;
      ray.Ng.z[k] = Ng.z;
      
      /* invoke filter function */
      const vbool8 valid(1 << k);
      RTCFilterFunc8 filter8 = geometry->occlusionFilter8;
      if (geometry->ispcOcclusionFilter8) ((ISPCFilterFunc8)filter8)(geometry->userPtr,(RTCRay8&)ray,valid);
      else filter8(&valid,geometry->userPtr,(RTCRay8&)ray);
      const bool passed = ray.geomID[k] != -1;
      
      /* restore hit if filter not passed */
      if (unlikely(!passed)) {
        store8f(&ray.tfar,ray_tfar);
        store8i(&ray.geomID,ray_geomID);
      }
      return passed;
    }
    
#endif


#if defined(__AVX512F__)
    __forceinline vbool16 runIntersectionFilter(const vbool16& valid, const Geometry* const geometry, Ray16& ray, 
                                             const vfloat16& u, const vfloat16& v, const vfloat16& t, const Vec3vf16& Ng, const int geomID, const int primID)
    {
      /* temporarily update hit information */
      const vfloat16 ray_u = ray.u;           store16f(valid,&ray.u,u);
      const vfloat16 ray_v = ray.v;           store16f(valid,&ray.v,v);
      const vfloat16 ray_tfar = ray.tfar;     store16f(valid,&ray.tfar,t);
      const vint16 ray_geomID = ray.geomID; store16i(valid,&ray.geomID,geomID);
      const vint16 ray_primID = ray.primID; store16i(valid,&ray.primID,primID);
      const vfloat16 ray_Ng_x = ray.Ng.x;     store16f(valid,&ray.Ng.x,Ng.x);
      const vfloat16 ray_Ng_y = ray.Ng.y;     store16f(valid,&ray.Ng.y,Ng.y);
      const vfloat16 ray_Ng_z = ray.Ng.z;     store16f(valid,&ray.Ng.z,Ng.z);
      
      /* invoke filter function */
      RTCFilterFunc16  filter16 = geometry->intersectionFilter16;
      if (geometry->ispcIntersectionFilter16) ((ISPCFilterFunc16)filter16)(geometry->userPtr,(RTCRay16&)ray,valid);
      else { const vbool16 valid_temp = valid; filter16(&valid_temp,geometry->userPtr,(RTCRay16&)ray); }
      const vbool16 valid_failed = valid & (ray.geomID == vint16(-1));
      const vbool16 valid_passed = valid & (ray.geomID != vint16(-1));
      
      /* restore hit if filter not passed */
      if (unlikely(any(valid_failed))) 
      {
        store16f(valid_failed,&ray.u,ray_u);
        store16f(valid_failed,&ray.v,ray_v);
        store16f(valid_failed,&ray.tfar,ray_tfar);
        store16i(valid_failed,&ray.geomID,ray_geomID);
        store16i(valid_failed,&ray.primID,ray_primID);
        store16f(valid_failed,&ray.Ng.x,ray_Ng_x);
        store16f(valid_failed,&ray.Ng.y,ray_Ng_y);
        store16f(valid_failed,&ray.Ng.z,ray_Ng_z);
      }
      return valid_passed;
    }
    
    __forceinline vbool16 runOcclusionFilter(const vbool16& valid, const Geometry* const geometry, Ray16& ray, 
                                          const vfloat16& u, const vfloat16& v, const vfloat16& t, const Vec3vf16& Ng, const int geomID, const int primID)
    {
      /* temporarily update hit information */
      const vfloat16 ray_tfar = ray.tfar; 
      const vint16 ray_geomID = ray.geomID;
      store16f(valid,&ray.u,u);
      store16f(valid,&ray.v,v);
      store16f(valid,&ray.tfar,t);
      store16i(valid,&ray.geomID,geomID);
      store16i(valid,&ray.primID,primID);
      store16f(valid,&ray.Ng.x,Ng.x);
      store16f(valid,&ray.Ng.y,Ng.y);
      store16f(valid,&ray.Ng.z,Ng.z);
      
      /* invoke filter function */
      RTCFilterFunc16 filter16 = geometry->occlusionFilter16;
      if (geometry->ispcOcclusionFilter16) ((ISPCFilterFunc16)filter16)(geometry->userPtr,(RTCRay16&)ray,valid);
      else { const vbool16 valid_temp = valid; filter16(&valid_temp,geometry->userPtr,(RTCRay16&)ray); }
      const vbool16 valid_failed = valid & (ray.geomID == vint16(-1));
      const vbool16 valid_passed = valid & (ray.geomID != vint16(-1));
      
      /* restore hit if filter not passed */
      store16f(valid_failed,&ray.tfar,ray_tfar);
      store16i(valid_failed,&ray.geomID,ray_geomID);
      return valid_passed;
    }
    
    __forceinline bool runIntersectionFilter(const Geometry* const geometry, Ray16& ray, const size_t k,
                                             const float& u, const float& v, const float& t, const Vec3fa& Ng, const int geomID, const int primID)
    {
      /* temporarily update hit information */
      const vfloat16 ray_u = ray.u;           ray.u[k] = u;
      const vfloat16 ray_v = ray.v;           ray.v[k] = v;
      const vfloat16 ray_tfar = ray.tfar;     ray.tfar[k] = t;
      const vint16 ray_geomID = ray.geomID; ray.geomID[k] = geomID;
      const vint16 ray_primID = ray.primID; ray.primID[k] = primID;
      const vfloat16 ray_Ng_x = ray.Ng.x;     ray.Ng.x[k] = Ng.x;
      const vfloat16 ray_Ng_y = ray.Ng.y;     ray.Ng.y[k] = Ng.y;
      const vfloat16 ray_Ng_z = ray.Ng.z;     ray.Ng.z[k] = Ng.z;
      
      /* invoke filter function */
      const vbool16 valid(1 << k);
      RTCFilterFunc16  filter16 = geometry->intersectionFilter16;
      if (geometry->ispcIntersectionFilter16) ((ISPCFilterFunc16)filter16)(geometry->userPtr,(RTCRay16&)ray,valid);
      else filter16(&valid,geometry->userPtr,(RTCRay16&)ray);
      const bool passed = ray.geomID[k] != -1;
      
      /* restore hit if filter not passed */
      if (unlikely(!passed)) {
        store16f(&ray.u,ray_u);
        store16f(&ray.v,ray_v);
        store16f(&ray.tfar,ray_tfar);
        store16i(&ray.geomID,ray_geomID);
        store16i(&ray.primID,ray_primID);
        store16f(&ray.Ng.x,ray_Ng_x);
        store16f(&ray.Ng.y,ray_Ng_y);
        store16f(&ray.Ng.z,ray_Ng_z);
      }
      return passed;
    }
    
    __forceinline bool runOcclusionFilter(const Geometry* const geometry, Ray16& ray, const size_t k,
                                          const float& u, const float& v, const float& t, const Vec3fa& Ng, const int geomID, const int primID)
    {
      /* temporarily update hit information */
      const vfloat16 ray_tfar = ray.tfar; 
      const vint16 ray_geomID = ray.geomID;
      ray.u[k] = u;
      ray.v[k] = v;
      ray.tfar[k] = t;
      ray.geomID[k] = geomID;
      ray.primID[k] = primID;
      ray.Ng.x[k] = Ng.x;
      ray.Ng.y[k] = Ng.y;
      ray.Ng.z[k] = Ng.z;
      
      /* invoke filter function */
      const vbool16 valid(1 << k);
      RTCFilterFunc16 filter16 = geometry->occlusionFilter16;
      if (geometry->ispcOcclusionFilter16) ((ISPCFilterFunc16)filter16)(geometry->userPtr,(RTCRay16&)ray,valid);
      else filter16(&valid,geometry->userPtr,(RTCRay16&)ray);
      const bool passed = ray.geomID[k] != -1;
      
      /* restore hit if filter not passed */
      if (unlikely(!passed)) {
        store16f(&ray.tfar,ray_tfar);
        store16i(&ray.geomID,ray_geomID);
      }
      return passed;
    }
    
#endif

  }
}
