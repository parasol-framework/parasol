//----------------------------------------------------------------------------
// Anti-Grain Geometry - Version 2.4
// Copyright (C) 2002-2005 Maxim Shemanarev (http://www.antigrain.com)
//
// Permission to copy, use, modify, sell and distribute this software
// is granted provided this copyright notice appears in all copies.
// This software is provided "as is" without express or implied
// warranty, and with no claim as to its suitability for any purpose.


#ifndef AGG_BASICS_INCLUDED
#define AGG_BASICS_INCLUDED

#include <math.h>
#include "agg_config.h"

//---------------------------------------------------------AGG_CUSTOM_ALLOCATOR
#ifdef AGG_CUSTOM_ALLOCATOR
#include "agg_allocator.h"
#else
namespace agg
{
    // The policy of all AGG containers and memory allocation strategy
    // in general is that no allocated data requires explicit construction.
    // It means that the allocator can be really simple; you can even
    // replace new/delete to malloc/free. The constructors and destructors
    // won't be called in this case, however everything will remain working.
    // The second argument of deallocate() is the size of the allocated
    // block. You can use this information if you wish.
    //------------------------------------------------------------pod_allocator
    template<class T> struct pod_allocator
    {
        static T*   allocate(unsigned num)       { return new T [num]; }
        static void deallocate(T* ptr, unsigned) { delete [] ptr;      }
    };

    // Single object allocator. It's also can be replaced with your custom
    // allocator. The difference is that it can only allocate a single
    // object and the constructor and destructor must be called.
    // In AGG there is no need to allocate an array of objects with
    // calling their constructors (only single ones). So that, if you
    // replace these new/delete to malloc/free make sure that the in-place
    // new is called and take care of calling the destructor too.
    //------------------------------------------------------------obj_allocator
    template<class T> struct obj_allocator
    {
        static T*   allocate()         { return new T; }
        static void deallocate(T* ptr) { delete ptr;   }
    };
}
#endif


//-------------------------------------------------------- Default basic types
//
// If the compiler has different capacity of the basic types you can redefine
// them via the compiler command line or by generating agg_config.h that is
// empty by default.
//
#ifndef AGG_INT8
#define AGG_INT8 signed char
#endif

#ifndef AGG_INT8U
#define AGG_INT8U unsigned char
#endif

#ifndef AGG_INT16
#define AGG_INT16 short
#endif

#ifndef AGG_INT16U
#define AGG_INT16U unsigned short
#endif

#ifndef AGG_INT32
#define AGG_INT32 int
#endif

#ifndef AGG_INT32U
#define AGG_INT32U unsigned
#endif

#ifndef AGG_INT64
#if defined(_MSC_VER) || defined(__BORLANDC__)
#define AGG_INT64 signed __int64
#else
#define AGG_INT64 signed long long
#endif
#endif

#ifndef AGG_INT64U
#if defined(_MSC_VER) || defined(__BORLANDC__)
#define AGG_INT64U unsigned __int64
#else
#define AGG_INT64U unsigned long long
#endif
#endif

//------------------------------------------------ Some fixes for MS Visual C++
#if defined(_MSC_VER)
#pragma warning(disable:4786) // Identifier was truncated...
#endif

#if defined(_MSC_VER)
#define AGG_INLINE __forceinline
#else
#define AGG_INLINE inline
#endif

namespace agg
{
    //-------------------------------------------------------------------------
    typedef AGG_INT8   int8;
    typedef AGG_INT8U  int8u;
    typedef AGG_INT16  int16;
    typedef AGG_INT16U int16u;
    typedef AGG_INT32  int32;
    typedef AGG_INT32U int32u;
    typedef AGG_INT64  int64;
    typedef AGG_INT64U int64u;

#if defined(AGG_FISTP)
#pragma warning(push)
#pragma warning(disable : 4035) //Disable warning "no return value"
    AGG_INLINE int iround(double v)
    {
        int t;
        __asm fld   qword ptr [v]
        __asm fistp dword ptr [t]
        __asm mov eax, dword ptr [t]
    }
    AGG_INLINE unsigned uround(double v)
    {
        unsigned t;
        __asm fld   qword ptr [v]
        __asm fistp dword ptr [t]
        __asm mov eax, dword ptr [t]
    }
#pragma warning(pop)
    AGG_INLINE unsigned ufloor(double v)
    {
        return unsigned(floor(v));
    }
    AGG_INLINE unsigned uceil(double v)
    {
        return unsigned(ceil(v));
    }
#elif defined(AGG_QIFIST)
    AGG_INLINE int iround(double v)
    {
        return int(v);
    }
    AGG_INLINE int uround(double v)
    {
        return unsigned(v);
    }
    AGG_INLINE unsigned ufloor(double v)
    {
        return unsigned(floor(v));
    }
    AGG_INLINE unsigned uceil(double v)
    {
        return unsigned(ceil(v));
    }
#else
    AGG_INLINE int iround(double v)
    {
        return int((v < 0.0) ? v - 0.5 : v + 0.5);
    }
    AGG_INLINE int uround(double v)
    {
        return unsigned(v + 0.5);
    }
    AGG_INLINE unsigned ufloor(double v)
    {
        return unsigned(v);
    }
    AGG_INLINE unsigned uceil(double v)
    {
        return unsigned(ceil(v));
    }
#endif

    template<int Limit> struct saturation
    {
        AGG_INLINE static int iround(double v)
        {
            if(v < double(-Limit)) return -Limit;
            if(v > double( Limit)) return  Limit;
            return agg::iround(v);
        }
    };

    template<unsigned Shift> struct mul_one
    {
        AGG_INLINE static unsigned mul(unsigned a, unsigned b)
        {
            unsigned q = a * b + (1 << (Shift-1));
            return (q + (q >> Shift)) >> Shift;
        }
    };

    typedef unsigned char cover_type;

    static const int cover_shift = 8;
    static const int cover_size  = 1 << cover_shift;
    static const int cover_mask  = cover_size - 1;
    static const int cover_none  = 0;
    static const int cover_full  = cover_mask;

    // These constants determine the subpixel accuracy, to be more precise,
    // the number of bits of the fractional part of the coordinates.
    // The possible coordinate capacity in bits can be calculated by formula:
    // sizeof(int) * 8 - poly_subpixel_shift, i.e, for 32-bit integers and
    // 8-bits fractional part the capacity is 24 bits.

    static const int poly_subpixel_shift = 8;
    static const int poly_subpixel_scale = 1<<poly_subpixel_shift;
    static const int poly_subpixel_mask  = poly_subpixel_scale-1;

    enum filling_rule_e
    {
        fill_non_zero,
        fill_even_odd
    };

    const double pi = 3.14159265358979323846;

    inline double deg2rad(double deg)
    {
        return deg * pi / 180.0;
    }

    inline double rad2deg(double rad)
    {
        return rad * 180.0 / pi;
    }

    template<class T> struct rect_base
    {
        typedef T            value_type;
        typedef rect_base<T> self_type;
        T x1, y1, x2, y2;

        rect_base() {}
        rect_base(T x1_, T y1_, T x2_, T y2_) : x1(x1_), y1(y1_), x2(x2_), y2(y2_) {}

        void init(T x1_, T y1_, T x2_, T y2_) {
            x1 = x1_; y1 = y1_; x2 = x2_; y2 = y2_;
        }

        const self_type& normalize() {
            T t;
            if(x1 > x2) { t = x1; x1 = x2; x2 = t; }
            if(y1 > y2) { t = y1; y1 = y2; y2 = t; }
            return *this;
        }

        bool clip(const self_type& r) {
            if(x2 > r.x2) x2 = r.x2;
            if(y2 > r.y2) y2 = r.y2;
            if(x1 < r.x1) x1 = r.x1;
            if(y1 < r.y1) y1 = r.y1;
            return x1 <= x2 && y1 <= y2;
        }

        bool is_valid() const {
            return x1 <= x2 && y1 <= y2;
        }

        bool hit_test(T x, T y) const {
            return (x >= x1 && x <= x2 && y >= y1 && y <= y2);
        }
    };

    template<class Rect>
    inline Rect intersect_rectangles(const Rect& r1, const Rect& r2) {
        Rect r = r1;

        if (r.x2 > r2.x2) r.x2 = r2.x2;
        if (r.y2 > r2.y2) r.y2 = r2.y2;
        if (r.x1 < r2.x1) r.x1 = r2.x1;
        if (r.y1 < r2.y1) r.y1 = r2.y1;
        return r;
    }

    template<class Rect>
    inline Rect unite_rectangles(const Rect& r1, const Rect& r2) {
        Rect r = r1;
        if(r.x2 < r2.x2) r.x2 = r2.x2;
        if(r.y2 < r2.y2) r.y2 = r2.y2;
        if(r.x1 > r2.x1) r.x1 = r2.x1;
        if(r.y1 > r2.y1) r.y1 = r2.y1;
        return r;
    }

    typedef rect_base<int>    rect_i;
    typedef rect_base<float>  rect_f;
    typedef rect_base<double> rect_d;

    static const int path_cmd_stop     = 0;
    static const int path_cmd_move_to  = 1;
    static const int path_cmd_line_to  = 2;
    static const int path_cmd_curve3   = 3;
    static const int path_cmd_curve4   = 4;
    static const int path_cmd_end_poly = 0x0F;
    static const int path_cmd_mask     = 0x0F;

    static const int path_flags_none  = 0;
    static const int path_flags_ccw   = 0x10;
    static const int path_flags_cw    = 0x20;
    static const int path_flags_close = 0x40;
    static const int path_flags_mask  = 0xF0;

    //---------------------------------------------------------------is_vertex
    inline bool is_vertex(unsigned c)
    {
        return c >= path_cmd_move_to && c < path_cmd_end_poly;
    }

    //--------------------------------------------------------------is_drawing
    inline bool is_drawing(unsigned c)
    {
        return c >= path_cmd_line_to && c < path_cmd_end_poly;
    }

    //-----------------------------------------------------------------is_stop
    inline bool is_stop(unsigned c)
    {
        return c == path_cmd_stop;
    }

    //--------------------------------------------------------------is_move_to
    inline bool is_move_to(unsigned c)
    {
        return c == path_cmd_move_to;
    }

    //--------------------------------------------------------------is_line_to
    inline bool is_line_to(unsigned c)
    {
        return c == path_cmd_line_to;
    }

    //----------------------------------------------------------------is_curve
    inline bool is_curve(unsigned c)
    {
        return c == path_cmd_curve3 || c == path_cmd_curve4;
    }

    //---------------------------------------------------------------is_curve3
    inline bool is_curve3(unsigned c)
    {
        return c == path_cmd_curve3;
    }

    //---------------------------------------------------------------is_curve4
    inline bool is_curve4(unsigned c)
    {
        return c == path_cmd_curve4;
    }

    //-------------------------------------------------------------is_end_poly
    inline bool is_end_poly(unsigned c)
    {
        return (c & path_cmd_mask) == path_cmd_end_poly;
    }

    //----------------------------------------------------------------is_close
    inline bool is_close(unsigned c)
    {
        return (c & ~(path_flags_cw | path_flags_ccw)) ==
               (path_cmd_end_poly | path_flags_close);
    }

    //------------------------------------------------------------is_next_poly
    inline bool is_next_poly(unsigned c)
    {
        return is_stop(c) || is_move_to(c) || is_end_poly(c);
    }

    //-------------------------------------------------------------------is_cw
    inline bool is_cw(unsigned c)
    {
        return (c & path_flags_cw) != 0;
    }

    //------------------------------------------------------------------is_ccw
    inline bool is_ccw(unsigned c)
    {
        return (c & path_flags_ccw) != 0;
    }

    //-------------------------------------------------------------is_oriented
    inline bool is_oriented(unsigned c)
    {
        return (c & (path_flags_cw | path_flags_ccw)) != 0;
    }

    //---------------------------------------------------------------is_closed
    inline bool is_closed(unsigned c)
    {
        return (c & path_flags_close) != 0;
    }

    //----------------------------------------------------------get_close_flag
    inline unsigned get_close_flag(unsigned c)
    {
        return c & path_flags_close;
    }

    //-------------------------------------------------------clear_orientation
    inline unsigned clear_orientation(unsigned c)
    {
        return c & ~(path_flags_cw | path_flags_ccw);
    }

    //---------------------------------------------------------get_orientation
    inline unsigned get_orientation(unsigned c)
    {
        return c & (path_flags_cw | path_flags_ccw);
    }

    //---------------------------------------------------------set_orientation
    inline unsigned set_orientation(unsigned c, unsigned o)
    {
        return clear_orientation(c) | o;
    }

    //--------------------------------------------------------------point_base
    template<class T> struct point_base
    {
        typedef T value_type;
        T x,y;
        point_base() {}
        point_base(T x_, T y_) : x(x_), y(y_) {}
    };
    typedef point_base<int>    point_i; //-----point_i
    typedef point_base<float>  point_f; //-----point_f
    typedef point_base<double> point_d; //-----point_d

    //-------------------------------------------------------------vertex_base
    template<class T> struct vertex_base
    {
        typedef T value_type;
        T x,y;
        unsigned cmd;
        vertex_base() {}
        vertex_base(T x_, T y_, unsigned cmd_) : x(x_), y(y_), cmd(cmd_) {}
    };
    typedef vertex_base<int>    vertex_i; //-----vertex_i
    typedef vertex_base<float>  vertex_f; //-----vertex_f
    typedef vertex_base<double> vertex_d; //-----vertex_d

    //----------------------------------------------------------------row_info
    template<class T> struct row_info
    {
        int x1, x2;
        T* ptr;
        row_info() {}
        row_info(int x1_, int x2_, T* ptr_) : x1(x1_), x2(x2_), ptr(ptr_) {}
    };

    //----------------------------------------------------------const_row_info
    template<class T> struct const_row_info
    {
        int x1, x2;
        const T* ptr;
        const_row_info() {}
        const_row_info(int x1_, int x2_, const T* ptr_) :
            x1(x1_), x2(x2_), ptr(ptr_) {}
    };

    //------------------------------------------------------------is_equal_eps
    template<class T> inline bool is_equal_eps(T v1, T v2, T epsilon)
    {
        return fabs(v1 - v2) <= double(epsilon);
    }

}


#endif

