// ======================================================================
/*!
 * \brief Utility functions for parsing the request
 *
 * Separated from PluginImpl to clarify the implementation
 */
// ======================================================================

#pragma once

#include "ObsQueryParams.h"
#include "Producers.h"
#include "AggregationInterval.h"
#include <engines/geonames/Engine.h>
#include <engines/geonames/WktGeometry.h>
#include <timeseries/TimeSeriesInclude.h>
#include <timeseries/OptionParsers.h>
#include <grid-files/common/AttributeList.h>
#include <grid-content/queryServer/definition/AliasFileCollection.h>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
class Config;

// ----------------------------------------------------------------------
/*!
 * \brief Individual precision settings
 */
// ----------------------------------------------------------------------

struct Query : public ObsQueryParams
{
  Query() = delete;
  Query(const State& state, const Spine::HTTP::Request& req, Config& config);

  // Note: Data members ordered according to the advice of Clang Analyzer to avoid excessive padding

  // iot_producer_specifier, latestTimestep, origintime, timeproducers, loptions, timeformatter,
  // lastpoint, weekdays, wmos, lpnns, fmisids, precisions, valueformatter, boundingBox,
  // dataFilter, levels, pressures, heights, poptions, maxAggregationIntervals, wktGeometries,
  // toptions, numberofstations, maxdistanceOptionGiven, findnearestvalidpoint, debug, allplaces,
  // latestObservation, useDataCache, starttimeOptionGiven, endtimeOptionGiven,
  // timeAggregationRequested,

  using ParamPrecisions = std::vector<int>;
  using Levels = std::set<int>;
  using Pressures = std::set<double>;
  using Heights = std::set<double>;

  // DO NOT FORGET TO CHANGE hash_value IF YOU ADD ANY NEW PARAMETERS

  double step;  // used with path geometry

  std::size_t startrow;    // Paging; first (0-) row to return; default 0
  std::size_t maxresults;  // max rows to return (page length); default 0 (all)

  std::string wmo;
  std::string fmisid;
  std::string lpnn;
  std::string language;
  std::string keyword;
  std::string timezone;
  std::string leveltype;
  std::string format;
  std::string timeformat;
  std::string timestring;
  std::string localename;
  std::locale outlocale;
  std::string areasource;
  std::string crs;

  boost::posix_time::ptime latestTimestep;
  boost::optional<boost::posix_time::ptime> origintime;

  TimeProducers timeproducers;
  std::shared_ptr<Engine::Geonames::LocationOptions> loptions;
  boost::shared_ptr<Fmi::TimeFormatter> timeformatter;

  // last coordinate used while forming the output
  mutable NFmiPoint lastpoint;

  std::vector<int> weekdays;

  ParamPrecisions precisions;
  Fmi::ValueFormatter valueformatter;

  Levels levels;
  Pressures pressures;
  Heights heights;

  TS::OptionParsers::ParameterOptions poptions;
  MaxAggregationIntervals maxAggregationIntervals;
  Engine::Geonames::WktGeometries wktGeometries;
  TS::TimeSeriesGeneratorOptions toptions;

  bool maxdistanceOptionGiven;
  bool findnearestvalidpoint;
  bool debug = false;
  bool starttimeOptionGiven;
  bool endtimeOptionGiven;
  bool timeAggregationRequested;
  std::string forecastSource;
  T::AttributeList attributeList;

  Spine::LocationList inKeywordLocations;
  bool groupareas{true};

  double maxdistance_kilometers() const;
  double maxdistance_meters() const;
  // DO NOT FORGET TO CHANGE hash_value IF YOU ADD ANY NEW PARAMETERS

 private:
  void parse_levels(const Spine::HTTP::Request& theReq);

  void parse_precision(const Spine::HTTP::Request& theReq, const Config& config);

  void parse_producers(const Spine::HTTP::Request& theReq,
					   const State& theState);
  void parse_parameters(const Spine::HTTP::Request& theReq);
  void parse_aggregation_intervals(const Spine::HTTP::Request& theReq);
  void parse_attr(const Spine::HTTP::Request& theReq);
  bool parse_grib_loptions(const State& state);
  void parse_inkeyword_locations(const Spine::HTTP::Request& theReq, const State& state);
  void parse_origintime(const Spine::HTTP::Request& theReq);

  QueryServer::AliasFileCollection* itsAliasFileCollectionPtr;

  std::string maxdistance;
};

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
