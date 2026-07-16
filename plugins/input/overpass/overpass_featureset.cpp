/*****************************************************************************
 *
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2026 Artem Pavlenko
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

#include "overpass_featureset.hpp"

overpass_featureset::overpass_featureset(features_ptr features, mapnik::box2d<double> const& bbox)
    : features_(std::move(features)),
      bbox_(bbox),
      pos_(0)
{}

overpass_featureset::~overpass_featureset() {}

mapnik::feature_ptr overpass_featureset::next()
{
    while (pos_ < features_->size())
    {
        mapnik::feature_ptr const& f = (*features_)[pos_++];
        if (bbox_.intersects(f->envelope()))
        {
            return f;
        }
    }
    return mapnik::feature_ptr();
}
