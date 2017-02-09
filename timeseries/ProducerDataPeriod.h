#pragma once

#include "Producers.h"

#include <engines/querydata/Engine.h>
#include <engines/observation/Engine.h>

#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/foreach.hpp>
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
  typedef std::map<std::string, boost::posix_time::time_period> DataPeriod;

  enum eTime
  {
    STARTTIME,
    ENDTIME
  };

  DataPeriod itsDataPeriod;

  boost::local_time::local_date_time getTime(const std::string& producer,
                                             const std::string& timezone,
                                             const Fmi::TimeZones& timezones,
                                             eTime time_enum) const;

  boost::posix_time::ptime getTime(const std::string& producer, eTime time_enum) const;

  void getQEngineDataPeriods(const SmartMet::Engine::Querydata::Engine& querydata,
                             const TimeProducers& producers);

  void getObsEngineDataPeriods(const SmartMet::Engine::Observation::Engine& observation,
                               const TimeProducers& producers,
                               const boost::posix_time::ptime& now);

 public:
  boost::local_time::local_date_time getLocalStartTime(const std::string& producer,
                                                       const std::string& timezone,
                                                       const Fmi::TimeZones& timezones) const;

  boost::posix_time::ptime getUTCStartTime(const std::string& producer) const;

  boost::local_time::local_date_time getLocalEndTime(const std::string& producer,
                                                     const std::string& timezone,
                                                     const Fmi::TimeZones& timezones) const;

  boost::posix_time::ptime getUTCEndTime(const std::string& producer) const;

  void init(const State& state,
            const SmartMet::Engine::Querydata::Engine& querydata,
            const SmartMet::Engine::Observation::Engine* observation,
            const TimeProducers& producers);

  std::string info()
  {
    std::string str;

    std::vector<std::string> producernames;

    // Retrieve all keys
    boost::copy(itsDataPeriod | boost::adaptors::map_keys, std::back_inserter(producernames));

    BOOST_FOREACH (const std::string& producer, producernames)
    {
      str.append("producer -> period: ")
          .append(producer)
          .append(" -> ")
          .append(to_simple_string(itsDataPeriod.at(producer)))
          .append("\n");
    }
    return str;
  }

};  // class ProducerDataPeriod

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
