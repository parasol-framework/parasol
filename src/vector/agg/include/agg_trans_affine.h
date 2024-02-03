//----------------------------------------------------------------------------
// Anti-Grain Geometry - Version 2.4
// Copyright (C) 2002-2005 Maxim Shemanarev (http://www.antigrain.com)
//
// Permission to copy, use, modify, sell and distribute this software
// is granted provided this copyright notice appears in all copies.
// This software is provided "as is" without express or implied
// warranty, and with no claim as to its suitability for any purpose.
//
// Affine transformation classes.
//
// See Implementation agg_trans_affine.cpp
//
// Affine transformation are linear transformations in Cartesian coordinates
// (strictly speaking not only in Cartesian, but for the beginning we will
// think so). They are rotation, scaling, translation and skewing.
// After any affine transformation a line segment remains a line segment
// and it will never become a curve.
//
// There will be no math about matrix calculations, since it has been
// described many times. Ask yourself a very simple question:
// "why do we need to understand and use some matrix stuff instead of just
// rotating, scaling and so on". The answers are:
//
// 1. Any combination of transformations can be done by only 4 multiplications
//    and 4 additions in floating point.
// 2. One matrix transformation is equivalent to the number of consecutive
//    discrete transformations, i.e. the matrix "accumulates" all transformations
//    in the order of their settings. Suppose we have 4 transformations:
//       * rotate by 30 degrees,
//       * scale X to 2.0,
//       * scale Y to 1.5,
//       * move to (100, 100).
//    The result will depend on the order of these transformations,
//    and the advantage of matrix is that the sequence of discrete calls:
//    rotate(30), scaleX(2.0), scaleY(1.5), move(100,100)
//    will have exactly the same result as the following matrix transformations:
//
//    affine_matrix m;
//    m *= rotate_matrix(30);
//    m *= scaleX_matrix(2.0);
//    m *= scaleY_matrix(1.5);
//    m *= move_matrix(100,100);
//
//    m.transform_my_point_at_last(x, y);
//
// What is the good of it? In real life we will set-up the matrix only once
// and then transform many points, let alone the convenience to set any
// combination of transformations.
//
// So, how to use it? Very easy - literally as it's shown above. Not quite,
// let us write a correct example:
//
// agg::trans_affine m;
// m *= agg::trans_affine_rotation(30.0 * 3.1415926 / 180.0);
// m *= agg::trans_affine_scaling(2.0, 1.5);
// m *= agg::trans_affine_translation(100.0, 100.0);
// m.transform(&x, &y);
//
// The affine matrix is all you need to perform any linear transformation,
// but all transformations have origin point (0,0). It means that we need to
// use 2 translations if we want to rotate someting around (100,100):
//
// m *= agg::trans_affine_translation(-100.0, -100.0);         // move to (0,0)
// m *= agg::trans_affine_rotation(30.0 * 3.1415926 / 180.0);  // rotate
// m *= agg::trans_affine_translation(100.0, 100.0);           // move back to (100,100)


#ifndef AGG_TRANS_AFFINE_INCLUDED
#define AGG_TRANS_AFFINE_INCLUDED

#include <math.h>
#include "agg_basics.h"

namespace agg
{
   const double affine_epsilon = 1e-14;

   struct trans_affine {
      // sx,sy = scale
      // shy,shx = shear / rotate
      // tx, ty = translate
      double sx, shy, shx, sy, tx, ty;

      // Identity matrix
      trans_affine() : sx(1.0), shy(0.0), shx(0.0), sy(1.0), tx(0.0), ty(0.0) {}

      // Custom matrix. Usually used in derived classes
      trans_affine(double v0, double v1, double v2, double v3, double v4, double v5) :
          sx(v0), shy(v1), shx(v2), sy(v3), tx(v4), ty(v5) {}

      // Custom matrix from m[6]
      explicit trans_affine(const double* m) : sx(m[0]), shy(m[1]), shx(m[2]), sy(m[3]), tx(m[4]), ty(m[5]) {}

      // Rectangle to a parallelogram.
      trans_affine(double x1, double y1, double x2, double y2, const double* parl) {
          rect_to_parl(x1, y1, x2, y2, parl);
      }

      // Parallelogram to a rectangle.
      trans_affine(const double* parl, double x1, double y1, double x2, double y2) {
          parl_to_rect(parl, x1, y1, x2, y2);
      }

      // Arbitrary parallelogram transformation.
      trans_affine(const double* src, const double* dst) {
          parl_to_parl(src, dst);
      }

      inline bool is_normal() { // Return true if the transform is normalised (does nothing if applied)
         return sx == 1.0 and shy == 0 and shx == 0 and sy == 1.0 and tx == 0.0 and ty == 0.0;
      }

      inline bool is_simple() { // Returns true if the transform configuration is limited to translating.
         return sx == 1.0 and shy == 0 and shx == 0 and sy == 1.0;
      }

      inline bool is_complex() {
         return (sx != 1.0) or (shy != 0) or (shx != 0) or (sy != 1.0);
      }

      //---------------------------------- Parallelogram transformations
      // transform a parallelogram to another one. Src and dst are
      // pointers to arrays of three points (double[6], x1,y1,...) that
      // identify three corners of the parallelograms assuming implicit
      // fourth point. The arguments are arrays of double[6] mapped
      // to x1,y1, x2,y2, x3,y3  where the coordinates are:
      //        *-----------------*
      //       /          (x3,y3)/
      //      /                 /
      //     /(x1,y1)   (x2,y2)/
      //    *-----------------*

      const trans_affine& parl_to_parl(const double* src, const double* dst);
      const trans_affine& rect_to_parl(double x1, double y1, double x2, double y2, const double* parl);
      const trans_affine& parl_to_rect(const double* parl, double x1, double y1, double x2, double y2);

      // Reset - load an identity matrix
      const trans_affine& reset();

      // Direct transformations operations
      const trans_affine& translate(double x, double y);
      const trans_affine& rotate(double a);
      const trans_affine& scale(double s);
      const trans_affine& scale(double x, double y);
      const trans_affine& scaleX(double x);
      const trans_affine& scaleY(double y);

      // Multiply matrix to another one
      const trans_affine& multiply(const trans_affine& m);

      const trans_affine& multiply(double sx, double shy, double shx, double sy, double tx, double ty) {
         trans_affine m(sx, shy, shx, sy, tx, ty);
         multiply(m);
         return *this;
      }

      // Multiply "m" to "this" and assign the result to "this"
      const trans_affine& premultiply(const trans_affine& m);

      // Multiply matrix to inverse of another one
      const trans_affine& multiply_inv(const trans_affine& m);

      // Multiply inverse of "m" to "this" and assign the result to "this"
      const trans_affine& premultiply_inv(const trans_affine& m);

      // Invert matrix. Do not try to invert degenerate matrices,
      // there's no check for validity. If you set scale to 0 and
      // then try to invert matrix, expect unpredictable result.
      const trans_affine& invert();

      // Mirroring around X
      const trans_affine& flip_x();

      // Mirroring around Y
      const trans_affine& flip_y();

      // Store matrix to an array [6] of double
      void store_to(double* m) const {
          *m++ = sx; *m++ = shy; *m++ = shx; *m++ = sy; *m++ = tx; *m++ = ty;
      }

      // Load matrix from an array [6] of double
      const trans_affine& load_from(const double* m) {
          sx = *m++; shy = *m++; shx = *m++; sy = *m++; tx = *m++;  ty = *m++;
          return *this;
      }

      const trans_affine& load_all(const double a, const double b, const double c, const double d, const double e, const double f) {
          sx = a; shy = b; shx = c; sy = d; tx = e; ty = f;
          return *this;
      }

      // Multiply the matrix by another one
      const trans_affine& operator *= (const trans_affine& m) { return multiply(m); }

      // Multiply the matrix by inverse of another one
      const trans_affine& operator /= (const trans_affine& m) { return multiply_inv(m); }

      // Multiply the matrix by another one and return the result in a separate matrix.

      trans_affine operator * (const trans_affine& m) { return trans_affine(*this).multiply(m); }

      // Multiply the matrix by inverse of another one and return the result in a seperate matrix.

      trans_affine operator / (const trans_affine& m) { return trans_affine(*this).multiply_inv(m); }

      // Calculate and return the inverse matrix
      trans_affine operator ~ () const {
          trans_affine ret = *this;
          return ret.invert();
      }

      // Equal operator with default epsilon
      bool operator == (const trans_affine& m) const { return is_equal(m, affine_epsilon); }

      // Not Equal operator with default epsilon
      bool operator != (const trans_affine& m) const { return !is_equal(m, affine_epsilon); }

      // Direct transformation of x and y
      void transform(double* x, double* y) const;

      point_d transform(const point_d &) const;

      // Direct transformation of x and y, 2x2 matrix only, no translation
      void transform_2x2(double* x, double* y) const;

      // Inverse transformation of x and y. It works slower than the
      // direct transformation. For massive operations it's better to
      // invert() the matrix and then use direct transformations.
      void inverse_transform(double* x, double* y) const;

      // Calculate the determinant of matrix
      double determinant() const { return sx * sy - shy * shx; }

      // Calculate the reciprocal of the determinant
      double determinant_reciprocal() const { return 1.0 / (sx * sy - shy * shx); }

      // Get the average scale (by X and Y).
      // Basically used to calculate the approximation_scale when decompositioning curves into line segments.
      double scale() const;

      // Check to see if the matrix is not degenerate
      bool is_valid(double epsilon = affine_epsilon) const;

      // Check to see if it's an identity matrix
      bool is_identity(double epsilon = affine_epsilon) const;

      // Check to see if two matrices are equal
      bool is_equal(const trans_affine& m, double epsilon = affine_epsilon) const;

      // Determine the major parameters. Use with caution considering possible degenerate cases.

      double rotation() const;
      void translation(double* dx, double* dy) const;
      void scaling(double* x, double* y) const;
      void scaling_abs(double* x, double* y) const;
   };
   
   inline point_d trans_affine::transform(const point_d &Point) const {
      return { Point.x * sx  + Point.y * shx + tx, Point.x * shy + Point.y * sy  + ty };
   }

   inline void trans_affine::transform(double* x, double* y) const {
      double tmp = *x;
      *x = tmp * sx  + *y * shx + tx;
      *y = tmp * shy + *y * sy  + ty;
   }

   inline void trans_affine::transform_2x2(double* x, double* y) const {
      double tmp = *x;
      *x = tmp * sx  + *y * shx;
      *y = tmp * shy + *y * sy;
   }

   inline void trans_affine::inverse_transform(double* x, double* y) const {
       double d = determinant_reciprocal();
       double a = (*x - tx) * d;
       double b = (*y - ty) * d;
       *x = a * sy - b * shx;
       *y = b * sx - a * shy;
   }

   inline double trans_affine::scale() const {
      double x = 0.707106781 * sx  + 0.707106781 * shx;
      double y = 0.707106781 * shy + 0.707106781 * sy;
      return sqrt(x*x + y*y);
   }

   inline const trans_affine& trans_affine::translate(double x, double y) {
      tx += x;
      ty += y;
      return *this;
   }

   inline const trans_affine& trans_affine::rotate(double a) {
      double ca = cos(a);
      double sa = sin(a);
      double t0 = sx  * ca - shy * sa;
      double t2 = shx * ca - sy * sa;
      double t4 = tx  * ca - ty * sa;
      shy = sx  * sa + shy * ca;
      sy  = shx * sa + sy * ca;
      ty  = tx  * sa + ty * ca;
      sx  = t0;
      shx = t2;
      tx  = t4;
      return *this;
   }

   inline const trans_affine& trans_affine::scaleX(double x) {
       sx  *= x;
       shx *= x;
       tx  *= x;
       return *this;
   }

   inline const trans_affine& trans_affine::scaleY(double y) {
       sy  *= y;
       shy *= y;
       ty  *= y;
       return *this;
   }

   inline const trans_affine& trans_affine::scale(double x, double y) {
       double mm0 = x; // Possible hint for the optimizer
       double mm3 = y;
       sx  *= mm0;
       shx *= mm0;
       tx  *= mm0;
       shy *= mm3;
       sy  *= mm3;
       ty  *= mm3;
       return *this;
   }

   inline const trans_affine& trans_affine::scale(double s) {
       double m = s; // Possible hint for the optimizer
       sx  *= m;
       shx *= m;
       tx  *= m;
       shy *= m;
       sy  *= m;
       ty  *= m;
       return *this;
   }

   inline const trans_affine& trans_affine::premultiply(const trans_affine& m) {
      trans_affine t = m;
      return *this = t.multiply(*this);
   }

   inline const trans_affine& trans_affine::multiply_inv(const trans_affine& m) {
      trans_affine t = m;
      t.invert();
      return multiply(t);
   }

   inline const trans_affine& trans_affine::premultiply_inv(const trans_affine& m) {
      trans_affine t = m;
      t.invert();
      return *this = t.multiply(*this);
   }

   // Used to calculate scaling coefficients in image resampling.  When there is considerable shear this method gives us much
   // better estimation than just sx, sy.

   inline void trans_affine::scaling_abs(double* x, double* y) const {
      *x = sqrt(sx  * sx  + shx * shx);
      *y = sqrt(shy * shy + sy  * sy);
   }

   // Rotation matrix. sin() and cos() are calculated twice for the same angle.
   // There's no harm because the performance of sin()/cos() is very good on all
   // modern processors. Besides, this operation is not going to be invoked too
   // often.

   class trans_affine_rotation : public trans_affine {
      public:
         trans_affine_rotation(double a) : trans_affine(cos(a), sin(a), -sin(a), cos(a), 0.0, 0.0) {}
   };

   // Scaling matrix. x, y - scale coefficients by X and Y respectively
   class trans_affine_scaling : public trans_affine {
      public:
         trans_affine_scaling(double x, double y) : trans_affine(x, 0.0, 0.0, y, 0.0, 0.0) {}
         trans_affine_scaling(double s) : trans_affine(s, 0.0, 0.0, s, 0.0, 0.0) {}
   };

   // Translation matrix
   class trans_affine_translation : public trans_affine {
      public:
         trans_affine_translation(double x, double y) :
            trans_affine(1.0, 0.0, 0.0, 1.0, x, y) {}
   };

   // Skewing (shear) matrix
   class trans_affine_skewing : public trans_affine {
      public:
         trans_affine_skewing(double x, double y) : trans_affine(1.0, tan(y), tan(x), 1.0, 0.0, 0.0) {}
   };

   // Rotate, Scale and Translate, associating 0...dist with line segment x1,y1,x2,y2

   class trans_affine_line_segment : public trans_affine {
      public:
         trans_affine_line_segment(double x1, double y1, double x2, double y2, double dist) {
            double dx = x2 - x1;
            double dy = y2 - y1;
            if (dist > 0.0) multiply(trans_affine_scaling(sqrt(dx * dx + dy * dy) / dist));
            multiply(trans_affine_rotation(atan2(dy, dx)));
            multiply(trans_affine_translation(x1, y1));
         }
   };

   // Reflection matrix. Reflect coordinates across the line through the origin containing the unit vector (ux, uy).
   // Contributed by John Horigan

   class trans_affine_reflection_unit : public trans_affine {
      public:
         trans_affine_reflection_unit(double ux, double uy) :
            trans_affine(2.0 * ux * ux - 1.0, 2.0 * ux * uy, 2.0 * ux * uy, 2.0 * uy * uy - 1.0, 0.0, 0.0) {}
   };

   // Reflection matrix. Reflect coordinates across the line through
   // the origin at the angle a or containing the non-unit vector (x, y).
   // Contributed by John Horigan

   class trans_affine_reflection : public trans_affine_reflection_unit {
      public:
         trans_affine_reflection(double a) : trans_affine_reflection_unit(cos(a), sin(a)) {}

         trans_affine_reflection(double x, double y) :
            trans_affine_reflection_unit(x / sqrt(x * x + y * y), y / sqrt(x * x + y * y)) {}
   };
}

#endif
