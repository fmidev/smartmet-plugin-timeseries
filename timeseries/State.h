// ======================================================================
/*!
 * \brief Query specific state variables and caches
 *
 * The State fixes the wall clock time for the duration of the query
 * so that different locations will not get different times simply
 * because the minute or hour may change during the calculations.
 *
 * The State object is often used to pass requests to the plugin
 * so that the results may be cached. This is not for speed, but
 * to make sure the same selection is made again if needed.
 *
 * Note that in general there are no thread safety issues since the
 * object is query specific.
 */
// ======================================================================

#pragma once
#include <boost/date_time/posix_time/ptime.hpp>
#include <engines/querydata/OriginTime.h>
#include <engines/querydata/Producer.h>
#include <engines/querydata/Q.h>
#include <timeseries/TimeSeriesInclude.h>

namespace Fmi
{
class TimeZones;
}

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
class Engine;
}
namespace Grid
{
class Engine;
}

#ifndef WITHOUT_OBSERVATION
namespace Observation
{
class Engine;
}
#endif

namespace Geonames
{
class Engine;
}
}  // namespace Engine

namespace Plugin
{
namespace TimeSeries
{
class Plugin;

class State
{
 public:
  // We always construct with the plugin only
  State(const Plugin& thePlugin);
  State() = delete;
  State(const State& other) = delete;

  // Access engines
  const Engine::Querydata::Engine& getQEngine() const;
  const Engine::Geonames::Engine& getGeoEngine() const;
  const Engine::Grid::Engine* getGridEngine() const;
#ifndef WITHOUT_OBSERVATION
  Engine::Observation::Engine* getObsEngine() const;
#endif

  const Fmi::TimeZones& getTimeZones() const;
  // The fixed time during the query may also be overridden
  // for testing purposes
  const boost::posix_time::ptime& getTime() const;
  void setTime(const boost::posix_time::ptime& theTime);

  // Get querydata for the given input
  Engine::Querydata::Q get(
      const Engine::Querydata::Producer& theProducer) const;
  Engine::Querydata::Q get(
      const Engine::Querydata::Producer& theProducer,
      const Engine::Querydata::OriginTime& theOriginTime) const;
  TS::LocalTimePoolPtr getLocalTimePool() const;

 private:
  const Plugin& itsPlugin;
  boost::posix_time::ptime itsTime;
  TS::LocalTimePoolPtr itsLocalTimePool{nullptr};

  // Querydata caches - always make the same choice for same locations and producers
  using QCache = std::map<Engine::Querydata::Producer, Engine::Querydata::Q>;
  using TimedQCache = std::map<Engine::Querydata::OriginTime, QCache>;

  mutable QCache itsQCache;
  mutable TimedQCache itsTimedQCache;

};  // class State

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
