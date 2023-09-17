#pragma once

#include "OptionParsers.h"
#include "TimeSeriesInclude.h"
#include <macgyver/Exception.h>
#include <macgyver/TimeFormatter.h>
#include <macgyver/TimeZones.h>
#include <spine/Location.h>
#include <spine/Parameter.h>

namespace Fmi
{
class ValueFormatter;
}

namespace SmartMet
{
namespace TimeSeries
{
bool special(const Spine::Parameter& theParam);

bool parameter_is_arithmetic(const Spine::Parameter& theParameter);

bool is_plain_location_query(const OptionParsers::ParameterList& theParams);

bool is_location_parameter(const std::string& paramname);

bool is_time_parameter(std::string paramname);

std::string location_parameter(const Spine::LocationPtr& loc,
                               const std::string& paramName,
                               const Fmi::ValueFormatter& valueformatter,
                               const std::string& timezone,
                               int precision,
                               const std::string& crs = "EPSG:4326");

TS::Value time_parameter(const std::string& paramname,
                         const boost::local_time::local_date_time& ldt,
                         const boost::posix_time::ptime& now,
                         const Spine::Location& loc,
                         const std::string& timezone,
                         const Fmi::TimeZones& timezones,
                         const std::locale& outlocale,
                         const Fmi::TimeFormatter& timeformatter,
                         const std::string& timestring);

Spine::Parameter makeParameter(const std::string& name);

bool is_special_parameter(const std::string& name);

Spine::Parameter get_query_param(const Spine::Parameter& parameter);
void transform_wgs84_coordinates(const std::string& name,
                                 const std::string& target_crs,
                                 const Spine::Location& loc,
                                 TS::TimeSeries& tseries);
void transform_wgs84_coordinates(const std::string& name,
                                 const std::string& crs,
                                 TS::TimeSeriesGroup& tsg);
std::string get_parameter_id(const Spine::Parameter& parameter);

}  // namespace TimeSeries
}  // namespace SmartMet
