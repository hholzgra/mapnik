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

#include "overpass_datasource.hpp"
#include "overpass_featureset.hpp"

// mapnik
#include <mapnik/debug.hpp>
#include <mapnik/feature_factory.hpp>
#include <mapnik/geometry.hpp>
#include <mapnik/util/fs.hpp>

#include <mapnik/warning.hpp>
MAPNIK_DISABLE_WARNING_PUSH
#include <mapnik/warning_ignore.hpp>
#include <boost/property_tree/detail/rapidxml.hpp>
MAPNIK_DISABLE_WARNING_POP

// curl
#include <curl/curl.h>

// stl
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <utility>

namespace rapidxml = boost::property_tree::detail::rapidxml;

DATASOURCE_PLUGIN_IMPL(overpass_datasource_plugin, overpass_datasource);
DATASOURCE_PLUGIN_EXPORT(overpass);

// libcurl requires one-time global setup before any easy handle is created;
// the plugin load/unload hooks are the only place this can be done safely.
void overpass_datasource_plugin::after_load() const
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}
void overpass_datasource_plugin::before_unload() const
{
    curl_global_cleanup();
}

overpass_datasource::overpass_datasource(mapnik::parameters const& params)
    : datasource(params),
      url_(*params.get<std::string>("url", "https://overpass-api.de/api/interpreter")),
      user_agent_(*params.get<std::string>("user-agent", "mapnik-overpass-input/0.1")),
      response_file_(params.get<std::string>("response_file")),
      http_timeout_(*params.get<mapnik::value_integer>("timeout", 30)),
      extent_(-180, -90, 180, 90),
      tr_(*params.get<std::string>("encoding", "utf-8")),
      desc_(overpass_datasource::name(), *params.get<std::string>("encoding", "utf-8"))
{
    auto query = params.get<std::string>("query");
    if (!query)
        throw mapnik::datasource_exception("Overpass Plugin: missing <Parameter name=\"query\"> (Overpass QL)");
    query_ = *query;

    auto extent = params.get<std::string>("extent");
    if (extent && !extent_.from_string(*extent))
        throw mapnik::datasource_exception("Overpass Plugin: invalid <Parameter name=\"extent\">: " + *extent);

    // always present, independent of the freeform OSM tags
    desc_.add_descriptor(mapnik::attribute_descriptor("osm_id", mapnik::Integer));
    desc_.add_descriptor(mapnik::attribute_descriptor("osm_type", mapnik::String));
}

overpass_datasource::~overpass_datasource() {}

char const* overpass_datasource::name()
{
    return "overpass";
}

mapnik::datasource::datasource_t overpass_datasource::type() const
{
    return datasource::Vector;
}

mapnik::box2d<double> overpass_datasource::envelope() const
{
    return extent_;
}

std::optional<mapnik::datasource_geometry_t> overpass_datasource::get_geometry_type() const
{
    // nodes, ways and areas may all occur in one response
    return mapnik::datasource_geometry_t::Collection;
}

mapnik::layer_descriptor overpass_datasource::get_descriptor() const
{
    return desc_;
}

std::string overpass_datasource::bind_query(mapnik::box2d<double> const& bbox) const
{
    // overpass-turbo convention: {{bbox}} = "south,west,north,east"
    std::ostringstream ss;
    ss.precision(10);
    ss << bbox.miny() << ',' << bbox.minx() << ',' << bbox.maxy() << ',' << bbox.maxx();
    std::string const bbox_str = ss.str();

    std::string ql = query_;
    std::string const token = "{{bbox}}";
    for (std::size_t pos = ql.find(token); pos != std::string::npos; pos = ql.find(token, pos))
    {
        ql.replace(pos, token.size(), bbox_str);
        pos += bbox_str.size();
    }
    return ql;
}

std::string overpass_datasource::fetch(std::string const& ql) const
{
    if (response_file_)
    {
        // offline testing: serve a canned response instead of going online
        std::ifstream in(*response_file_, std::ios::binary);
        if (!in)
            throw mapnik::datasource_exception("Overpass Plugin: cannot read response_file: " + *response_file_);
        std::ostringstream body;
        body << in.rdbuf();
        return body.str();
    }
    return http_post(ql);
}

namespace {

std::size_t append_to_string(char* ptr, std::size_t size, std::size_t nmemb, void* userdata)
{
    static_cast<std::string*>(userdata)->append(ptr, size * nmemb);
    return size * nmemb;
}

} // namespace

std::string overpass_datasource::http_post(std::string const& ql) const
{
    CURL* curl = curl_easy_init();
    if (!curl)
        throw mapnik::datasource_exception("Overpass Plugin: curl_easy_init failed");

    char* escaped = curl_easy_escape(curl, ql.c_str(), static_cast<int>(ql.size()));
    std::string body = "data=";
    body += escaped;
    curl_free(escaped);

    std::string response;
    char errbuf[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(curl, CURLOPT_URL, url_.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, append_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent_.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(http_timeout_));
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, ""); // enable compression

    CURLcode rc = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK)
        throw mapnik::datasource_exception("Overpass Plugin: request to " + url_ + " failed: " + errbuf);
    if (status != 200)
    {
        // Overpass sends its diagnostics (rate limit, timeout, QL errors) as
        // an HTML body -- pass a snippet along, it is usually the answer
        throw mapnik::datasource_exception("Overpass Plugin: server returned HTTP " + std::to_string(status) + ": " +
                                           response.substr(0, 500));
    }
    return response;
}

namespace {

double attr_double(rapidxml::xml_node<>* node, char const* name)
{
    if (auto* attr = node->first_attribute(name))
        return std::strtod(attr->value(), nullptr);
    throw mapnik::datasource_exception(std::string("Overpass Plugin: element without '") + name +
                                       "' attribute - query must use 'out geom;'");
}

mapnik::value_integer attr_integer(rapidxml::xml_node<>* node, char const* name)
{
    if (auto* attr = node->first_attribute(name))
        return static_cast<mapnik::value_integer>(std::strtoll(attr->value(), nullptr, 10));
    return 0;
}

} // namespace

overpass_datasource::features_ptr overpass_datasource::parse_osm_xml(std::string&& body) const
{
    auto features = std::make_shared<std::vector<mapnik::feature_ptr>>();
    auto ctx = std::make_shared<mapnik::context_type>();
    std::set<std::string> tag_keys;
    mapnik::value_integer feature_id = 1;

    rapidxml::xml_document<> doc;
    try
    {
        // rapidxml parses in situ and needs a mutable, NUL-terminated buffer
        doc.parse<rapidxml::parse_default>(body.data());
    }
    catch (rapidxml::parse_error const& ex)
    {
        throw mapnik::datasource_exception(std::string("Overpass Plugin: malformed response: ") + ex.what());
    }

    rapidxml::xml_node<>* osm = doc.first_node("osm");
    if (!osm)
        throw mapnik::datasource_exception("Overpass Plugin: response is not OSM XML (missing <osm> root)");

    for (rapidxml::xml_node<>* el = osm->first_node(); el; el = el->next_sibling())
    {
        std::string const el_name(el->name(), el->name_size());
        mapnik::geometry::geometry<double> geom;

        if (el_name == "node")
        {
            geom = mapnik::geometry::point<double>(attr_double(el, "lon"), attr_double(el, "lat"));
        }
        else if (el_name == "way")
        {
            mapnik::geometry::line_string<double> line;
            for (rapidxml::xml_node<>* nd = el->first_node("nd"); nd; nd = nd->next_sibling("nd"))
            {
                line.emplace_back(attr_double(nd, "lon"), attr_double(nd, "lat"));
            }
            if (line.size() < 2)
                continue;
            if (line.size() >= 4 && line.front() == line.back())
            {
                // closed way -> polygon. OSM area semantics are really
                // tag-dependent (highway=* rings are usually lines); refine
                // via a parameter when needed.
                mapnik::geometry::polygon<double> poly;
                poly.emplace_back(line.begin(), line.end());
                geom = std::move(poly);
            }
            else
            {
                geom = std::move(line);
            }
        }
        else
        {
            // TODO: relations (multipolygon ring assembly from member
            // geometries); "remark" elements carry server diagnostics
            if (el_name == "remark")
            {
                MAPNIK_LOG_WARN(overpass) << "overpass server remark: " << std::string(el->value(), el->value_size());
            }
            continue;
        }

        mapnik::feature_ptr feature = mapnik::feature_factory::create(ctx, feature_id++);
        feature->set_geometry(std::move(geom));
        feature->put_new("osm_id", attr_integer(el, "id"));
        feature->put_new("osm_type", tr_.transcode(el_name.c_str()));
        for (rapidxml::xml_node<>* tag = el->first_node("tag"); tag; tag = tag->next_sibling("tag"))
        {
            auto* k = tag->first_attribute("k");
            auto* v = tag->first_attribute("v");
            if (!k || !v)
                continue;
            std::string const key(k->value(), k->value_size());
            feature->put_new(key, tr_.transcode(v->value(), static_cast<int>(v->value_size())));
            tag_keys.insert(key);
        }
        features->push_back(std::move(feature));
    }

    // grow the descriptor by the tag keys seen so far, so introspection
    // tools see the fields actually delivered (OSM tags are freeform)
    for (auto const& key : tag_keys)
    {
        bool known = false;
        for (auto const& attr_desc : desc_.get_descriptors())
        {
            if (attr_desc.get_name() == key)
            {
                known = true;
                break;
            }
        }
        if (!known)
            desc_.add_descriptor(mapnik::attribute_descriptor(key, mapnik::String));
    }

    MAPNIK_LOG_DEBUG(overpass) << "overpass_datasource: parsed " << features->size() << " features";
    return features;
}

mapnik::featureset_ptr overpass_datasource::features(mapnik::query const& q) const
{
    std::string const ql = bind_query(q.get_bbox());

    features_ptr parsed;
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto itr = cache_.find(ql);
        if (itr != cache_.end())
            parsed = itr->second;
    }
    if (!parsed)
    {
        // fetch outside the lock -- a slow server must not serialize other
        // layers; worst case two threads fetch the same query once each
        parsed = parse_osm_xml(fetch(ql));
        std::lock_guard<std::mutex> lock(cache_mutex_);
        // one bound query per extent is the norm: renders repeat the same
        // extent across styles, new extents obsolete old entries
        if (cache_.size() >= 8)
            cache_.clear();
        cache_.emplace(ql, parsed);
    }
    return std::make_shared<overpass_featureset>(parsed, q.get_bbox());
}

mapnik::featureset_ptr overpass_datasource::features_at_point(mapnik::coord2d const& pt, double tol) const
{
    // TODO: wrap pt+tol into a bbox query once needed (grid renderer /
    // hit-testing); not required for plain rendering
    return mapnik::featureset_ptr();
}
