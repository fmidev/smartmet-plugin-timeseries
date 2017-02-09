#include "State.h"
#include "Plugin.h"
#include <spine/Exception.h>
#include <spine/Exception.h>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
// ----------------------------------------------------------------------
/*!
 * \brief Initialize'the query state object
 */
// ----------------------------------------------------------------------

State::State(const Plugin& thePlugin)
    : itsPlugin(thePlugin), itsTime(boost::posix_time::second_clock::universal_time())
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the querydata engine
 */
// ----------------------------------------------------------------------

const SmartMet::Engine::Querydata::Engine& State::getQEngine() const
{
  try
  {
    return itsPlugin.getQEngine();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the GEO engine
 */
// ----------------------------------------------------------------------

const SmartMet::Engine::Geonames::Engine& State::getGeoEngine() const
{
  try
  {
    return itsPlugin.getGeoEngine();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the OBS engine
 */
// ----------------------------------------------------------------------

SmartMet::Engine::Observation::Engine* State::getObsEngine() const
{
  try
  {
    return itsPlugin.getObsEngine();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Access to time zone information
 */
// ----------------------------------------------------------------------

const Fmi::TimeZones& State::getTimeZones() const
{
  try
  {
    return itsPlugin.getTimeZones();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the wall clock time for the query
 */
// ----------------------------------------------------------------------

const boost::posix_time::ptime& State::getTime() const
{
  return itsTime;
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the wall clock time for the query.
 *
 * This is usually used for testing purposes only. For example,
 * regression tests may set the "wall clock" in order to be
 * able to use old querydata during testing.
 */
// ----------------------------------------------------------------------

void State::setTime(const boost::posix_time::ptime& theTime)
{
  itsTime = theTime;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get querydata for the given input
 */
// ----------------------------------------------------------------------

SmartMet::Engine::Querydata::Q State::get(
    const SmartMet::Engine::Querydata::Producer& theProducer) const
{
  try
  {
    // Use cached result if there is one
    auto res = itsQCache.find(theProducer);
    if (res != itsQCache.end())
      return res->second;

    // Get the data from the engine
    auto q = itsPlugin.getQEngine().get(theProducer);

    // Cache the obtained data and return it. The cache is
    // request specific, no need for mutexes here.
    itsQCache[theProducer] = q;
    return q;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get querydata for the given input
 */
// ----------------------------------------------------------------------

SmartMet::Engine::Querydata::Q State::get(
    const SmartMet::Engine::Querydata::Producer& theProducer,
    const SmartMet::Engine::Querydata::OriginTime& theOriginTime) const
{
  try
  {
    // Use cached result if there is one
    auto res = itsTimedQCache.find(theOriginTime);
    if (res != itsTimedQCache.end())
    {
      auto res2 = res->second.find(theProducer);
      if (res2 != res->second.end())
        return res2->second;
    }

    // Get the data from the engine
    auto q = itsPlugin.getQEngine().get(theProducer, theOriginTime);

    // Cache the obtained data and return it. The cache is
    // request specific, no need for mutexes here.

    itsTimedQCache[theOriginTime][theProducer] = q;
    return q;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
