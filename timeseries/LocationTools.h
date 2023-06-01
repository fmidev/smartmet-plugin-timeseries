#pragma once

#include <engines/geonames/Engine.h>
#include <engines/gis/Engine.h>
#include <engines/observation/Engine.h>
#include <gis/OGR.h>
#include <newbase/NFmiSvgPath.h>
#include <spine/Location.h>
#include <spine/Parameter.h>
#include <timeseries/TimeSeriesInclude.h>
#include <string>
#include <utility>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
std::string get_name_base(const std::string& theName);

const OGRGeometry* get_ogr_geometry(const Spine::TaggedLocation& tloc,
                                    const Engine::Gis::GeometryStorage& geometryStorage);

std::unique_ptr<OGRGeometry> get_ogr_geometry(const std::string& wktString, double radius = 0.0);

void get_svg_path(const Spine::TaggedLocation& tloc,
                  const Engine::Gis::GeometryStorage& geometryStorage,
                  NFmiSvgPath& svgPath);

Spine::LocationList get_location_list(const NFmiSvgPath& thePath,
                                      const std::string& thePathName,
                                      const double& stepInKm,
                                      const Engine::Geonames::Engine& geonames);

std::string get_location_id(const Spine::LocationPtr& loc);

Spine::LocationPtr get_location(const Engine::Geonames::Engine& geonames,
                                int id,
                                const std::string& idtype,
                                const std::string& language);

int get_fmisid_value(const TS::TimeSeries& ts);

#ifndef WITHOUT_OBSERVATION

std::vector<int> get_fmisids_for_wkt(Engine::Observation::Engine* observation,
                                     const Engine::Observation::Settings& settings,
                                     const std::string& wktstring);

int get_fmisid_index(const Engine::Observation::Settings& settings);
int get_fmisid_value(const TS::Value& value);

std::unique_ptr<Spine::Location> get_coordinate_location(double lon,
                                                         double lat,
                                                         const std::string& language,
                                                         const Engine::Geonames::Engine& geoEngine);

#endif

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
