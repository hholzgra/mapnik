/*****************************************************************************
 *
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2013 Artem Pavlenko
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *****************************************************************************/

#if defined(HAVE_SKIA)

#include <mapnik/skia/skia_renderer.hpp>
#include <mapnik/vertex_converters.hpp>
#include <mapnik/symbolizer_helpers.hpp>
// skia
#include <SkCanvas.h>
#include <SkPath.h>
#include <SkPaint.h>
#include <SkDashPathEffect.h>
#include "agg_trans_affine.h"

namespace mapnik {


void set_comp_op(SkPaint & paint, composite_mode_e comp_op)
{
    SkXfermode::Mode mode = SkXfermode::kSrcOver_Mode;
    switch (comp_op)
    {
    case clear:
        mode = SkXfermode::kClear_Mode;
        break;
    case src:
        mode = SkXfermode::kSrc_Mode;
        break;
    case dst:
        mode = SkXfermode::kDst_Mode;
        break;
    case src_over:
        mode = SkXfermode::kSrcOver_Mode;
        break;
    case dst_over:
        mode = SkXfermode::kDstOver_Mode;
        break;
    case src_in:
        mode = SkXfermode::kSrcIn_Mode;
        break;
    case dst_in:
        mode = SkXfermode::kDstIn_Mode;
        break;
    case src_out:
        mode = SkXfermode::kSrcOut_Mode;
        break;
    case dst_out:
        mode = SkXfermode::kDstOut_Mode;
        break;
    case src_atop:
        mode = SkXfermode::kSrcATop_Mode;
        break;
    case dst_atop:
        mode = SkXfermode::kDstATop_Mode;
        break;
    case _xor:
        mode = SkXfermode::kXor_Mode;
        break;
    case plus:
        mode = SkXfermode::kPlus_Mode;
        break;
    case multiply:
        mode = SkXfermode::kMultiply_Mode;
        break;
    case screen:
        mode = SkXfermode::kScreen_Mode;
        break;
    case overlay:
        mode = SkXfermode::kOverlay_Mode;
        break;
    case darken:
        mode = SkXfermode::kDarken_Mode;
        break;
    case lighten:
        mode = SkXfermode::kLighten_Mode;
        break;
    case color_dodge:
        mode = SkXfermode::kColorDodge_Mode;
        break;
    case color_burn:
        mode = SkXfermode::kColorBurn_Mode;
        break;
    case hard_light:
        mode = SkXfermode::kHardLight_Mode;
        break;
    case soft_light:
        mode = SkXfermode::kSoftLight_Mode;
        break;
    case difference:
        mode = SkXfermode::kDifference_Mode;
        break;
    case exclusion:
        mode = SkXfermode::kExclusion_Mode;
        break;
    case hue:
        mode = SkXfermode::kHue_Mode;
        break;
    case saturation:
        mode = SkXfermode::kSaturation_Mode;
        break;
    case _color:
        mode = SkXfermode::kColor_Mode;
        break;
    default:
        break;
    }

    paint.setXfermodeMode(mode);
}

struct skia_path_adapter : private mapnik::noncopyable
{
    skia_path_adapter(SkPath & path)
        : sk_path_(path) {}

    template <typename T>
    void add_path(T & path)
    {
        vertex2d vtx(vertex2d::no_init);
        path.rewind(0);
        while ((vtx.cmd = path.vertex(&vtx.x, &vtx.y)) != SEG_END)
        {
            switch (vtx.cmd)
            {
            case SEG_MOVETO:
                sk_path_.moveTo(vtx.x, vtx.y);
            case SEG_LINETO:
                sk_path_.lineTo(vtx.x, vtx.y);
            }
        }
    }

    SkPath & sk_path_;
};

skia_renderer::skia_renderer(Map const& map, SkCanvas & canvas, double scale_factor)
    : feature_style_processor<skia_renderer>(map,scale_factor),
      canvas_(canvas),
      width_(map.width()),
      height_(map.height()),
      t_(map.width(), map.height(), map.get_current_extent()),
      scale_factor_(scale_factor),
      typeface_cache_(),
      //font_manager_(typeface_cache_),
      font_engine_(),
      font_manager_(font_engine_),
      detector_(boost::make_shared<label_collision_detector4>(
                    box2d<double>(-map.buffer_size(), -map.buffer_size(),
                                  map.width() + map.buffer_size(), map.height() + map.buffer_size()))) {}

skia_renderer::~skia_renderer() {}

void skia_renderer::start_map_processing(Map const& map)
{
    MAPNIK_LOG_DEBUG(skia_renderer) << "skia_renderer_base: Start map processing bbox=" << map.get_current_extent();
    boost::optional<color> bg = map.background();
    if (bg)
    {
        canvas_.drawARGB((*bg).alpha(), (*bg).red(), (*bg).green(), (*bg).blue());
    }
}

void skia_renderer::end_map_processing(Map const& map)
{
    MAPNIK_LOG_DEBUG(skia_renderer) << "skia_renderer_base: End map processing";
}

void skia_renderer::start_layer_processing(layer const& lay, box2d<double> const& query_extent)
{
    MAPNIK_LOG_DEBUG(skia_renderer) << "skia_renderer_base: Start processing layer=" << lay.name() ;
    MAPNIK_LOG_DEBUG(skia_renderer) << "skia_renderer_base: -- datasource=" << lay.datasource().get();
    MAPNIK_LOG_DEBUG(skia_renderer) << "skia_renderer_base: -- query_extent=" << query_extent;
    query_extent_ = query_extent;
}
void skia_renderer::end_layer_processing(layer const& lay)
{
    MAPNIK_LOG_DEBUG(skia_renderer) << "skia_renderer_base: End layer processing";
}
void skia_renderer::start_style_processing(feature_type_style const& st)
{
    MAPNIK_LOG_DEBUG(skia_renderer) << "skia_renderer:start style processing";
}
void skia_renderer::end_style_processing(feature_type_style const& st)
{
    MAPNIK_LOG_DEBUG(skia_renderer) << "skia_renderer:end style processing";
}

void skia_renderer::process(line_symbolizer const& sym,
                            mapnik::feature_impl & feature,
                            proj_transform const& prj_trans)
{
     agg::trans_affine tr;
     evaluate_transform(tr, feature, sym.get_transform());

     box2d<double> clipping_extent = query_extent_;
     SkPath path;
     skia_path_adapter adapter(path);

     typedef boost::mpl::vector<clip_line_tag,transform_tag,affine_transform_tag,simplify_tag,smooth_tag> conv_types;
     vertex_converter<box2d<double>, skia_path_adapter, line_symbolizer,
                      CoordTransform, proj_transform, agg::trans_affine, conv_types>
         converter(clipping_extent,adapter,sym,t_,prj_trans,tr,scale_factor_);

     if (prj_trans.equal() && sym.clip()) converter.template set<clip_line_tag>(); //optional clip (default: true)
     converter.template set<transform_tag>(); //always transform
     converter.template set<affine_transform_tag>();
     if (sym.simplify_tolerance() > 0.0) converter.template set<simplify_tag>(); // optional simplify converter
     if (sym.smooth() > 0.0) converter.template set<smooth_tag>(); // optional smooth converter

     BOOST_FOREACH( geometry_type & geom, feature.paths())
     {
         if (geom.size() > 1)
         {
             converter.apply(geom);
         }
     }

     stroke const& strk = sym.get_stroke();
     color const& col = strk.get_color();
     SkPaint paint;
     paint.setStyle(SkPaint::kStroke_Style);
     paint.setAntiAlias(true);
     set_comp_op(paint, sym.comp_op());
     paint.setARGB(int(col.alpha() * strk.get_opacity()), col.red(), col.green(), col.blue());
     paint.setStrokeWidth(strk.get_width() * scale_factor_);

     switch (strk.get_line_join())
     {
     case MITER_JOIN:
         paint.setStrokeJoin(SkPaint::kMiter_Join);
         break;
     case MITER_REVERT_JOIN:
         break;
     case ROUND_JOIN:
         paint.setStrokeJoin(SkPaint::kRound_Join);
         break;
     case BEVEL_JOIN:
         paint.setStrokeJoin(SkPaint::kBevel_Join);
         break;
     default:
         break;
     }

     switch (strk.get_line_cap())
     {
     case BUTT_CAP:
         paint.setStrokeCap(SkPaint::kButt_Cap);
         break;
     case SQUARE_CAP:
         paint.setStrokeCap(SkPaint::kSquare_Cap);
         break;
     case ROUND_CAP:
         paint.setStrokeCap(SkPaint::kRound_Cap);
         break;
     default:
         break;
     }

     if (strk.has_dash())
     {
         std::vector<SkScalar> dash;
         for (auto p : strk.get_dash_array())
         {
             dash.push_back(p.first * scale_factor_);
             dash.push_back(p.second * scale_factor_);
         }

         SkDashPathEffect dash_effect(&dash[0], dash.size(), strk.dash_offset(), false);
         paint.setPathEffect(&dash_effect);
         canvas_.drawPath(path, paint);
     }
     else
     {
         canvas_.drawPath(path, paint);
     }
}

void skia_renderer::process(polygon_symbolizer const& sym,
                            mapnik::feature_impl & feature,
                            proj_transform const& prj_trans)
{

    agg::trans_affine tr;
    evaluate_transform(tr, feature, sym.get_transform());

    SkPath path;
    skia_path_adapter adapter(path);

    typedef boost::mpl::vector<clip_poly_tag,transform_tag,affine_transform_tag,simplify_tag,smooth_tag> conv_types;
    vertex_converter<box2d<double>, skia_path_adapter, polygon_symbolizer,
                     CoordTransform, proj_transform, agg::trans_affine, conv_types>
        converter(query_extent_, adapter ,sym,t_,prj_trans,tr,scale_factor_);

    if (prj_trans.equal() && sym.clip()) converter.template set<clip_poly_tag>(); //optional clip (default: true)
    converter.template set<transform_tag>(); //always transform
    converter.template set<affine_transform_tag>();
    if (sym.simplify_tolerance() > 0.0) converter.template set<simplify_tag>(); // optional simplify converter
    if (sym.smooth() > 0.0) converter.template set<smooth_tag>(); // optional smooth converter
    BOOST_FOREACH( geometry_type & geom, feature.paths())
    {
        if (geom.size() > 2)
        {
            converter.apply(geom);
        }
    }

    color const& fill = sym.get_fill();
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setAntiAlias(true);
    set_comp_op(paint, sym.comp_op());
    paint.setARGB(int(fill.alpha() * sym.get_opacity()), fill.red(), fill.green(), fill.blue());
    canvas_.drawPath(path, paint);
}

void skia_renderer::process(text_symbolizer const& sym,
                            mapnik::feature_impl & feature,
                            proj_transform const& prj_trans)
{
    text_symbolizer_helper<face_manager<freetype_engine>,label_collision_detector4>
        //text_symbolizer_helper<skia_font_manager,label_collision_detector4>
        helper(sym, feature, prj_trans,
               width_, height_,
               scale_factor_,
               t_, font_manager_, *detector_, query_extent_);

    while (helper.next())
    {
        placements_type const& placements = helper.placements();

        for (unsigned i = 0; i < placements.size(); ++i)
        {
            double sx = placements[i].center.x;
            double sy = placements[i].center.y;

            placements[i].rewind();
            SkPaint paint;
            paint.setAntiAlias(true);
            set_comp_op(paint, sym.comp_op());
            SkTypeface * typeface = typeface_cache_.create("ArialUnicodeMS"); // FIXME
            if (typeface) paint.setTypeface(typeface);
            paint.setTextEncoding(SkPaint::kUTF32_TextEncoding);

            // halo
            for (int j = 0; j < placements[i].num_nodes(); ++j)
            {
                char_info_ptr c;
                double x, y, angle;
                placements[i].vertex(c, x, y, angle);
                if (c->format->halo_radius > 0)
                {
                    paint.setStyle(SkPaint::kStroke_Style);
                    double text_size = c->format->text_size * scale_factor_;
                    paint.setTextSize((SkScalar)text_size);
                    paint.setStrokeWidth(2.0 * c->format->halo_radius * scale_factor_);
                    paint.setStrokeJoin(SkPaint::kRound_Join);
                    color const& halo_fill = c->format->halo_fill;
                    paint.setARGB(int(halo_fill.alpha() * c->format->text_opacity), halo_fill.red(), halo_fill.green(), halo_fill.blue());
                    SkPoint pt = SkPoint::Make(0,0);
                    canvas_.save();
                    canvas_.translate((SkScalar)(sx + x), (SkScalar)(sy - y));
                    canvas_.rotate(-(SkScalar)180 * (angle/M_PI));
                    canvas_.drawPosText(&(c->c),1, &pt, paint);
                    canvas_.restore();
                }
            }

            // text
            placements[i].rewind();
            for (int j = 0; j < placements[i].num_nodes(); ++j)
            {
                char_info_ptr c;
                double x, y, angle;
                placements[i].vertex(c, x, y, angle);
                paint.setStyle(SkPaint::kFill_Style);
                double text_size = c->format->text_size * scale_factor_;
                paint.setTextSize((SkScalar)text_size);
                color const& fill = c->format->fill;
                paint.setARGB(int(fill.alpha() * c->format->text_opacity), fill.red(), fill.green(), fill.blue());
                SkPoint pt = SkPoint::Make(0,0);
                canvas_.save();
                canvas_.translate((SkScalar)(sx + x), (SkScalar)(sy - y));
                canvas_.rotate(-(SkScalar)180 * (angle/M_PI));
                canvas_.drawPosText(&(c->c),1, &pt, paint);
                canvas_.restore();
            }
        }
    }
}

}

#endif // HAVE_SKIA
