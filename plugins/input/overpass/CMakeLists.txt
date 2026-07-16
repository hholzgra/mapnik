find_package(CURL REQUIRED)

add_plugin_target(input-overpass "overpass")
target_sources(input-overpass ${_plugin_visibility}
    overpass_datasource.cpp
    overpass_featureset.cpp
)
target_link_libraries(input-overpass ${_plugin_visibility}
    mapnik::mapnik
    CURL::libcurl
)
