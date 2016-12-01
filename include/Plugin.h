// ======================================================================
/*!
 * \brief Smartmet TimeSeries plugin interface
 */
// ======================================================================

#pragma once

#include "Config.h"
#include "Query.h"
#include "QueryLevelDataCache.h"
#include "LonLatDistance.h"
#include "ObsParameter.h"

#include <spine/SmartMetCache.h>
#include <spine/SmartMetPlugin.h>
#include <spine/HTTP.h>
#include <spine/PostGISDataSource.h>
#include <spine/Reactor.h>
#include <spine/TimeSeriesGeneratorCache.h>

#include <newbase/NFmiSvgPath.h>
#include <newbase/NFmiPoint.h>

#include <macgyver/TimeZones.h>

#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>
#include <map>
#include <string>
#include <queue>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
class Engine;
}
namespace Geonames
{
class Engine;
}
namespace Gis
{
class Engine;
}
}

namespace Plugin
{
namespace TimeSeries
{
class State;
class PluginImpl;

typedef boost::variant<SmartMet::Spine::TimeSeries::TimeSeriesPtr,
                       SmartMet::Spine::TimeSeries::TimeSeriesVectorPtr,
                       SmartMet::Spine::TimeSeries::TimeSeriesGroupPtr> TimeSeriesData;
typedef std::vector<std::pair<std::string, std::vector<TimeSeriesData> > > OutputData;
typedef std::pair<int, std::string> PressureLevelParameterPair;
typedef std::map<PressureLevelParameterPair, SmartMet::Spine::TimeSeries::TimeSeriesPtr>
    ParameterTimeSeriesMap;
typedef std::map<PressureLevelParameterPair, SmartMet::Spine::TimeSeries::TimeSeriesGroupPtr>
    ParameterTimeSeriesGroupMap;
typedef std::vector<ObsParameter> ObsParameters;
typedef std::pair<int, SmartMet::Spine::TimeSeries::TimeSeriesVectorPtr> FmisidTSVectorPair;
typedef std::vector<FmisidTSVectorPair> TimeSeriesByLocation;

class Plugin : public SmartMetPlugin, private boost::noncopyable
{
 public:
  Plugin(SmartMet::Spine::Reactor* theReactor, const char* theConfig);
  virtual ~Plugin();

  const std::string& getPluginName() const;
  int getRequiredAPIVersion() const;
  bool queryIsFast(const SmartMet::Spine::HTTP::Request& theRequest) const;

  bool ready() const;

  // Get timezone information
  const Fmi::TimeZones& getTimeZones() const { return itsGeoEngine->getTimeZones(); }
  // Get the engines
  const SmartMet::Engine::Querydata::Engine& getQEngine() const { return *itsQEngine; }
  const SmartMet::Engine::Geonames::Engine& getGeoEngine() const { return *itsGeoEngine; }
  SmartMet::Engine::Observation::Engine& getObsEngine() const { return *itsObsEngine; }
 protected:
  void init();
  void shutdown();
  void requestHandler(SmartMet::Spine::Reactor& theReactor,
                      const SmartMet::Spine::HTTP::Request& theRequest,
                      SmartMet::Spine::HTTP::Response& theResponse);

 private:
  Plugin();

  std::size_t hash_value(const State& state,
                         Query masterquery,
                         const SmartMet::Spine::HTTP::Request& request);

  void query(const State& theState,
             const SmartMet::Spine::HTTP::Request& req,
             SmartMet::Spine::HTTP::Response& response);

  void processQuery(const State& state,
                    SmartMet::Spine::Table& data,
                    Query& masterquery,
                    SmartMet::Spine::PostGISDataSource& postGISDataSource);
  void processObsEngineQuery(const State& state,
                             Query& query,
                             OutputData& outputData,
                             const AreaProducers& areaproducers,
                             const ProducerDataPeriod& producerDataPeriod,
                             ObsParameters& obsParameters,
                             SmartMet::Spine::PostGISDataSource& postGISDataSource);
  void processQEngineQuery(const State& state,
                           Query& query,
                           OutputData& outputData,
                           const AreaProducers& areaproducers,
                           const ProducerDataPeriod& producerDataPeriod,
                           SmartMet::Spine::PostGISDataSource& postGISDataSource);
  void fetchLocationValues(Query& query,
                           SmartMet::Spine::PostGISDataSource& postGISDataSource,
                           SmartMet::Spine::Table& data,
                           unsigned int column_index,
                           unsigned int row_index);
  void fetchObsEngineValuesForArea(const State& state,
                                   const std::string& producer,
                                   const SmartMet::Spine::TaggedLocation& tloc,
                                   const ObsParameters& obsParameters,
                                   SmartMet::Engine::Observation::Settings& settings,
                                   Query& query,
                                   OutputData& outputData);
  void fetchObsEngineValuesForPlaces(const State& state,
                                     const std::string& producer,
                                     const SmartMet::Spine::TaggedLocation& tloc,
                                     const ObsParameters& obsParameters,
                                     SmartMet::Engine::Observation::Settings& settings,
                                     Query& query,
                                     OutputData& outputData);
  void fetchObsEngineValues(const State& state,
                            const std::string& producer,
                            const SmartMet::Spine::TaggedLocation& tloc,
                            const ObsParameters& obsParameters,
                            SmartMet::Engine::Observation::Settings& settings,
                            Query& query,
                            OutputData& outputData);
  void fetchQEngineValues(const State& state,
                          const SmartMet::Spine::ParameterAndFunctions& paramfunc,
                          const SmartMet::Spine::TaggedLocation& tloc,
                          Query& query,
                          const AreaProducers& areaproducers,
                          const ProducerDataPeriod& producerDataPeriod,
                          SmartMet::Spine::PostGISDataSource& postGISDataSource,
                          QueryLevelDataCache& queryLevelDataCache,
                          OutputData& outputData);
  bool isObsProducer(const std::string& producer) const;
  void setCommonObsSettings(SmartMet::Engine::Observation::Settings& settings,
                            const std::string& producer,
                            const ProducerDataPeriod& producerDataPeriod,
                            const boost::posix_time::ptime& now,
                            const ObsParameters& obsParameters,
                            Query& query,
                            SmartMet::Spine::PostGISDataSource& postGISDataSource) const;
  void setLocationObsSettings(SmartMet::Engine::Observation::Settings& settings,
                              const std::string& producer,
                              const ProducerDataPeriod& producerDataPeriod,
                              const boost::posix_time::ptime& now,
                              const SmartMet::Spine::TaggedLocation& tloc,
                              const ObsParameters& obsParameters,
                              Query& query,
                              SmartMet::Spine::PostGISDataSource& postGISDataSource) const;

  std::vector<ObsParameter> getObsParamers(const Query& query) const;

  SmartMet::Spine::TimeSeriesGenerator::LocalTimeList generateQEngineQueryTimes(
      const SmartMet::Engine::Querydata::Q& q,
      const Query& query,
      const std::string paramname) const;

  SmartMet::Spine::LocationPtr getLocationForArea(
      const SmartMet::Spine::TaggedLocation& tloc,
      const Query& query,
      const NFmiSvgPath& svgPath,
      SmartMet::Spine::PostGISDataSource& postGISDataSource) const;

  const std::string itsModuleName;
  Config itsConfig;
  SmartMet::Spine::PostGISDataSource itsPostGISDataSource;
  bool itsReady;

  SmartMet::Spine::Reactor* itsReactor;
  SmartMet::Engine::Querydata::Engine* itsQEngine;
  SmartMet::Engine::Geonames::Engine* itsGeoEngine;
  SmartMet::Engine::Gis::Engine* itsGisEngine;
  SmartMet::Engine::Observation::Engine* itsObsEngine;

  // station types (producers) supported by observation
  std::set<std::string> itsObsEngineStationTypes;

  // Cached results
  mutable std::unique_ptr<SmartMet::Spine::SmartMetCache> itsCache;
  // Cached time series
  mutable std::unique_ptr<SmartMet::Spine::TimeSeriesGeneratorCache> itsTimeSeriesCache;

};  // class Plugin

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
