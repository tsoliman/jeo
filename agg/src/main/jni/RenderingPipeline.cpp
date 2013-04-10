#include <jni.h>
#include <cmath>
#include "agg_array.h"
#include "agg_rendering_buffer.h"
#include "agg_pixfmt_rgb.h"
#include "agg_color_rgba.h"
#include "agg_basics.h"
#include "agg_renderer_base.h"
#include "agg_renderer_scanline.h"
#include "agg_renderer_markers.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_scanline_p.h"
#include "agg_scanline_u.h"
#include "agg_path_storage.h"
#include "agg_conv_curve.h"
#include "agg_conv_stroke.h"
#include "agg_conv_dash.h"
#include "agg_conv_transform.h"
#include "RenderingPipeline.h"
#include "VertexSource.h"

typedef agg::rgba8 ColorType;
typedef agg::order_rgba OrderType;
typedef agg::pixel32_type PixelType;
typedef agg::comp_op_adaptor_rgba_pre<ColorType, OrderType> BlenderType;
typedef agg::pixfmt_custom_blend_rgba<BlenderType, agg::rendering_buffer> PixfmtType;
  //typedef agg::pixfmt_rgba32 PixfmtType;
typedef agg::renderer_base<PixfmtType> RendererType;

  
void set_gamma(
    agg::rasterizer_scanline_aa<> *ras, std::string method, float g) {
  
  if (method.empty()) {
    method = "power";
  }

  if (method.compare("linear") == 0) {
    ras->gamma(g > -1 ? agg::gamma_power(g) : agg::gamma_power());
  }
  else if (method.compare("threshold") == 0) {
    ras->gamma(g > -1 ? agg::gamma_threshold(g) : agg::gamma_threshold());
  }
  else if (method.compare("multiply") == 0) {
    ras->gamma(g > -1 ? agg::gamma_multiply(g) : agg::gamma_multiply());
  }
  else if (method.compare("none") == 0) {
    ras->gamma(agg::gamma_none());
  }
  else {
    ras->gamma(g > -1 ? agg::gamma_power(g) : agg::gamma_power());
  }
}

template<class V> RenderingPipeline<V>::RenderingPipeline()
   : at(agg::trans_affine()) {
}

template<class V> void RenderingPipeline<V>::set_transform(
    double scx, double scy, double tx, double ty) {
  at = agg::trans_affine(scx, 0, 0, scy, tx, ty);
}

template<class V> void RenderingPipeline<V>::fill_path(V *path, char *p) {

  while (*p != agg::path_cmd_stop) {
    switch(*p++) {
      case agg::path_cmd_move_to:
        path->move_to(*((float*)p), *((float*)(p+4))); 
        p += 8;
        break;

      case agg::path_cmd_line_to:
        path->line_to(*((float*)p), *((float*)(p+4))); 
        p += 8;
        break;

      case agg::path_cmd_end_poly:
        path->close_polygon(); 
        break;

      case agg::path_cmd_stop:
        return;
    }
  }
}

template<class V> void RenderingPipeline<V>::draw_point(
    V *point, const PointStyle &style, RenderingBuffer *rb) {

  PixfmtType pixf(rb->rbuf);
  pixf.comp_op(style.comp_op);

  agg::renderer_base<PixfmtType> renderer_base(pixf);
  agg::renderer_markers<RendererType> renderer(renderer_base);
  renderer.fill_color(style.color);

  if (style.line) {
    renderer.line_color(style.line->color);
  }
  agg::conv_transform<V> tx_point(*point, at);

  int r = (int)sqrt(style.width*style.width+style.height*style.height)*0.5;
  double x;
  double y;

  unsigned cmd;
  while (!agg::is_stop(tx_point.vertex(&x, &y))) {
     renderer.marker((int)x, (int)y, r, style.marker);
  }
}

template<class V> void RenderingPipeline<V>::draw_line(
    V *line, const LineStyle &style, RenderingBuffer *rb) {

  PixfmtType pixf(rb->rbuf);
  pixf.comp_op(style.comp_op);

  agg::renderer_base<PixfmtType> renderer_base(pixf);
  agg::renderer_scanline_aa_solid<RendererType> renderer(renderer_base);
  renderer.color(style.color);

  // create the rasterizer
  agg::rasterizer_scanline_aa<> rasterizer;
  set_gamma(&rasterizer, style.gamma_method, style.gamma);
  rasterizer.reset();

  if (style.dash_array.size() > 0) {
    //dash the line
    typedef agg::conv_dash<V> DashType;
    DashType dash_stroke(*line);

    std::list<double>::const_iterator it = style.dash_array.begin();
    while(it != style.dash_array.end()) {
      double dash_len = *it++;
      double gap_len = *it++;
      dash_stroke.add_dash(dash_len, gap_len);
    }

    typedef agg::conv_transform<DashType> TransStrokeType;
    TransStrokeType tx_stroke(dash_stroke, at);

    typedef agg::conv_stroke<TransStrokeType> StrokeType;
    StrokeType stroke(tx_stroke);
    stroke.width(style.width);
    rasterizer.add_path(stroke);
  }
  else {
    typedef agg::conv_transform<V> TransStrokeType;
    TransStrokeType tx_stroke(*line, at);

    typedef agg::conv_stroke<TransStrokeType> StrokeType;
    StrokeType stroke(tx_stroke);
    stroke.width(style.width);
    rasterizer.add_path(stroke);
  }

  agg::scanline_p8 scanline;
  agg::render_scanlines(rasterizer, scanline, renderer);
}


template<class V> void RenderingPipeline<V>::draw_polygon(
    V *poly, const PolyStyle &style, RenderingBuffer *rb) {

  PixfmtType pixf(rb->rbuf);
  pixf.comp_op(style.comp_op);

  agg::renderer_base<PixfmtType> renderer_base(pixf);

  agg::rasterizer_scanline_aa<> rasterizer;
  set_gamma(&rasterizer, style.gamma_method, style.gamma);
  rasterizer.reset();

  //typedef agg::conv_transform<V> PolyType;
  //PolyType tx_poly(poly, at);
  typedef agg::conv_transform<agg::path_storage> PolyType;
  PolyType tx_poly(*poly, at);

  agg::renderer_scanline_aa_solid<RendererType> renderer(renderer_base);
  agg::scanline_p8 scanline;

  typedef agg::conv_curve<PolyType> CurveType;
  typedef agg::conv_stroke<CurveType> StrokeType;

  CurveType curve(tx_poly);
  StrokeType stroke(curve);

  if (style.fill_color) {
    rasterizer.add_path(curve);
    renderer.color(*style.fill_color);
    agg::render_scanlines(rasterizer, scanline, renderer);
  }

  if (style.line) {
    stroke.width(style.line->width);

    rasterizer.add_path(stroke);
    renderer.color(style.line->color);
    agg::render_scanlines(rasterizer, scanline, renderer);
  }
}

template class RenderingPipeline<agg::path_storage>;