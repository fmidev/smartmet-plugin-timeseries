// ======================================================================
/*!
 * \brief Smartmet TimeSeries plugin interface
 */
// ======================================================================

#pragma once

#include "Config.h"
#include "Engines.h"

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

class Plugin : public SmartMetPlugin
{
 public:
  Plugin() = delete;
  Plugin(const Plugin& other) = delete;
  Plugin& operator=(const Plugin& other) = delete;
  Plugin(Plugin&& other) = delete;
  Plugin& operator=(Plugin&& other) = delete;
  Plugin(Spine::Reactor* theReactor, const char* theConfig);
  ~Plugin() override;

  const std::string& getPluginName() const override;
  int getRequiredAPIVersion() const override;

  bool ready() const;

  // Get timezone information
  const Fmi::TimeZones& getTimeZones() const { return itsEngines.geoEngine->getTimeZones(); }
  // Get the engines
  const Engines& getEngines() const { return itsEngines; }

 protected:
  void init() override;
  void shutdown() override;
  void requestHandler(Spine::Reactor& theReactor,
                      const Spine::HTTP::Request& theRequest,
                      Spine::HTTP::Response& theResponse) override;

 private:
  void query(const State& theState,
             const Spine::HTTP::Request& req,
             Spine::HTTP::Response& response);

  Fmi::Cache::CacheStatistics getCacheStats() const override;

  void grouplocations(Spine::HTTP::Request& theRequest);

  const std::string itsModuleName;
  Config itsConfig;
  bool itsReady = false;

  Spine::Reactor* itsReactor = nullptr;

  // All engines used ny timeseries plugin
  Engines itsEngines;

  // station types (producers) supported by observation
  std::set<std::string> itsObsEngineStationTypes;

  // Cached time series
  mutable std::unique_ptr<TS::TimeSeriesGeneratorCache> itsTimeSeriesCache;

  // Geometries and their svg-representations are stored here
  Engine::Gis::GeometryStorage itsGeometryStorage;

  friend class QEngineQuery;
  friend class ObsEngineQuery;
  friend class GridEngineQuery;
  friend class QueryProcessingHub;

};  // class Plugin

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
