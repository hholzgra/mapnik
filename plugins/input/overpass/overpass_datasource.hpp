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

#ifndef OVERPASS_DATASOURCE_HPP
#define OVERPASS_DATASOURCE_HPP

#include <mapnik/datasource.hpp>
#include <mapnik/datasource_plugin.hpp>
#include <mapnik/feature.hpp>
#include <mapnik/feature_layer_desc.hpp>
#include <mapnik/geometry/box2d.hpp>
#include <mapnik/params.hpp>
#include <mapnik/query.hpp>
#include <mapnik/unicode.hpp>
#include <mapnik/value/types.hpp>

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

DATASOURCE_PLUGIN_DEF(overpass_datasource_plugin, overpass);

// Datasource issuing Overpass QL queries against an Overpass API server
// (https://wiki.openstreetmap.org/wiki/Overpass_API) per render request,
// with the render extent substituted for the {{bbox}} placeholder (the
// overpass-turbo convention: expands to "south,west,north,east").
//
// The query must end in "out geom;" (or "out center;") so every returned
// element carries its own coordinates -- the plugin does not resolve OSM
// topology (node references) itself.
//
// Layers using this datasource should declare srs=+proj=longlat (WGS84) so
// the bbox arrives in the lat/lon values Overpass expects.
class overpass_datasource : public mapnik::datasource
{
  public:
    using features_ptr = std::shared_ptr<std::vector<mapnik::feature_ptr> const>;

    overpass_datasource(mapnik::parameters const& params);
    virtual ~overpass_datasource();

    mapnik::datasource::datasource_t type() const override;
    static char const* name();
    mapnik::featureset_ptr features(mapnik::query const& q) const override;
    mapnik::featureset_ptr features_at_point(mapnik::coord2d const& pt, double tol = 0) const override;
    mapnik::box2d<double> envelope() const override;
    std::optional<mapnik::datasource_geometry_t> get_geometry_type() const override;
    mapnik::layer_descriptor get_descriptor() const override;

  private:
    // {{bbox}} -> "south,west,north,east" for the given render extent
    std::string bind_query(mapnik::box2d<double> const& bbox) const;
    // POST the query to the configured server (or read response_file) and
    // return the raw OSM XML response body
    std::string fetch(std::string const& ql) const;
    std::string http_post(std::string const& ql) const;
    // OSM XML (with per-element geometry) -> mapnik features
    features_ptr parse_osm_xml(std::string&& body) const;

    std::string url_;
    std::string query_;
    std::string user_agent_;
    std::optional<std::string> response_file_; // offline testing / debugging
    mapnik::value_integer http_timeout_;       // seconds
    mapnik::box2d<double> extent_;
    mapnik::transcoder tr_;

    // one response cache entry per bound query -- a single render calls
    // features() once per attached style, always with the same extent
    mutable std::mutex cache_mutex_;
    mutable std::map<std::string, features_ptr> cache_;

    mutable mapnik::layer_descriptor desc_;
};

#endif // OVERPASS_DATASOURCE_HPP
