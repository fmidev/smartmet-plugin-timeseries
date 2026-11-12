// ======================================================================
/*!
 * \brief Smartmet TimeSeries data functions
 */
// ======================================================================

#pragma once

#include <spine/Location.h>
#include <timeseries/TimeSeries.h>
#include <timeseries/TimeSeriesGenerator.h>
#include <timeseries/TimeSeriesInclude.h>
#include <string>

namespace Fmi
{
class TimeZones;
}  // namespace Fmi
namespace SmartMet::Plugin::TimeSeries
{
class State;
struct Query;
}  // namespace SmartMet::Plugin::TimeSeries

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
namespace UtilityFunctions
{
bool is_special_parameter(const std::string& paramname);
void get_special_parameter_values(const std::string& paramname,
                                  int precision,
                                  const TS::TimeSeriesGenerator::LocalTimeList& tlist,
                                  const Spine::LocationPtr& loc,
                                  const Query& query,
                                  const State& state,
                                  const Fmi::TimeZones& timezones,
                                  const TS::TimeSeriesPtr& result);
void get_special_parameter_values(const std::string& paramname,
                                  int precision,
                                  const TS::TimeSeriesGenerator::LocalTimeList& tlist,
                                  const Spine::LocationList& llist,
                                  const Query& query,
                                  const State& state,
                                  const Fmi::TimeZones& timezones,
                                  const TS::TimeSeriesGroupPtr& result);
bool is_mobile_producer(const std::string& producer);
bool is_flash_producer(const std::string& producer);
bool is_icebuoy_or_copernicus_producer(const std::string& producer);
bool is_flash_or_mobile_producer(const std::string& producer);

}  // namespace UtilityFunctions
}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
