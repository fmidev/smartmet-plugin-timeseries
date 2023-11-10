#include "ProducerDataPeriod.h"
#include "State.h"

#include <macgyver/Exception.h>
#include <macgyver/TimeZones.h>

#include <macgyver/LocalDateTime.h>
#include <macgyver/DateTime.h>
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
Fmi::LocalDateTime ProducerDataPeriod::getTime(const std::string& producer,
                                                               const std::string& timezone,
                                                               const Fmi::TimeZones& timezones,
                                                               eTime time_enum) const
{
  try
  {
    try
    {
      Fmi::TimeZonePtr tz = timezones.time_zone_from_string(timezone);

      if (itsDataPeriod.find(producer) == itsDataPeriod.end())
        return Fmi::LocalDateTime(not_a_date_time, tz);

      if (time_enum == STARTTIME)
        return {itsDataPeriod.at(producer).begin(), tz};

      return {itsDataPeriod.at(producer).last() + boost::posix_time::microseconds(1), tz};
    }
    catch (...)
    {
      throw Fmi::Exception(BCP, "Failed to construct local time for timezone '" + timezone + "'");
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Fmi::DateTime ProducerDataPeriod::getTime(const std::string& producer,
                                                     eTime time_enum) const
{
  try
  {
    if (itsDataPeriod.find(producer) != itsDataPeriod.end())
    {
      if (time_enum == STARTTIME)
        return itsDataPeriod.at(producer).begin();

      return (itsDataPeriod.at(producer).last() + boost::posix_time::microseconds(1));
    }

    return not_a_date_time;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

#ifndef WITHOUT_OBSERVATION
void ProducerDataPeriod::getObsEngineDataPeriods(const Engine::Observation::Engine& observation,
                                                 const TimeProducers& producers,
                                                 const Fmi::DateTime& now)
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
            producer, boost::posix_time::time_period(now - Fmi::Hours(24), now)));
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
#endif

// localtime
Fmi::LocalDateTime ProducerDataPeriod::getLocalStartTime(
    const std::string& producer, const std::string& timezone, const Fmi::TimeZones& timezones) const
{
  try
  {
    return getTime(producer, timezone, timezones, STARTTIME);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// utc
Fmi::DateTime ProducerDataPeriod::getUTCStartTime(const std::string& producer) const
{
  try
  {
    return getTime(producer, STARTTIME);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// localtime
Fmi::LocalDateTime ProducerDataPeriod::getLocalEndTime(
    const std::string& producer, const std::string& timezone, const Fmi::TimeZones& timezones) const
{
  try
  {
    return getTime(producer, timezone, timezones, ENDTIME);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// utc
Fmi::DateTime ProducerDataPeriod::getUTCEndTime(const std::string& producer) const
{
  try
  {
    return getTime(producer, ENDTIME);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
