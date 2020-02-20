#include "ProducerDataPeriod.h"
#include "State.h"

#include <macgyver/TimeZones.h>
#include <spine/Exception.h>

#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/utility.hpp>

#include <map>
#include <string>

using namespace boost;
using namespace local_time;
using namespace gregorian;

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
boost::local_time::local_date_time ProducerDataPeriod::getTime(const std::string& producer,
                                                               const std::string& timezone,
                                                               const Fmi::TimeZones& timezones,
                                                               eTime time_enum) const
{
  try
  {
    try
    {
      time_zone_ptr tz = timezones.time_zone_from_string(timezone);

      if (itsDataPeriod.find(producer) == itsDataPeriod.end())
        return local_date_time(not_a_date_time, tz);

      if (time_enum == STARTTIME)
        return local_date_time(itsDataPeriod.at(producer).begin(), tz);
      else
        return local_date_time(
            itsDataPeriod.at(producer).last() + boost::posix_time::microseconds(1), tz);
    }
    catch (...)
    {
      throw Spine::Exception(BCP, "Failed to construct local time for timezone '" + timezone + "'");
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::posix_time::ptime ProducerDataPeriod::getTime(const std::string& producer,
                                                     eTime time_enum) const
{
  try
  {
    if (itsDataPeriod.find(producer) != itsDataPeriod.end())
    {
      if (time_enum == STARTTIME)
        return itsDataPeriod.at(producer).begin();
      else
        return (itsDataPeriod.at(producer).last() + boost::posix_time::microseconds(1));
    }

    return not_a_date_time;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void ProducerDataPeriod::getQEngineDataPeriods(const Engine::Querydata::Engine& querydata,
                                               const TimeProducers& producers)
{
  try
  {
    for (const auto& areaproducers : producers)
    {
      for (const auto& producer : areaproducers)
      {
        auto period = querydata.getProducerTimePeriod(producer);
        if (!period.is_null())
          itsDataPeriod.insert(make_pair(producer, period));
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

#ifndef WITHOUT_OBSERVATION
void ProducerDataPeriod::getObsEngineDataPeriods(const Engine::Observation::Engine& observation,
                                                 const TimeProducers& producers,
                                                 const boost::posix_time::ptime& now)
{
  try
  {
    std::set<std::string> obsproducers = observation.getValidStationTypes();

    for (const auto& areaproducers : producers)
    {
      for (const auto& producer : areaproducers)
      {
        if (obsproducers.find(producer) == obsproducers.end())
          continue;

        itsDataPeriod.insert(make_pair(
            producer, boost::posix_time::time_period(now - boost::posix_time::hours(24), now)));
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
#endif

// localtime
boost::local_time::local_date_time ProducerDataPeriod::getLocalStartTime(
    const std::string& producer, const std::string& timezone, const Fmi::TimeZones& timezones) const
{
  try
  {
    return getTime(producer, timezone, timezones, STARTTIME);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// utc
boost::posix_time::ptime ProducerDataPeriod::getUTCStartTime(const std::string& producer) const
{
  try
  {
    return getTime(producer, STARTTIME);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// localtime
boost::local_time::local_date_time ProducerDataPeriod::getLocalEndTime(
    const std::string& producer, const std::string& timezone, const Fmi::TimeZones& timezones) const
{
  try
  {
    return getTime(producer, timezone, timezones, ENDTIME);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// utc
boost::posix_time::ptime ProducerDataPeriod::getUTCEndTime(const std::string& producer) const
{
  try
  {
    return getTime(producer, ENDTIME);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void ProducerDataPeriod::init(const State& state,
                              const Engine::Querydata::Engine& querydata,
#ifndef WITHOUT_OBSERVATION
                              const Engine::Observation::Engine* observation,
#endif
                              const TimeProducers& producers)
{
  try
  {
    itsDataPeriod.clear();
    getQEngineDataPeriods(querydata, producers);
#ifndef WITHOUT_OBSERVATION
    if (observation != nullptr)
      getObsEngineDataPeriods(*observation, producers, state.getTime());
#endif
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
