// ======================================================================
/*!
 * \brief Utility functions for parsing the request
 *
 * Separated from PluginImpl to clarify the implementation
 */
// ======================================================================

#pragma once

#include "Producers.h"
#include "AggregationInterval.h"

#include <spine/HTTP.h>
#include <spine/Location.h>
#include <spine/Parameter.h>
#include <spine/ValueFormatter.h>
#include <spine/TimeSeriesGeneratorOptions.h>
#include <spine/OptionParsers.h>
#include <spine/TableFeeder.h>

#include <newbase/NFmiPoint.h>
#include <macgyver/TimeFormatter.h>

#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

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

  // DO NOT FORGET TO CHANGE hash_value IF YOU ADD ANY NEW PARAMETERS

  double maxdistance;
  double step;  // used with path geometry
  bool maxdistanceOptionGiven;
  bool findnearestvalidpoint;

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
  // observation params begin
  std::vector<int> wmos;
  std::vector<int> lpnns;
  std::vector<int> fmisids;
  std::vector<int> geoids;
  std::vector<int> weekdays;
  std::map<std::string, double> boundingBox;
  int numberofstations;
  bool allplaces;
  bool latestObservation;
  // observation params end

  bool starttimeOptionGiven;
  bool endtimeOptionGiven;
  boost::optional<boost::posix_time::ptime> origintime;
  boost::posix_time::ptime latestTimestep;

  TimeProducers timeproducers;
  Levels levels;

  // shared so that copying would be fast
  std::shared_ptr<SmartMet::Engine::Geonames::LocationOptions> loptions;

  SmartMet::Spine::OptionParsers::ParameterOptions poptions;
  SmartMet::Spine::TimeSeriesGeneratorOptions toptions;
  ParamPrecisions precisions;

  SmartMet::Spine::ValueFormatter valueformatter;
  boost::shared_ptr<Fmi::TimeFormatter> timeformatter;
  MaxAggregationIntervals maxAggregationIntervals;
  bool timeAggregationRequested;

  // DO NOT FORGET TO CHANGE hash_value IF YOU ADD ANY NEW PARAMETERS

  // last coordinate used while forming the output
  mutable NFmiPoint lastpoint;

 private:
  Query();

  void parse_producers(const SmartMet::Spine::HTTP::Request& theReq);

  void parse_levels(const SmartMet::Spine::HTTP::Request& theReq);

  void parse_precision(const SmartMet::Spine::HTTP::Request& theReq, const Config& config);
  void parse_parameters(const SmartMet::Spine::HTTP::Request& theReq,
                        const SmartMet::Engine::Observation::Engine& theObsEngine);
};

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
