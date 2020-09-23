#include "State.h"
#include "Plugin.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <macgyver/Exception.h>

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

const Engine::Querydata::Engine& State::getQEngine() const
{
  try
  {
    return itsPlugin.getQEngine();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}




const Engine::Grid::Engine* State::getGridEngine() const
{
  try
  {
    return itsPlugin.getGridEngine();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}


// ----------------------------------------------------------------------
/*!
 * \brief Get the GEO engine
 */
// ----------------------------------------------------------------------

const Engine::Geonames::Engine& State::getGeoEngine() const
{
  try
  {
    return itsPlugin.getGeoEngine();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the OBS engine
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION
Engine::Observation::Engine* State::getObsEngine() const
{
  try
  {
    return itsPlugin.getObsEngine();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
#endif

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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

Engine::Querydata::Q State::get(const Engine::Querydata::Producer& theProducer) const
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get querydata for the given input
 */
// ----------------------------------------------------------------------

Engine::Querydata::Q State::get(const Engine::Querydata::Producer& theProducer,
                                const Engine::Querydata::OriginTime& theOriginTime) const
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
    throw Fmi::Exception::Trace(BCP, "Failed to get querydata for the requested origintime")
        .disableStackTrace();
  }
}

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
