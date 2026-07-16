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

#ifndef OVERPASS_FEATURESET_HPP
#define OVERPASS_FEATURESET_HPP

#include <mapnik/featureset.hpp>
#include <mapnik/feature.hpp>
#include <mapnik/geometry/box2d.hpp>

#include <memory>
#include <vector>

// Serves the features parsed from one Overpass API response, filtered by the
// query bbox. The feature vector is shared with the datasource's response
// cache, so repeated featuresets over the same response carry no copy cost.
class overpass_featureset : public mapnik::Featureset
{
  public:
    using features_ptr = std::shared_ptr<std::vector<mapnik::feature_ptr> const>;

    overpass_featureset(features_ptr features, mapnik::box2d<double> const& bbox);
    virtual ~overpass_featureset();
    mapnik::feature_ptr next() override;

  private:
    features_ptr features_;
    mapnik::box2d<double> bbox_;
    std::size_t pos_;
};

#endif // OVERPASS_FEATURESET_HPP
