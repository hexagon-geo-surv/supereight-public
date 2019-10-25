/*

 Copyright (c) 2014 University of Edinburgh, Imperial College, University of Manchester.
 Developed in the PAMELA project, EPSRC Programme Grant EP/K008730/1

 This code is licensed under the MIT License.


 Copyright 2016 Emanuele Vespa, Imperial College London

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 3. Neither the name of the copyright holder nor the names of its contributors
 may be used to endorse or promote products derived from this software without
 specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sophus/se3.hpp>

#include <se/utils/math_utils.h>
#include <se/commons.h>
#include <lodepng.h>
#include <se/timings.h>
#include <se/continuous/volume_template.hpp>
#include <se/image/image.hpp>
#include <se/ray_iterator.hpp>

/* Raycasting implementations */
#include "bfusion/rendering_impl.hpp"
#include "kfusion/rendering_impl.hpp"



namespace se {
  namespace internal {
    se::Image<int> scale_image(640, 480);
    std::vector<Eigen::Vector3f, Eigen::aligned_allocator<Eigen::Vector3f>>
      color_map =
      {
        {102, 194, 165},
        {252, 141, 98},
        {141, 160, 203},
        {231, 138, 195},
        {166, 216, 84},
        {255, 217, 47},
        {229, 196, 148},
        {179, 179, 179},
      };
  }
}



template<typename T>
void raycastKernel(const Volume<T>&            volume,
                   se::Image<Eigen::Vector3f>& vertex,
                   se::Image<Eigen::Vector3f>& normal,
                   const Eigen::Matrix4f&      view,
                   const float                 nearPlane,
                   const float                 farPlane,
                   const float                 mu,
                   const float                 step,
                   const float                 largestep) {

  TICK();
  int y;
#pragma omp parallel for shared(normal, vertex), private(y)
  for (y = 0; y < vertex.height(); y++) {
#pragma omp simd
    for (int x = 0; x < vertex.width(); x++) {
      Eigen::Vector2i pos(x, y);
      const Eigen::Vector3f dir =
          (view.topLeftCorner<3, 3>() * Eigen::Vector3f(x, y, 1.f)).normalized();
      const Eigen::Vector3f transl = view.topRightCorner<3, 1>();
      se::ray_iterator<T> ray(*volume._map_index, transl, dir, nearPlane, farPlane);
      ray.next();
      const float t_min = ray.tcmin(); /* Get distance to the first intersected block */
      const Eigen::Vector4f hit = t_min > 0.f
          ? raycast(volume, transl, dir, t_min, ray.tmax(), mu, step, largestep)
          : Eigen::Vector4f::Constant(0.f);
      if (hit.w() >= 0.f) {
        vertex[x + y * vertex.width()] = hit.head<3>();
        Eigen::Vector3f surfNorm = volume.grad(hit.head<3>(),
            int(hit.w() + 0.5f),
            [](const auto& val){ return val.x; });
        se::internal::scale_image(x, y) = static_cast<int>(hit.w());
        if (surfNorm.norm() == 0.f) {
          //normal[pos] = normalize(surfNorm); // APN added
          normal[pos.x() + pos.y() * normal.width()] = Eigen::Vector3f(INVALID, 0.f, 0.f);
        } else {
          // Invert normals if SDF
          normal[pos.x() + pos.y() * normal.width()] =
              (std::is_same<T, SDF>::value || std::is_same<T, MultiresSDF>::value)
              ? (-1.f * surfNorm).normalized()
              : surfNorm.normalized();
        }
      } else {
        vertex[pos.x() + pos.y() * vertex.width()] = Eigen::Vector3f::Constant(0.f);
        normal[pos.x() + pos.y() * normal.width()] = Eigen::Vector3f(INVALID, 0.f, 0.f);
      }
    }
  }
  TOCK("raycastKernel", vertex.width() * vertex.height());
}



void renderDepthKernel(unsigned char*         out,
                       float*                 depth,
                       const Eigen::Vector2i& depthSize,
                       const float            nearPlane,
                       const float            farPlane) {

  TICK();

  float rangeScale = 1.f / (farPlane - nearPlane);

  int y;
#pragma omp parallel for shared(out), private(y)
  for (y = 0; y < depthSize.y(); y++) {
    int rowOffeset = y * depthSize.x();
    for (int x = 0; x < depthSize.x(); x++) {

      unsigned int pos = rowOffeset + x;
      unsigned int idx = pos * 4;

      if (depth[pos] < nearPlane) {
        out[idx + 0] = 255;
        out[idx + 1] = 255;
        out[idx + 2] = 255;
        out[idx + 3] = 0;
      } else if (depth[pos] > farPlane) {
        out[idx + 0] = 0;
        out[idx + 1] = 0;
        out[idx + 2] = 0;
        out[idx + 3] = 0;
      } else {
        const float d = (depth[pos] - nearPlane) * rangeScale;
        unsigned char rgbw[4];
        gs2rgb(d, rgbw);
        out[idx + 0] = rgbw[0];
        out[idx + 1] = rgbw[1];
        out[idx + 2] = rgbw[2];
        out[idx + 3] = rgbw[3];
      }
    }
  }
  TOCK("renderDepthKernel", depthSize.x() * depthSize.y());
}



void renderTrackKernel(unsigned char*         out,
                       const TrackData*       data,
                       const Eigen::Vector2i& outSize) {

  TICK();

  int y;
#pragma omp parallel for shared(out), private(y)
  for (y = 0; y < outSize.y(); y++)
    for (int x = 0; x < outSize.x(); x++) {
      const int pos = x + outSize.x() * y;
      const int idx = pos * 4;
      switch (data[pos].result) {
        case 1:
          out[idx + 0] = 128;
          out[idx + 1] = 128;
          out[idx + 2] = 128;
          out[idx + 3] = 0;
          break;
        case -1:
          out[idx + 0] = 0;
          out[idx + 1] = 0;
          out[idx + 2] = 0;
          out[idx + 3] = 0;
          break;
        case -2:
          out[idx + 0] = 255;
          out[idx + 1] = 0;
          out[idx + 2] = 0;
          out[idx + 3] = 0;
          break;
        case -3:
          out[idx + 0] = 0;
          out[idx + 1] = 255;
          out[idx + 2] = 0;
          out[idx + 3] = 0;
          break;
        case -4:
          out[idx + 0] = 0;
          out[idx + 1] = 0;
          out[idx + 2] = 255;
          out[idx + 3] = 0;
          break;
        case -5:
          out[idx + 0] = 255;
          out[idx + 1] = 255;
          out[idx + 2] = 0;
          out[idx + 3] = 0;
          break;
        default:
          out[idx + 0] = 255;
          out[idx + 1] = 128;
          out[idx + 2] = 128;
          out[idx + 3] = 0;
          break;
      }
    }
  TOCK("renderTrackKernel", outSize.x() * outSize.y());
}



template <typename T>
void renderVolumeKernel(const Volume<T>&                  volume,
                        unsigned char*                    out, // RGBW packed
                        const Eigen::Vector2i&            depthSize,
                        const Eigen::Matrix4f&            view,
                        const float                       nearPlane,
                        const float                       farPlane,
                        const float                       mu,
                        const float                       step,
                        const float                       largestep,
                        const Eigen::Vector3f&            light,
                        const Eigen::Vector3f&            ambient,
                        bool                              render,
                        const se::Image<Eigen::Vector3f>& vertex,
                        const se::Image<Eigen::Vector3f>& normal) {

  TICK();
  int y;
#pragma omp parallel for shared(out), private(y)
  for (y = 0; y < depthSize.y(); y++) {
    for (int x = 0; x < depthSize.x(); x++) {
      Eigen::Vector4f hit;
      Eigen::Vector3f test, surfNorm;
      const int idx = (x + depthSize.x()*y) * 4;

      if (render) {
        const Eigen::Vector3f dir =
            (view.topLeftCorner<3, 3>() * Eigen::Vector3f(x, y, 1.f)).normalized();
        const Eigen::Vector3f transl = view.topRightCorner<3, 1>();
        se::ray_iterator<T> ray(*volume._map_index, transl, dir, nearPlane, farPlane);
        ray.next();
        const float t_min = ray.tmin(); /* Get distance to the first intersected block */
        hit = t_min > 0.f
            ? raycast(volume, transl, dir, t_min, ray.tmax(), mu, step, largestep)
            : Eigen::Vector4f::Constant(0.f);
        if (hit.w() >= 0.f) {
          test = hit.head<3>();
          surfNorm = volume.grad(test, [](const auto& val){ return val.x; });

          // Invert normals if SDF
          surfNorm = (std::is_same<T, SDF>::value || std::is_same<T, MultiresSDF>::value)
              ? -1.f * surfNorm
              : surfNorm;
        } else {
          surfNorm = Eigen::Vector3f(INVALID, 0.f, 0.f);
        }
      } else {
        test = vertex[x + depthSize.x() * y];
        surfNorm = normal[x + depthSize.x() * y];
      }

      if (surfNorm.x() != INVALID && surfNorm.norm() > 0.f) {
        const Eigen::Vector3f diff = (test - light).normalized();
        const Eigen::Vector3f dir
            = Eigen::Vector3f::Constant(fmaxf(surfNorm.normalized().dot(diff), 0.f));
        Eigen::Vector3f col = dir + ambient;
        se::math::clamp(col, Eigen::Vector3f::Constant(0.f), Eigen::Vector3f::Constant(1.f));
        col = col.cwiseProduct(se::internal::color_map[se::internal::scale_image(x, y)]);
        out[idx + 0] = col.x();
        out[idx + 1] = col.y();
        out[idx + 2] = col.z();
        out[idx + 3] = 255;
      } else {
        out[idx + 0] = 0;
        out[idx + 1] = 0;
        out[idx + 2] = 0;
        out[idx + 3] = 255;
      }
    }
  }
  TOCK("renderVolumeKernel", depthSize.x() * depthSize.y());
}



static inline void printNormals(const se::Image<Eigen::Vector3f>& in,
                                const unsigned int                xdim,
                                const unsigned int                ydim,
                                const char*                       filename) {

  unsigned char* image = new unsigned char [xdim * ydim * 4];
  for (unsigned int y = 0; y < ydim; ++y) {
    for (unsigned int x = 0; x < xdim; ++x){
      const Eigen::Vector3f n = in[x + y * xdim];
      image[4 * xdim * y + 4 * x + 0] = (n.x() / 2 + 0.5) * 255;
      image[4 * xdim * y + 4 * x + 1] = (n.y() / 2 + 0.5) * 255;
      image[4 * xdim * y + 4 * x + 2] = (n.z() / 2 + 0.5) * 255;
      image[4 * xdim * y + 4 * x + 3] = 255;
    }
  }
  lodepng_encode32_file(std::string(filename).append(".png").c_str(),
      image, xdim, ydim);
}



// Find ALL the intersection along a ray till the farPlane.
template <typename T>
void raycast_full(
    const Volume<T>&                                                         volume,
    std::vector<Eigen::Vector4f, Eigen::aligned_allocator<Eigen::Vector4f>>& points,
    const Eigen::Vector3f&                                                   origin,
    const Eigen::Vector3f&                                                   direction,
    const float                                                              farPlane,
    const float                                                              step,
    const float                                                              largestep) {

  float t = 0;
  float stepsize = largestep;
  float f_t = volume.interp(origin + direction * t, [](const auto& val){ return val.x;}).first;
  t += step;
  float f_tt = 1.f;


  for (; t < farPlane; t += stepsize) {
    f_tt = volume.interp(origin + direction * t, [](const auto& val){ return val.x;}).first;
    if (f_tt < 0.f && f_t > 0.f && std::abs(f_tt - f_t) < 0.5f) {     // got it, jump out of inner loop
      auto data_t  = volume.get(origin + direction * (t - stepsize));
      auto data_tt = volume.get(origin + direction * t);
      if (f_t == 1.0 || f_tt == 1.0 || data_t.y == 0 || data_tt.y == 0 ) {
        f_t = f_tt;
        continue;
      }
      t = t + stepsize * f_tt / (f_t - f_tt);
      points.push_back((origin + direction * t).homogeneous());
    }
    if (f_tt < std::abs(0.8f)) {
      // coming closer, reduce stepsize
      stepsize = step;
    }
    f_t = f_tt;
  }
}

