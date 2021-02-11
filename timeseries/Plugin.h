// ======================================================================
/*!
 * \brief Smartmet TimeSeries plugin interface
 */
// ======================================================================

#pragma once

#include "Config.h"
#include "DataFunctions.h"
#include "LonLatDistance.h"
#include "ObsParameter.h"
#include "Query.h"
#include "QueryLevelDataCache.h"
#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>
#include <engines/gis/GeometryStorage.h>
#include <macgyver/TimeZones.h>
#include <newbase/NFmiPoint.h>
#include <newbase/NFmiSvgPath.h>
#include <spine/HTTP.h>
#include <spine/Reactor.h>
#include <spine/SmartMetCache.h>
#include <spine/SmartMetPlugin.h>
#include <spine/TimeSeriesGeneratorCache.h>
#include <map>
#include <queue>
#include <string>

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
}  // namespace Engine

namespace Plugin
{
namespace TimeSeries
{
class State;
class PluginImpl;

struct SettingsInfo
{
  Engine::Observation::Settings settings;
  bool is_area{false};
  std::string area_name{""};

  SettingsInfo(const Engine::Observation::Settings& s, bool isa, const std::string& an)
      : settings(s), is_area(isa), area_name(an)
  {
  }
};

class Plugin : public SmartMetPlugin, private boost::noncopyable
{
 public:
  Plugin(Spine::Reactor* theReactor, const char* theConfig);
  virtual ~Plugin();

  const std::string& getPluginName() const;
  int getRequiredAPIVersion() const;
  bool queryIsFast(const Spine::HTTP::Request& theRequest) const;

  bool ready() const;

  // Get timezone information
  const Fmi::TimeZones& getTimeZones() const { return itsGeoEngine->getTimeZones(); }
  // Get the engines
  const Engine::Querydata::Engine& getQEngine() const { return *itsQEngine; }
  const Engine::Geonames::Engine& getGeoEngine() const { return *itsGeoEngine; }
#ifndef WITHOUT_OBSERVATION
  // May return null
  Engine::Observation::Engine* getObsEngine() const { return itsObsEngine; }
#endif

 protected:
  void init();
  void shutdown();
  void requestHandler(Spine::Reactor& theReactor,
                      const Spine::HTTP::Request& theRequest,
                      Spine::HTTP::Response& theResponse);

 private:
  Plugin();

  std::size_t hash_value(const State& state,
                         Query masterquery,
                         const Spine::HTTP::Request& request);

  void query(const State& theState,
             const Spine::HTTP::Request& req,
             Spine::HTTP::Response& response);

  void processQuery(const State& state, Spine::Table& data, Query& masterquery);

  void processQEngineQuery(const State& state,
                           Query& query,
                           OutputData& outputData,
                           const AreaProducers& areaproducers,
                           const ProducerDataPeriod& producerDataPeriod);
  void fetchStaticLocationValues(Query& query,
                                 Spine::Table& data,
                                 unsigned int column_index,
                                 unsigned int row_index);

  void fetchQEngineValues(const State& state,
                          const Spine::ParameterAndFunctions& paramfunc,
                          const Spine::TaggedLocation& tloc,
                          Query& query,
                          const AreaProducers& areaproducers,
                          const ProducerDataPeriod& producerDataPeriod,
                          QueryLevelDataCache& queryLevelDataCache,
                          OutputData& outputData);

#ifndef WITHOUT_OBSERVATION
  bool isObsProducer(const std::string& producer) const;

  void processObsEngineQuery(const State& state,
                             Query& query,
                             OutputData& outputData,
                             const AreaProducers& areaproducers,
                             const ProducerDataPeriod& producerDataPeriod,
                             const ObsParameters& obsParameters);
  void fetchObsEngineValuesForArea(const State& state,
                                   const std::string& producer,
                                   const ObsParameters& obsParameters,
                                   const std::string& areaName,
                                   Engine::Observation::Settings& settings,
                                   Query& query,
                                   OutputData& outputData);
  void fetchObsEngineValuesForPlaces(const State& state,
                                     const std::string& producer,
                                     const ObsParameters& obsParameters,
                                     Engine::Observation::Settings& settings,
                                     Query& query,
                                     OutputData& outputData);

  void getCommonObsSettings(Engine::Observation::Settings& settings,
                            const std::string& producer,
                            Query& query) const;
  void getObsSettings(std::vector<SettingsInfo>& settingsVector,
                      const std::string& producer,
                      const ProducerDataPeriod& producerDataPeriod,
                      const boost::posix_time::ptime& now,
                      const ObsParameters& obsParameters,
                      Query& query) const;

  bool resolveAreaStations(const Spine::LocationPtr & location,
                           const std::string& producer,
                           Query& query,
                           Engine::Observation::Settings& settings,
                           std::string& name) const;
  void resolveParameterSettings(const ObsParameters& obsParameters,
                                const Query& query,
                                const std::string& producer,
                                Engine::Observation::Settings& settings,
                                unsigned int& aggregationIntervalBehind,
                                unsigned int& aggregationIntervalAhead) const;
  void resolveTimeSettings(const std::string& producer,
                           const ProducerDataPeriod& producerDataPeriod,
                           const boost::posix_time::ptime& now,
                           Query& query,
                           unsigned int aggregationIntervalBehind,
                           unsigned int aggregationIntervalAhead,
                           Engine::Observation::Settings& settings) const;
  std::vector<ObsParameter> getObsParameters(const Query& query) const;
#endif

  Spine::TimeSeriesGenerator::LocalTimeList generateQEngineQueryTimes(
      const Query& query, const std::string& paramname) const;

  Spine::LocationPtr getLocationForArea(const Spine::TaggedLocation& tloc,
                                        const Query& query,
                                        NFmiSvgPath* svgPath = nullptr) const;
  void checkInKeywordLocations(Query& masterquery);

  const std::string itsModuleName;
  Config itsConfig;
  bool itsReady = false;

  Spine::Reactor* itsReactor = nullptr;
  Engine::Querydata::Engine* itsQEngine = nullptr;
  Engine::Geonames::Engine* itsGeoEngine = nullptr;
  Engine::Gis::Engine* itsGisEngine = nullptr;
#ifndef WITHOUT_OBSERVATION
  Engine::Observation::Engine* itsObsEngine = nullptr;
#endif

  // station types (producers) supported by observation
  std::set<std::string> itsObsEngineStationTypes;

  // Cached results
  mutable std::unique_ptr<Spine::SmartMetCache> itsCache;
  // Cached time series
  mutable std::unique_ptr<Spine::TimeSeriesGeneratorCache> itsTimeSeriesCache;

  // Geometries and their svg-representations are stored here
  Engine::Gis::GeometryStorage itsGeometryStorage;
};  // class Plugin

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
