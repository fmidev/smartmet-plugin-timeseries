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
#include <engines/querydata/OriginTime.h>
#include <engines/querydata/Producer.h>
#include <engines/querydata/Q.h>
#include <boost/date_time/posix_time/ptime.hpp>

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
namespace Observation
{
class Engine;
}
namespace Geonames
{
class Engine;
}
}

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
  const SmartMet::Engine::Querydata::Engine& getQEngine() const;
  const SmartMet::Engine::Geonames::Engine& getGeoEngine() const;
  SmartMet::Engine::Observation::Engine* getObsEngine() const;

  const Fmi::TimeZones& getTimeZones() const;
  // The fixed time during the query may also be overridden
  // for testing purposes
  const boost::posix_time::ptime& getTime() const;
  void setTime(const boost::posix_time::ptime& theTime);

  // Get querydata for the given input
  SmartMet::Engine::Querydata::Q get(
      const SmartMet::Engine::Querydata::Producer& theProducer) const;
  SmartMet::Engine::Querydata::Q get(
      const SmartMet::Engine::Querydata::Producer& theProducer,
      const SmartMet::Engine::Querydata::OriginTime& theOriginTime) const;

 private:
  const Plugin& itsPlugin;
  boost::posix_time::ptime itsTime;

  // Querydata caches - always make the same choice for same locations and producers
  typedef std::map<SmartMet::Engine::Querydata::Producer, SmartMet::Engine::Querydata::Q> QCache;
  typedef std::map<SmartMet::Engine::Querydata::OriginTime, QCache> TimedQCache;

  mutable QCache itsQCache;
  mutable TimedQCache itsTimedQCache;

};  // class State

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
