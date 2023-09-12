#include "UtilityFunctions.h"
#include "State.h"
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <timeseries/ParameterKeywords.h>
#include <timeseries/ParameterTools.h>
#include <engines/observation/ExternalAndMobileProducerId.h>
#include <engines/observation/Keywords.h>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
namespace UtilityFunctions
{

bool is_special_parameter(const std::string& paramname)
{
  if (paramname == ORIGINTIME_PARAM || paramname == LEVEL_PARAM)
    return false;

  return (TS::is_time_parameter(paramname) || TS::is_location_parameter(paramname));
}

void get_special_parameter_values(const std::string& paramname,
                                  int precision,
                                  const TS::TimeSeriesGenerator::LocalTimeList& tlist,
                                  const Spine::LocationPtr& loc,
                                  const Query& query,
                                  const State& state,
                                  const Fmi::TimeZones& timezones,
                                  TS::TimeSeriesPtr& result)
{
  bool is_time_parameter = TS::is_time_parameter(paramname);
  bool is_location_parameter = TS::is_location_parameter(paramname);

  for (const auto& timestep : tlist)
  {
    if (is_time_parameter)
    {
      TS::Value value = TS::time_parameter(paramname,
                                           timestep,
                                           state.getTime(),
                                           *loc,
                                           query.timezone,
                                           timezones,
                                           query.outlocale,
                                           *query.timeformatter,
                                           query.timestring);
      result->emplace_back(TS::TimedValue(timestep, value));
    }
    if (is_location_parameter)
    {
      TS::Value value = TS::location_parameter(
          loc, paramname, query.valueformatter, query.timezone, precision, query.crs);

      result->emplace_back(TS::TimedValue(timestep, value));
    }
  }
}

void get_special_parameter_values(const std::string& paramname,
                                  int precision,
                                  const TS::TimeSeriesGenerator::LocalTimeList& tlist,
                                  const Spine::LocationList& llist,
                                  const Query& query,
                                  const State& state,
                                  const Fmi::TimeZones& timezones,
                                  TS::TimeSeriesGroupPtr& result)
{
  bool is_time_parameter = TS::is_time_parameter(paramname);
  bool is_location_parameter = TS::is_location_parameter(paramname);
  for (const auto& loc : llist)
  {
    auto timeseries = TS::TimeSeries(state.getLocalTimePool());
    for (const auto& timestep : tlist)
    {
      if (is_time_parameter)
      {
        TS::Value value = TS::time_parameter(paramname,
                                             timestep,
                                             state.getTime(),
                                             *loc,
                                             query.timezone,
                                             timezones,
                                             query.outlocale,
                                             *query.timeformatter,
                                             query.timestring);
        timeseries.emplace_back(TS::TimedValue(timestep, value));
      }
      if (is_location_parameter)
      {
        TS::Value value = TS::location_parameter(
            loc, paramname, query.valueformatter, query.timezone, precision, query.crs);

        timeseries.emplace_back(TS::TimedValue(timestep, value));
      }
    }
    if (!timeseries.empty())
      result->push_back(
          TS::LonLatTimeSeries(Spine::LonLat(loc->longitude, loc->latitude), timeseries));
  }
}

bool is_mobile_producer(const std::string& producer)
{
  try
  {
    return (producer == Engine::Observation::ROADCLOUD_PRODUCER ||
            producer == Engine::Observation::TECONER_PRODUCER ||
            producer == Engine::Observation::FMI_IOT_PRODUCER ||
            producer == Engine::Observation::NETATMO_PRODUCER ||
            producer == Engine::Observation::BK_HYDROMETA_PRODUCER);
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

bool is_flash_producer(const std::string& producer)
{
  try
  {
    return (producer == FLASH_PRODUCER);
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

bool is_icebuoy_or_copernicus_producer(const std::string& producer)
{
  try
  {
    return (producer == ICEBUOY_PRODUCER || producer == COPERNICUS_PRODUCER);
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

bool is_flash_or_mobile_producer(const std::string& producer)
{
  try
  {
    return (is_flash_producer(producer) || is_mobile_producer(producer) ||
            is_icebuoy_or_copernicus_producer(producer));
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

}  // namespace UtilityFunctions
}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
