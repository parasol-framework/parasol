// For Anti-Grain Geometry - Version 2.4
// http://www.antigrain.org
//
// Contribution Created By:
//  Milan Marusinec alias Milano
//  milan@marusinec.sk
//  Copyright (c) 2007-2008
//
// Permission to copy, use, modify, sell and distribute this software
// is granted provided this copyright notice appears in all copies.
// This software is provided "as is" without express or implied
// warranty, and with no claim as to its suitability for any purpose.

#ifndef AGG_SPAN_GRADIENT_CONTOUR_INCLUDED
#define AGG_SPAN_GRADIENT_CONTOUR_INCLUDED

#include "agg_basics.h"
#include "agg_trans_affine.h"
#include "agg_path_storage.h"
#include "agg_pixfmt_gray.h"
#include "agg_conv_transform.h"
#include "agg_conv_curve.h"
#include "agg_bounding_rect.h"
#include "agg_renderer_base.h"
#include "agg_renderer_primitives.h"
#include "agg_rasterizer_outline.h"
#include "agg_span_gradient.h"

constexpr double infinity = 1E20;

namespace agg
{
	class gradient_contour {
	private:
		std::vector<int8u> m_buffer;
		int m_width, m_height;
		double m_d1; // Ranges from 0 to 254
		double m_d2; // Ranges from 0.001 to 1.0

	public:
		gradient_contour() : m_width(0), m_height(0), m_d1(0), m_d2(1.0) { }
		gradient_contour(double d1, double d2) : m_width(0), m_height(0), m_d2(d2) { 
			m_d1 = (d1 > 254) ? 254 : d1;
		}

		int8u* contour_create(path_storage &ps);

		int    contour_width() { return m_width; }
		int    contour_height() { return m_height; }

		void   d1(double d) { m_d1 = d; }
		void   d2(double d) { m_d2 = d; }

		int calculate(int x, int y, int d) const {
			if (!m_buffer.empty()) {
				int px = (x >> agg::gradient_subpixel_shift) % m_width;
				int py = (y >> agg::gradient_subpixel_shift) % m_height;

				if (px < 0) px += m_width;
				if (py < 0) py += m_height;
				return iround((m_buffer[(py * m_width) + px] * m_d2) + m_d1) << gradient_subpixel_shift;
			}
			else return 0;
		}
	};

	static constexpr int square(int x) { return x * x; }

	// Distance transform algorithm by: Pedro Felzenszwalb
	void dt(std::vector<double> &spanf, std::vector<double> &spang, std::vector<double> &spanr, std::vector<int> &spann, int length)
	{
		spann[0] = 0;
		spang[0] = -infinity;
		spang[1] = +infinity;

		int k = 0;
		for (int q = 1; q <= length - 1; q++) {
			double s = ((spanf[q ] + square(q)) - (spanf[spann[k]] + square(spann[k]))) / (2 * q - 2 * spann[k]);

			while (s <= spang[k ]) {
				k--;
				s = ((spanf[q] + square(q)) - (spanf[spann[k]] + square(spann[k]))) / (2 * q - 2 * spann[k]);
			}

			k++;
			spann[k] = q;
			spang[k] = s;
			spang[k + 1] = +infinity;
		}

		int j = 0;
		for (int q = 0; q <= length - 1; q++) {
			while (spang[j + 1] < q) j++;
			spanr[q] = square(q - spann[j]) + spanf[spann[j]];
		}
	}

	// Distance transform algorithm by: Pedro Felzenszwalb
	int8u* gradient_contour::contour_create(path_storage &ps) {
		// I. Render Black And White NonAA Stroke of the Path
		// Path Bounding Box + Some Frame Space Around [configurable]
		agg::conv_curve<agg::path_storage> conv(ps);

		double x1, y1, x2, y2;
		if (agg::bounding_rect_single(conv, 0, &x1, &y1, &x2, &y2)) {
			// Create BW Rendering Surface
			auto width  = int(ceil(x2 - x1)) + 1;
			auto height = int(ceil(y2 - y1)) + 1;

			m_buffer.resize(width * height);
         std::fill(m_buffer.begin(), m_buffer.end(), 255);

			// Setup VG Engine & Render
			agg::rendering_buffer rb;
			rb.attach(m_buffer.data(), width, height, width);

			agg::pixfmt_gray8 pf(rb);
			agg::renderer_base<agg::pixfmt_gray8> renb(pf);
			agg::renderer_primitives<agg::renderer_base<agg::pixfmt_gray8>> prim(renb);
			agg::rasterizer_outline<renderer_primitives<agg::renderer_base<agg::pixfmt_gray8>>> ras(prim);

			agg::trans_affine mtx;
			mtx.translate(-x1, -y1);

			agg::conv_transform<agg::conv_curve<agg::path_storage>> trans(conv, mtx);

			prim.line_color(agg::rgba8(0,0,0,255));
			ras.add_path(trans);

			// II. Distance Transform
			// Create Float Buffer + 0 vs infinity (1e20) assignment
			std::vector<double> image(width * height);

			for (int y=0, l=0; y < height; y++) {
				for (int x=0; x < width; x++, l++) {
					if (m_buffer[l] == 0) image[l] = 0.0;
					else image[l] = infinity;
				}
			}

			// DT of 2d
			// SubBuff<double> max width,height
			int length = width;

			if (height > length) length = height;

			std::vector<double> spanf(length);
			std::vector<double> spang(length + 1);
			std::vector<double> spanr(length);
			std::vector<int> spann(length);

			// Transform along columns
			for (int x = 0; x < width; x++) {
				for (int y = 0; y < height; y++) spanf[y] = image[y * width + x];

				// DT of 1d
				dt(spanf, spang, spanr, spann, height);

				for (int y=0; y < height; y++) image[y * width + x] = spanr[y];
			}

			// Transform along rows
			for (int y=0; y < height; y++) {
				for (int x=0; x < width; x++) spanf[x] = image[y * width + x];

				// DT of 1d
				dt(spanf, spang, spanr, spann, width);

				for (int x=0; x < width; x++) image[y * width + x] = spanr[x];
			}

			// Take Square Roots, Min & Max
			double min = sqrt(image[0]);
			double max = min;

			for (int y=0, l=0; y < height; y++) {
				for (int x=0; x < width; x++, l++) {
					image[l] = sqrt(image[l]);
					if (min > image[l]) min = image[l];
					if (max < image[l]) max = image[l];
				}
			}

			// III. Convert To Grayscale
			if (min == max) m_buffer.clear();
         else {
				double scale = 255.0 / (max - min);
				for (int y=0, l=0; y < height; y++) {
					for (int x=0; x < width; x++, l++) m_buffer[l] = int8u(int((image[l] - min) * scale));
				}
			}

			m_width  = width;
			m_height = height;
		}

		return m_buffer.data();
	}
}

#endif