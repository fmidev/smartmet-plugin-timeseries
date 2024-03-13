#pragma once

#include "Producers.h"

#include <engines/querydata/Engine.h>
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/Engine.h>
#endif

#include <macgyver/LocalDateTime.h>
#include <macgyver/DateTime.h>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <map>
#include <string>

namespace Fmi
{
class TimeZones;
}

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
class State;

class ProducerDataPeriod
{
 private:
  using DataPeriod = std::map<std::string, Fmi::TimePeriod>;

  enum eTime
  {
    STARTTIME,
    ENDTIME
  };

  DataPeriod itsDataPeriod;

  Fmi::LocalDateTime getTime(const std::string& producer,
                                             const std::string& timezone,
                                             const Fmi::TimeZones& timezones,
                                             eTime time_enum) const;

  Fmi::DateTime getTime(const std::string& producer, eTime time_enum) const;

  void getQEngineDataPeriods(const Engine::Querydata::Engine& querydata,
                             const TimeProducers& producers);

#ifndef WITHOUT_OBSERVATION
  void getObsEngineDataPeriods(const Engine::Observation::Engine& observation,
                               const TimeProducers& producers,
                               const Fmi::DateTime& now);
#endif

 public:
  Fmi::LocalDateTime getLocalStartTime(const std::string& producer,
                                                       const std::string& timezone,
                                                       const Fmi::TimeZones& timezones) const;

  Fmi::DateTime getUTCStartTime(const std::string& producer) const;

  Fmi::LocalDateTime getLocalEndTime(const std::string& producer,
                                                     const std::string& timezone,
                                                     const Fmi::TimeZones& timezones) const;

  Fmi::DateTime getUTCEndTime(const std::string& producer) const;

#ifndef WITHOUT_OBSERVATION
  void init(const State& state,
            const Engine::Querydata::Engine& querydata,
            const Engine::Observation::Engine* observation,
            const TimeProducers& producers);
#else
  void init(const State& state,
            const Engine::Querydata::Engine& querydata,
            const TimeProducers& producers);
#endif

  std::string info()
  {
    std::string str;

    std::vector<std::string> producernames;

    // Retrieve all keys
    boost::copy(itsDataPeriod | boost::adaptors::map_keys, std::back_inserter(producernames));

    for (const std::string& producer : producernames)
    {
      str.append("producer -> period: ")
          .append(producer)
          .append(" -> ")
          .append(Fmi::date_time::to_simple_string(itsDataPeriod.at(producer)))
          .append("\n");
    }
    return str;
  }

};  // class ProducerDataPeriod

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
