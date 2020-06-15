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

#include <engines/grid/Engine.h>
#include <engines/geonames/Engine.h>
#include <engines/geonames/WktGeometry.h>

#include <grid-content/queryServer/definition/AliasFileCollection.h>
#include <grid-files/common/AdditionalParameters.h>
#include <grid-files/common/AttributeList.h>
#include <spine/HTTP.h>
#include <spine/Location.h>
#include <spine/OptionParsers.h>
#include <spine/Parameter.h>
#include <spine/TableFeeder.h>
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
  Query(const State& state, const SmartMet::Spine::HTTP::Request& req, Config& config);

  typedef std::vector<int> ParamPrecisions;
  typedef std::set<int> Levels;
  typedef std::set<double> Pressures;
  typedef std::set<double> Heights;

  // DO NOT FORGET TO CHANGE hash_value IF YOU ADD ANY NEW PARAMETERS

  double maxdistance;
  double step;  // used with path geometry
  bool maxdistanceOptionGiven;
  bool findnearestvalidpoint;

  std::size_t startrow;    // Paging; first (0-) row to return; default 0
  std::size_t maxresults;  //         max rows to return (page length); default 0 (all)

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
  std::vector<int> geoids;
  std::vector<int> weekdays;

#ifndef WITHOUT_OBSERVATION
  std::vector<int> wmos;
  std::vector<int> lpnns;
  std::vector<int> fmisids;
  std::map<std::string, double> boundingBox;
  int numberofstations;
  bool allplaces;
  bool latestObservation;
  Engine::Observation::SQLDataFilter sqlDataFilter;
  bool useDataCache;
#endif

  bool starttimeOptionGiven;
  bool endtimeOptionGiven;
  boost::optional<boost::posix_time::ptime> origintime;
  boost::posix_time::ptime latestTimestep;

  TimeProducers timeproducers;
  Levels levels;
  Pressures pressures;
  Heights heights;

  // shared so that copying would be fast
  std::shared_ptr<Engine::Geonames::LocationOptions> loptions;

  SmartMet::Spine::OptionParsers::ParameterOptions poptions;
  SmartMet::Spine::TimeSeriesGeneratorOptions toptions;
  ParamPrecisions precisions;

  SmartMet::Spine::ValueFormatter valueformatter;
  boost::shared_ptr<Fmi::TimeFormatter> timeformatter;
  MaxAggregationIntervals maxAggregationIntervals;
  bool timeAggregationRequested;
  // WKT geometries passed in URL are stored here
  Engine::Geonames::WktGeometries wktGeometries;
  std::string forecastSource;
  T::AttributeList attributeList;

  // DO NOT FORGET TO CHANGE hash_value IF YOU ADD ANY NEW PARAMETERS

  // last coordinate used while forming the output
  mutable NFmiPoint lastpoint;

 private:
  Query();

  void parse_levels(const SmartMet::Spine::HTTP::Request& theReq);

  void parse_precision(const SmartMet::Spine::HTTP::Request& theReq, const Config& config);

#ifndef WITHOUT_OBSERVATION
  void parse_parameters(const SmartMet::Spine::HTTP::Request& theReq,
                        const SmartMet::Engine::Observation::Engine* theObsEngine);
  void parse_producers(const SmartMet::Spine::HTTP::Request& theReq,
                       const SmartMet::Engine::Querydata::Engine& theQEngine,
                       const SmartMet::Engine::Grid::Engine* theGridEngine,
                       const SmartMet::Engine::Observation::Engine* theObsEngine);
#else
  void parse_parameters(const SmartMet::Spine::HTTP::Request& theReq);
  void parse_producers(const SmartMet::Spine::HTTP::Request& theReq,
                       const SmartMet::Engine::Querydata::Engine& theQEngine,
                       const SmartMet::Engine::Grid::Engine& theGridEngine);

#endif
  QueryServer::AliasFileCollection* itsAliasFileCollectionPtr;
};

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
