/*

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
#ifndef SE_MATH_HELPER_H
#define SE_MATH_HELPER_H
#include <iostream>
#include <cmath>
#include <Eigen/Dense>

/*
 * When compiling in debug mode Eigen compilation fails
 * due to -Wunused-parameter. Disable it if compiling with GCC.
 */
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#if __GNUC__ > 6
#pragma GCC diagnostic ignored "-Wint-in-bool-context"
#endif
#include <Eigen/Dense>
#pragma GCC diagnostic pop
#else
#include <Eigen/Dense>
#endif



// Defining M_PI is a compiler extension, we should not rely on it.
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif



namespace se {
  namespace math {
    template <typename T>
    static inline T fracf(const T& v) {
        return v - v.array().floor().matrix();
      }

    template <typename T>
    static inline T floorf(const T& v) {
      return v.array().floor();
    }

    template <typename T>
    static inline T fabs(const T& v) {
      return v.cwiseAbs();
    }

    template <typename Scalar>
    static constexpr inline Scalar sq(Scalar a) {
      return a * a;
    }

    template <typename Scalar>
    static constexpr inline Scalar cu(Scalar a) {
      return a * a * a;
    }

    template <typename Scalar>
    static inline bool in(const Scalar v,
                          const Scalar a,
                          const Scalar b) {
      return v >= a && v <= b;
    }

    constexpr int log2_const(int n){
      return (n < 2 ? 0 : 1 + log2_const(n/2));
    }

    static inline Eigen::Vector3f to_translation(const Eigen::Matrix4f& T) {
      Eigen::Vector3f t = T.block<3,1>(0,3);
      return t;
    }

    static inline Eigen::Matrix3f to_rotation(const Eigen::Matrix4f& T) {
      Eigen::Matrix3f R = T.block<3,3>(0,0);
      return R;
    }

    static inline Eigen::Matrix4f to_transformation(const Eigen::Vector3f& t) {
      Eigen::Matrix4f T = Eigen::Matrix4f::Identity();
      T.block<3,1>(0,3) = t;
      return T;
    }

    static inline Eigen::Matrix4f to_transformation(const Eigen::Matrix3f& R, const Eigen::Vector3f& t) {
      Eigen::Matrix4f T = Eigen::Matrix4f::Identity();
      T.block<3,3>(0,0) = R;
      T.block<3,1>(0,3) = t;
      return T;
    }

    static inline Eigen::Vector3f to_inverse_translation(const Eigen::Matrix4f& T) {
      Eigen::Vector3f t_inv = -T.block<3,3>(0,0).inverse() * T.block<3,1>(0,3);
      return t_inv;
    }

    static inline Eigen::Matrix3f to_inverse_rotation(const Eigen::Matrix4f& T) {
      Eigen::Matrix3f R_inv = (T.block<3,3>(0,0)).inverse();
      return R_inv;
    }

    static inline Eigen::Matrix4f to_inverse_transformation(const Eigen::Matrix4f& T) {
      Eigen::Matrix4f T_inv = Eigen::Matrix4f::Identity();
      T_inv.block<3,3>(0,0) = T.block<3,3>(0,0).inverse();
      T_inv.block<3,1>(0,3) = -T.block<3,3>(0,0).inverse() * T.block<3,1>(0,3);
      return T_inv;
    }

    template <typename T>
      static inline typename std::enable_if<std::is_arithmetic<T>::value, T>::type
      clamp(const T& f, const T& a, const T& b) {
        return std::max(a, std::min(f, b));
      }

    static inline void clamp(Eigen::Ref<Eigen::VectorXf>             res,
                             const Eigen::Ref<const Eigen::VectorXf> a,
                             const Eigen::Ref<Eigen::VectorXf>       b) {
      res = (res.array() < a.array()).select(a, res);
      res = (res.array() >= b.array()).select(b, res);
    }

    template <typename R, typename A, typename B>
      static inline void clamp(Eigen::MatrixBase<R>&       res,
                               const Eigen::MatrixBase<A>& a,
                               const Eigen::MatrixBase<B>& b) {
        res = res.array().max(a.array());
        res = res.array().min(b.array());
      }

    /*! \brief Compute the normal vector of a plane defined by 3 points.
     * The direction of the normal depends on the order of the points.
     */
    static Eigen::Vector4f plane_normal(const Eigen::Vector4f& p1,
                                        const Eigen::Vector4f& p2,
                                        const Eigen::Vector4f& p3) {
      // Plane tangent vectors
      const Eigen::Vector3f t1 = p2.head<3>() - p1.head<3>();
      const Eigen::Vector3f t2 = p3.head<3>() - p2.head<3>();
      // Unit normal vector
      return t1.cross(t2).normalized().homogeneous();
    }
  }
}
#endif
