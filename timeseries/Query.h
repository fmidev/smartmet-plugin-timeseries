// ======================================================================
/*!
 * \brief Utility functions for parsing the request
 *
 * Separated from PluginImpl to clarify the implementation
 */
// ======================================================================

#pragma once

#include "AggregationInterval.h"
#include "Producers.h"
#include <engines/geonames/WktGeometry.h>

#include <engines/geonames/Engine.h>
#include <engines/geonames/WktGeometry.h>
#include <engines/grid/Engine.h>

#include <grid-content/queryServer/definition/AliasFileCollection.h>
#include <grid-files/common/AdditionalParameters.h>
#include <grid-files/common/AttributeList.h>
#include <timeseries/TimeSeriesInclude.h>
#include <spine/HTTP.h>
#include <spine/Location.h>
#include <spine/OptionParsers.h>
#include <spine/Parameter.h>
#include <spine/TimeSeriesGeneratorOptions.h>
#include <spine/ValueFormatter.h>

#include <macgyver/TimeFormatter.h>
#include <newbase/NFmiPoint.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/shared_ptr.hpp>

#include <list>
#include <locale>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "ProducerDataPeriod.h"

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

struct Query
{
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

#ifndef WITHOUT_OBSERVATION
  std::string iot_producer_specifier;
#endif

  boost::posix_time::ptime latestTimestep;
  boost::optional<boost::posix_time::ptime> origintime;

  TimeProducers timeproducers;
  std::shared_ptr<Engine::Geonames::LocationOptions> loptions;
  boost::shared_ptr<Fmi::TimeFormatter> timeformatter;

  // last coordinate used while forming the output
  mutable NFmiPoint lastpoint;

  std::vector<int> weekdays;

#ifndef WITHOUT_OBSERVATION
  std::vector<int> wmos;
  std::vector<int> lpnns;
  std::vector<int> fmisids;
#endif

  ParamPrecisions precisions;
  Spine::ValueFormatter valueformatter;

#ifndef WITHOUT_OBSERVATION
  std::map<std::string, double> boundingBox;
  TS::DataFilter dataFilter;
#endif

  Levels levels;
  Pressures pressures;
  Heights heights;

  Spine::OptionParsers::ParameterOptions poptions;
  MaxAggregationIntervals maxAggregationIntervals;
  Engine::Geonames::WktGeometries wktGeometries;
  TS::TimeSeriesGeneratorOptions toptions;

#ifndef WITHOUT_OBSERVATION
  int numberofstations;
#endif

  bool maxdistanceOptionGiven;
  bool findnearestvalidpoint;
  bool debug = false;
  bool starttimeOptionGiven;
  bool endtimeOptionGiven;
  bool timeAggregationRequested;
  std::string forecastSource;
  T::AttributeList attributeList;

#ifndef WITHOUT_OBSERVATION
  bool allplaces;
  bool latestObservation;
  bool useDataCache;
#endif
  Spine::LocationList inKeywordLocations;
  bool groupareas{true};

  double maxdistance_kilometers() const;
  double maxdistance_meters() const;
  // DO NOT FORGET TO CHANGE hash_value IF YOU ADD ANY NEW PARAMETERS

 private:
  Query();

  void parse_levels(const Spine::HTTP::Request& theReq);

  void parse_precision(const Spine::HTTP::Request& theReq, const Config& config);

#ifndef WITHOUT_OBSERVATION
  void parse_parameters(const Spine::HTTP::Request& theReq,
                        const Engine::Observation::Engine* theObsEngine);
  void parse_producers(const Spine::HTTP::Request& theReq,
                       const Engine::Querydata::Engine& theQEngine,
                       const Engine::Grid::Engine* theGridEngine,
                       const Engine::Observation::Engine* theObsEngine);
#else
  void parse_parameters(const Spine::HTTP::Request& theReq);
  void parse_producers(const Spine::HTTP::Request& theReq,
                       const Engine::Querydata::Engine& theQEngine,
                       const Engine::Grid::Engine& theGridEngine);

#endif
  QueryServer::AliasFileCollection* itsAliasFileCollectionPtr;

  std::string maxdistance;
};

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
