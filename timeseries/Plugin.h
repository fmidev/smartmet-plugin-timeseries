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
#include <grid-content/queryServer/definition/QueryStreamer.h>
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
namespace Grid
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
class GridInterface;

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

  void processQuery(const State& state,
                    Spine::Table& data,
                    Query& masterquery,
                    QueryServer::QueryStreamer_sptr queryStreamer);

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

  void processGridEngineQuery(const State& state,
                              Query& masterquery,
                              OutputData& outputData,
                              QueryServer::QueryStreamer_sptr queryStreamer,
                              const AreaProducers& areaproducers,
                              const ProducerDataPeriod& producerDataPeriod);

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
                                   const Spine::TaggedLocation& tloc,
                                   const ObsParameters& obsParameters,
                                   Engine::Observation::Settings& settings,
                                   Query& query,
                                   OutputData& outputData);
  void fetchObsEngineValuesForPlaces(const State& state,
                                     const std::string& producer,
                                     const ObsParameters& obsParameters,
                                     Engine::Observation::Settings& settings,
                                     Query& query,
                                     OutputData& outputData);
  void fetchObsEngineValues(const State& state,
                            const std::string& producer,
                            const Spine::TaggedLocation& tloc,
                            const ObsParameters& obsParameters,
                            Engine::Observation::Settings& settings,
                            Query& query,
                            OutputData& outputData);

  void setCommonObsSettings(Engine::Observation::Settings& settings,
                            const std::string& producer,
                            const ProducerDataPeriod& producerDataPeriod,
                            const boost::posix_time::ptime& now,
                            const ObsParameters& obsParameters,
                            Query& query) const;
  void setLocationObsSettings(Engine::Observation::Settings& settings,
                              const std::string& producer,
                              const ProducerDataPeriod& producerDataPeriod,
                              const boost::posix_time::ptime& now,
                              const Spine::TaggedLocation& tloc,
                              const ObsParameters& obsParameters,
                              Query& query) const;

  std::vector<ObsParameter> getObsParameters(const Query& query) const;
  void setMobileAndExternalDataSettings(Engine::Observation::Settings& settings,
                                        Query& query) const;
#endif

  Spine::TimeSeriesGenerator::LocalTimeList generateQEngineQueryTimes(
      const Engine::Querydata::Q& q, const Query& query, const std::string& paramname) const;

  Spine::LocationPtr getLocationForArea(const Spine::TaggedLocation& tloc,
                                        const Query& query,
                                        NFmiSvgPath* svgPath = nullptr) const;

  Spine::LocationPtr getLocationForArea(const Spine::TaggedLocation& tloc,
                                        int radius,
                                        const Query& query,
                                        NFmiSvgPath* svgPath = nullptr) const;

  const std::string itsModuleName;
  Config itsConfig;
  bool itsReady;

  Spine::Reactor* itsReactor = nullptr;
  Engine::Querydata::Engine* itsQEngine = nullptr;
  Engine::Geonames::Engine* itsGeoEngine = nullptr;
  Engine::Gis::Engine* itsGisEngine = nullptr;
  std::unique_ptr<GridInterface> itsGridInterface;
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
