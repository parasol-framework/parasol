// Anti-Grain Geometry - Version 2.4
// Copyright (C) 2002-2005 Maxim Shemanarev (http://www.antigrain.com)
//
// Permission to copy, use, modify, sell and distribute this software
// is granted provided this copyright notice appears in all copies.
// This software is provided "as is" without express or implied
// warranty, and with no claim as to its suitability for any purpose.

#ifndef AGG_COLOR_RGBA_INCLUDED
#define AGG_COLOR_RGBA_INCLUDED

#include <math.h>
#include "agg_basics.h"
#include "../../../link/linear_rgb.h"

namespace agg {
   // Supported byte orders for RGB and RGBA pixel formats

    struct order_rgb  { enum rgb_e  { R=0, G=1, B=2, rgb_tag }; };       //----order_rgb
    struct order_bgr  { enum bgr_e  { B=0, G=1, R=2, rgb_tag }; };       //----order_bgr
    struct order_rgba { enum rgba_e { R=0, G=1, B=2, A=3, rgba_tag }; }; //----order_rgba
    struct order_argb { enum argb_e { A=0, R=1, G=2, B=3, rgba_tag }; }; //----order_argb
    struct order_abgr { enum abgr_e { A=0, B=1, G=2, R=3, rgba_tag }; }; //----order_abgr
    struct order_bgra { enum bgra_e { B=0, G=1, R=2, A=3, rgba_tag }; }; //----order_bgra

    struct rgba { // RGB values from 0.0 - 1.0
         typedef double value_type;

         double r, g, b, a;
         bool linear;

         rgba() {}

         rgba(double r_, double g_, double b_, double a_=1.0, bool linear_=false) :
            r(r_), g(g_), b(b_), a(a_), linear(linear_) {}

         rgba(const rgba& c, double a_) : r(c.r), g(c.g), b(c.b), a(a_), linear(c.linear) {}

         rgba(RGB8 &RGB) :
            r(DOUBLE(RGB.Red) / 255.0),
            g(DOUBLE(RGB.Green) / 255.0),
            b(DOUBLE(RGB.Blue) / 255.0),
            a(DOUBLE(RGB.Alpha) / 255.0),
            linear(false) {}

         rgba(RGB8 &RGB, UBYTE Alpha) :
            r(DOUBLE(RGB.Red) / 255.0),
            g(DOUBLE(RGB.Green) / 255.0),
            b(DOUBLE(RGB.Blue) / 255.0),
            a(DOUBLE(Alpha) / 255.0),
            linear(false) {}

         rgba(FRGB &RGB, DOUBLE Alpha) :
            r(RGB.Red),
            g(RGB.Green),
            b(RGB.Blue),
            a(Alpha),
            linear(false) {}

         void clear() {
            r = g = b = a = 0;
            linear = false;
         }

         const rgba & transparent() {
            a = 0.0;
            linear = false;
            return *this;
         }

         const rgba& opacity(double a_) {
            if (a_ < 0.0) a_ = 0.0;
            if (a_ > 1.0) a_ = 1.0;
            a = a_;
            return *this;
         }

         double opacity() const {
            return a;
         }

         const rgba & premultiply() {
            r *= a;
            g *= a;
            b *= a;
            return *this;
         }

         const rgba & premultiply(double a_) {
            if (a <= 0.0 || a_ <= 0.0) {
                r = g = b = a = 0.0;
                return *this;
            }
            a_ /= a;
            r *= a_;
            g *= a_;
            b *= a_;
            a  = a_;
            return *this;
         }

         const rgba & demultiply() {
            if (a == 0) {
               r = g = b = 0;
               return *this;
            }
            double a_ = 1.0 / a;
            r *= a_;
            g *= a_;
            b *= a_;
            return *this;
         }

         rgba gradient(rgba c, double k) const {
            rgba ret;
            ret.r = r + (c.r - r) * k;
            ret.g = g + (c.g - g) * k;
            ret.b = b + (c.b - b) * k;
            ret.a = a + (c.a - a) * k;
            return ret;
         }

         static rgba no_color() { return rgba(0,0,0,0, false); }

         static rgba from_wavelength(double wl, double gamma = 1.0);

         explicit rgba(double wavelen, double gamma=1.0) {
            *this = from_wavelength(wavelen, gamma);
         }

         void to_linear() {
            if (linear == false) {
               if (r <= 0.04045) r /= 12.92;
               else r = std::pow((r + 0.055) / 1.055, 2.4);

               if (g <= 0.04045) g /= 12.92;
               else g = std::pow((g + 0.055) / 1.055, 2.4);

               if (b <= 0.04045) b /= 12.92;
               else b = std::pow((b + 0.055) / 1.055, 2.4);
            }
            linear = true;
         }

         void to_rgb() {
            if (linear == true) {
               if (r < 0.0031308) r *= 12.92;
               else {
                  r = (std::pow(r, 1.0 / 2.4) * 1.055) - 0.055;
                  if (r < 0) r = 0;
                  else if (r > 255) r = 255;
               }

               if (g < 0.0031308) g *= 12.92;
               else {
                  g = (std::pow(g, 1.0 / 2.4) * 1.055) - 0.055;
                  if (g < 0) g = 0;
                  else if (g > 255) g = 255;
               }

               if (b < 0.0031308) b *= 12.92;
               else {
                  b = (std::pow(b, 1.0 / 2.4) * 1.055) - 0.055;
                  if (b < 0) b = 0;
                  else if (b > 255) b = 255;
               }
            }
            linear = false;
         }
   };

    inline rgba rgba_pre(double r, double g, double b, double a=1.0) {
        return rgba(r, g, b, a).premultiply();
    }

    inline rgba rgba_pre(const rgba& c) {
        return rgba(c).premultiply();
    }

    inline rgba rgba_pre(const rgba& c, double a) {
        return rgba(c, a).premultiply();
    }

    inline rgba rgba::from_wavelength(double wl, double gamma) {
        rgba t(0.0, 0.0, 0.0);

        if (wl >= 380.0 && wl <= 440.0) {
            t.r = -1.0 * (wl - 440.0) / (440.0 - 380.0);
            t.b = 1.0;
        }
        else if (wl >= 440.0 && wl <= 490.0) {
            t.g = (wl - 440.0) / (490.0 - 440.0);
            t.b = 1.0;
        }
        else if (wl >= 490.0 && wl <= 510.0) {
            t.g = 1.0;
            t.b = -1.0 * (wl - 510.0) / (510.0 - 490.0);
        }
        else if (wl >= 510.0 && wl <= 580.0) {
            t.r = (wl - 510.0) / (580.0 - 510.0);
            t.g = 1.0;
        }
        else if (wl >= 580.0 && wl <= 645.0) {
            t.r = 1.0;
            t.g = -1.0 * (wl - 645.0) / (645.0 - 580.0);
        }
        else if (wl >= 645.0 && wl <= 780.0) {
            t.r = 1.0;
        }

        double s = 1.0;
        if (wl > 700.0)       s = 0.3 + 0.7 * (780.0 - wl) / (780.0 - 700.0);
        else if (wl <  420.0) s = 0.3 + 0.7 * (wl - 380.0) / (420.0 - 380.0);

        t.r = pow(t.r * s, gamma);
        t.g = pow(t.g * s, gamma);
        t.b = pow(t.b * s, gamma);
        return t;
    }

    //===================================================================

    struct rgba8 { // RGB values from 0 - 255
        typedef UBYTE  value_type;
        typedef ULONG calc_type;
        typedef LONG  long_type;
        enum base_scale_e {
            base_shift = 8,
            base_scale = 1 << base_shift,
            base_mask  = base_scale - 1
        };
        typedef rgba8 self_type;

        value_type r;
        value_type g;
        value_type b;
        value_type a;

        rgba8() {}

        rgba8(const RGB8 &RGB) :
            r(value_type(RGB.Red)),
            g(value_type(RGB.Green)),
            b(value_type(RGB.Blue)),
            a(value_type(RGB.Alpha)) {}

        rgba8(const RGB8 &RGB, UBYTE Alpha) :
            r(value_type(RGB.Red)),
            g(value_type(RGB.Green)),
            b(value_type(RGB.Blue)),
            a(value_type(Alpha)) {}
        
        rgba8(const FRGB &RGB) :
            r((value_type)uround(RGB.Red * double(base_mask))),
            g((value_type)uround(RGB.Green * double(base_mask))),
            b((value_type)uround(RGB.Blue * double(base_mask))),
            a((value_type)uround(RGB.Alpha * double(base_mask))) {}

        rgba8(const FRGB &RGB, FLOAT Alpha) :
            r((value_type)uround(RGB.Red * double(base_mask))),
            g((value_type)uround(RGB.Green * double(base_mask))),
            b((value_type)uround(RGB.Blue * double(base_mask))),
            a((value_type)uround(Alpha * double(base_mask))) {}

        rgba8(unsigned r_, unsigned g_, unsigned b_, unsigned a_=base_mask) :
            r(value_type(r_)),
            g(value_type(g_)),
            b(value_type(b_)),
            a(value_type(a_)) {}

        rgba8(const rgba& c, double a_) :
            r((value_type)uround(c.r * double(base_mask))),
            g((value_type)uround(c.g * double(base_mask))),
            b((value_type)uround(c.b * double(base_mask))),
            a((value_type)uround(a_  * double(base_mask))) {}

        rgba8(const self_type& c, unsigned a_) :
            r(c.r), g(c.g), b(c.b), a(value_type(a_)) {}

        rgba8(const rgba& c) :
            r((value_type)uround(c.r * double(base_mask))),
            g((value_type)uround(c.g * double(base_mask))),
            b((value_type)uround(c.b * double(base_mask))),
            a((value_type)uround(c.a * double(base_mask))) {}

        void clear() {
            r = g = b = a = 0;
        }

        const self_type& transparent() {
            a = 0;
            return *this;
        }

        const self_type& opacity(double a_) {
            if (a_ < 0.0) a_ = 0.0;
            if (a_ > 1.0) a_ = 1.0;
            a = (value_type)uround(a_ * double(base_mask));
            return *this;
        }

        double opacity() const {
            return double(a) / double(base_mask);
        }

        inline const self_type& premultiply() {
            if (a == base_mask) return *this;
            if (a == 0) {
                r = g = b = 0;
                return *this;
            }
            r = value_type((calc_type(r) * a) >> base_shift);
            g = value_type((calc_type(g) * a) >> base_shift);
            b = value_type((calc_type(b) * a) >> base_shift);
            return *this;
        }

        inline const self_type& premultiply(unsigned a_) {
            if (a == base_mask && a_ >= base_mask) return *this;
            if (a == 0 || a_ == 0) {
                r = g = b = a = 0;
                return *this;
            }
            calc_type r_ = (calc_type(r) * a_) / a;
            calc_type g_ = (calc_type(g) * a_) / a;
            calc_type b_ = (calc_type(b) * a_) / a;
            r = value_type((r_ > a_) ? a_ : r_);
            g = value_type((g_ > a_) ? a_ : g_);
            b = value_type((b_ > a_) ? a_ : b_);
            a = value_type(a_);
            return *this;
        }


        inline const self_type& demultiply() {
            if (a == base_mask) return *this;
            if (a == 0) {
                r = g = b = 0;
                return *this;
            }
            calc_type r_ = (calc_type(r) * base_mask) / a;
            calc_type g_ = (calc_type(g) * base_mask) / a;
            calc_type b_ = (calc_type(b) * base_mask) / a;
            r = value_type((r_ > calc_type(base_mask)) ? calc_type(base_mask) : r_);
            g = value_type((g_ > calc_type(base_mask)) ? calc_type(base_mask) : g_);
            b = value_type((b_ > calc_type(base_mask)) ? calc_type(base_mask) : b_);
            return *this;
        }

        inline self_type gradient(const self_type& c, double k) const {
            self_type ret;
            calc_type ik = uround(k * int(base_scale));
            ret.r = value_type(calc_type(r) + (((calc_type(c.r) - r) * ik) >> base_shift));
            ret.g = value_type(calc_type(g) + (((calc_type(c.g) - g) * ik) >> base_shift));
            ret.b = value_type(calc_type(b) + (((calc_type(c.b) - b) * ik) >> base_shift));
            ret.a = value_type(calc_type(a) + (((calc_type(c.a) - a) * ik) >> base_shift));
            return ret;
        }

        inline self_type linear_gradient(const self_type& c, double k) const {
            self_type ret;
            calc_type ik = uround(k * int(base_scale));
            calc_type r_linear = glLinearRGB.convert(r);
            calc_type g_linear = glLinearRGB.convert(g);
            calc_type b_linear = glLinearRGB.convert(b);
            calc_type c_r_linear = glLinearRGB.convert(c.r);
            calc_type c_g_linear = glLinearRGB.convert(c.g);
            calc_type c_b_linear = glLinearRGB.convert(c.b);

            ret.r = glLinearRGB.invert(value_type(r_linear + (((c_r_linear - r_linear) * ik) >> base_shift)));
            ret.g = glLinearRGB.invert(value_type(g_linear + (((c_g_linear - g_linear) * ik) >> base_shift)));
            ret.b = glLinearRGB.invert(value_type(b_linear + (((c_b_linear - b_linear) * ik) >> base_shift)));
            ret.a = value_type(calc_type(a) + (((calc_type(c.a) - a) * ik) >> base_shift));
            return ret;
        }

        inline void add(const self_type& c, unsigned cover) {
            calc_type cr, cg, cb, ca;
            if (cover == cover_mask) {
                if (c.a == base_mask) {
                    *this = c;
                }
                else {
                    cr = r + c.r; r = (cr > calc_type(base_mask)) ? calc_type(base_mask) : cr;
                    cg = g + c.g; g = (cg > calc_type(base_mask)) ? calc_type(base_mask) : cg;
                    cb = b + c.b; b = (cb > calc_type(base_mask)) ? calc_type(base_mask) : cb;
                    ca = a + c.a; a = (ca > calc_type(base_mask)) ? calc_type(base_mask) : ca;
                }
            }
            else {
                cr = r + ((c.r * cover + cover_mask/2) >> cover_shift);
                cg = g + ((c.g * cover + cover_mask/2) >> cover_shift);
                cb = b + ((c.b * cover + cover_mask/2) >> cover_shift);
                ca = a + ((c.a * cover + cover_mask/2) >> cover_shift);
                r = (cr > calc_type(base_mask)) ? calc_type(base_mask) : cr;
                g = (cg > calc_type(base_mask)) ? calc_type(base_mask) : cg;
                b = (cb > calc_type(base_mask)) ? calc_type(base_mask) : cb;
                a = (ca > calc_type(base_mask)) ? calc_type(base_mask) : ca;
            }
        }

        template<class GammaLUT>
        inline void apply_gamma_dir(const GammaLUT& gamma) {
            r = gamma.dir(r);
            g = gamma.dir(g);
            b = gamma.dir(b);
        }

        template<class GammaLUT>
        inline void apply_gamma_inv(const GammaLUT& gamma) {
            r = gamma.inv(r);
            g = gamma.inv(g);
            b = gamma.inv(b);
        }

        inline void to_linear() {
           r = glLinearRGB.convert(r);
           g = glLinearRGB.convert(g);
           b = glLinearRGB.convert(b);
        }
      
        inline void to_rgb() {
           r = glLinearRGB.invert(r);
           g = glLinearRGB.invert(g);
           b = glLinearRGB.invert(b);
        }

      static self_type no_color() { return self_type(0,0,0,0); }

      static self_type from_wavelength(double wl, double gamma = 1.0) {
         return self_type(rgba::from_wavelength(wl, gamma));
      }
   };

    //-------------------------------------------------------------

    inline rgba8 rgba8_pre(unsigned r, unsigned g, unsigned b, unsigned a = rgba8::base_mask) {
        return rgba8(r,g,b,a).premultiply();
    }

    inline rgba8 rgba8_pre(const rgba8& c) {
        return rgba8(c).premultiply();
    }

    inline rgba8 rgba8_pre(const rgba8& c, unsigned a) {
        return rgba8(c,a).premultiply();
    }

    inline rgba8 rgba8_pre(const rgba& c) {
        return rgba8(c).premultiply();
    }

    inline rgba8 rgba8_pre(const rgba& c, double a) {
        return rgba8(c,a).premultiply();
    }

    inline rgba8 rgb8_packed(unsigned v) {
        return rgba8((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
    }

    inline rgba8 bgr8_packed(unsigned v) {
        return rgba8(v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF);
    }

    inline rgba8 argb8_packed(unsigned v) {
        return rgba8((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF, v >> 24);
    }

    template<class GammaLUT> rgba8 rgba8_gamma_dir(rgba8 c, const GammaLUT& gamma)
    {
        return rgba8(gamma.dir(c.r), gamma.dir(c.g), gamma.dir(c.b), c.a);
    }

    template<class GammaLUT> rgba8 rgba8_gamma_inv(rgba8 c, const GammaLUT& gamma)
    {
        return rgba8(gamma.inv(c.r), gamma.inv(c.g), gamma.inv(c.b), c.a);
    }

    //==================================================================rgba16
    struct rgba16
    {
        typedef int16u value_type;
        typedef int32u calc_type;
        typedef int64  long_type;
        enum base_scale_e
        {
            base_shift = 16,
            base_scale = 1 << base_shift,
            base_mask  = base_scale - 1
        };
        typedef rgba16 self_type;

        value_type r;
        value_type g;
        value_type b;
        value_type a;


        rgba16() {}


        rgba16(unsigned r_, unsigned g_, unsigned b_, unsigned a_=base_mask) :
            r(value_type(r_)),
            g(value_type(g_)),
            b(value_type(b_)),
            a(value_type(a_)) {}


        rgba16(const self_type& c, unsigned a_) :
            r(c.r), g(c.g), b(c.b), a(value_type(a_)) {}


        rgba16(const rgba& c) :
            r((value_type)uround(c.r * double(base_mask))),
            g((value_type)uround(c.g * double(base_mask))),
            b((value_type)uround(c.b * double(base_mask))),
            a((value_type)uround(c.a * double(base_mask))) {}


        rgba16(const rgba& c, double a_) :
            r((value_type)uround(c.r * double(base_mask))),
            g((value_type)uround(c.g * double(base_mask))),
            b((value_type)uround(c.b * double(base_mask))),
            a((value_type)uround(a_  * double(base_mask))) {}


        rgba16(const rgba8& c) :
            r(value_type((value_type(c.r) << 8) | c.r)),
            g(value_type((value_type(c.g) << 8) | c.g)),
            b(value_type((value_type(c.b) << 8) | c.b)),
            a(value_type((value_type(c.a) << 8) | c.a)) {}


        rgba16(const rgba8& c, unsigned a_) :
            r(value_type((value_type(c.r) << 8) | c.r)),
            g(value_type((value_type(c.g) << 8) | c.g)),
            b(value_type((value_type(c.b) << 8) | c.b)),
            a(value_type((             a_ << 8) | c.a)) {}


        void clear() {
            r = g = b = a = 0;
        }


        const self_type& transparent() {
            a = 0;
            return *this;
        }


        inline const self_type& opacity(double a_) {
            if (a_ < 0.0) a_ = 0.0;
            if (a_ > 1.0) a_ = 1.0;
            a = (value_type)uround(a_ * double(base_mask));
            return *this;
        }


        double opacity() const {
            return double(a) / double(base_mask);
        }


        inline const self_type& premultiply() {
            if (a == base_mask) return *this;
            if (a == 0) {
                r = g = b = 0;
                return *this;
            }
            r = value_type((calc_type(r) * a) >> base_shift);
            g = value_type((calc_type(g) * a) >> base_shift);
            b = value_type((calc_type(b) * a) >> base_shift);
            return *this;
        }


        inline const self_type& premultiply(unsigned a_) {
            if (a == base_mask && a_ >= base_mask) return *this;
            if (a == 0 || a_ == 0) {
                r = g = b = a = 0;
                return *this;
            }
            calc_type r_ = (calc_type(r) * a_) / a;
            calc_type g_ = (calc_type(g) * a_) / a;
            calc_type b_ = (calc_type(b) * a_) / a;
            r = value_type((r_ > a_) ? a_ : r_);
            g = value_type((g_ > a_) ? a_ : g_);
            b = value_type((b_ > a_) ? a_ : b_);
            a = value_type(a_);
            return *this;
        }


        inline const self_type& demultiply() {
            if (a == base_mask) return *this;
            if (a == 0) {
                r = g = b = 0;
                return *this;
            }
            calc_type r_ = (calc_type(r) * base_mask) / a;
            calc_type g_ = (calc_type(g) * base_mask) / a;
            calc_type b_ = (calc_type(b) * base_mask) / a;
            r = value_type((r_ > calc_type(base_mask)) ? calc_type(base_mask) : r_);
            g = value_type((g_ > calc_type(base_mask)) ? calc_type(base_mask) : g_);
            b = value_type((b_ > calc_type(base_mask)) ? calc_type(base_mask) : b_);
            return *this;
        }


        inline self_type gradient(const self_type& c, double k) const {
            self_type ret;
            calc_type ik = uround(k * int(base_scale));
            ret.r = value_type(calc_type(r) + (((calc_type(c.r) - r) * ik) >> base_shift));
            ret.g = value_type(calc_type(g) + (((calc_type(c.g) - g) * ik) >> base_shift));
            ret.b = value_type(calc_type(b) + (((calc_type(c.b) - b) * ik) >> base_shift));
            ret.a = value_type(calc_type(a) + (((calc_type(c.a) - a) * ik) >> base_shift));
            return ret;
        }


        inline void add(const self_type& c, unsigned cover) {
            calc_type cr, cg, cb, ca;
            if (cover == cover_mask) {
                if (c.a == base_mask) *this = c;
                else {
                    cr = r + c.r; r = (cr > calc_type(base_mask)) ? calc_type(base_mask) : cr;
                    cg = g + c.g; g = (cg > calc_type(base_mask)) ? calc_type(base_mask) : cg;
                    cb = b + c.b; b = (cb > calc_type(base_mask)) ? calc_type(base_mask) : cb;
                    ca = a + c.a; a = (ca > calc_type(base_mask)) ? calc_type(base_mask) : ca;
                }
            }
            else {
                cr = r + ((c.r * cover + cover_mask) >> cover_shift);
                cg = g + ((c.g * cover + cover_mask) >> cover_shift);
                cb = b + ((c.b * cover + cover_mask) >> cover_shift);
                ca = a + ((c.a * cover + cover_mask) >> cover_shift);
                r = (cr > calc_type(base_mask)) ? calc_type(base_mask) : cr;
                g = (cg > calc_type(base_mask)) ? calc_type(base_mask) : cg;
                b = (cb > calc_type(base_mask)) ? calc_type(base_mask) : cb;
                a = (ca > calc_type(base_mask)) ? calc_type(base_mask) : ca;
            }
        }

        template<class GammaLUT>
        inline void apply_gamma_dir(const GammaLUT& gamma) {
            r = gamma.dir(r);
            g = gamma.dir(g);
            b = gamma.dir(b);
        }

        template<class GammaLUT>
        inline void apply_gamma_inv(const GammaLUT& gamma) {
            r = gamma.inv(r);
            g = gamma.inv(g);
            b = gamma.inv(b);
        }

        static self_type no_color() { return self_type(0,0,0,0); }

        static self_type from_wavelength(double wl, double gamma = 1.0) {
            return self_type(rgba::from_wavelength(wl, gamma));
        }
    };

    //--------------------------------------------------------------

    inline rgba16 rgba16_pre(unsigned r, unsigned g, unsigned b, unsigned a = rgba16::base_mask) {
        return rgba16(r,g,b,a).premultiply();
    }

    inline rgba16 rgba16_pre(const rgba16& c, unsigned a) {
        return rgba16(c,a).premultiply();
    }

    inline rgba16 rgba16_pre(const rgba& c) {
        return rgba16(c).premultiply();
    }

    inline rgba16 rgba16_pre(const rgba& c, double a) {
        return rgba16(c,a).premultiply();
    }

    inline rgba16 rgba16_pre(const rgba8& c) {
        return rgba16(c).premultiply();
    }

    inline rgba16 rgba16_pre(const rgba8& c, unsigned a) {
        return rgba16(c,a).premultiply();
    }

    //------------------------------------------------------rgba16_gamma_dir

    template<class GammaLUT>
    rgba16 rgba16_gamma_dir(rgba16 c, const GammaLUT& gamma)
    {
        return rgba16(gamma.dir(c.r), gamma.dir(c.g), gamma.dir(c.b), c.a);
    }

    template<class GammaLUT>
    rgba16 rgba16_gamma_inv(rgba16 c, const GammaLUT& gamma)
    {
        return rgba16(gamma.inv(c.r), gamma.inv(c.g), gamma.inv(c.b), c.a);
    }
}

#endif
