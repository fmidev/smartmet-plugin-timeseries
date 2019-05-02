#pragma once
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <macgyver/TimeFormatter.h>
#include <macgyver/TimeZones.h>
#include <spine/Exception.h>
#include <spine/Location.h>
#include <spine/OptionParsers.h>
#include <spine/Parameter.h>
#include <spine/TimeSeries.h>
#include <spine/ValueFormatter.h>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
bool special(const Spine::Parameter& theParam);
bool parameter_is_arithmetic(const Spine::Parameter& theParameter);
bool is_plain_location_query(const Spine::OptionParsers::ParameterList& theParams);
bool is_location_parameter(const std::string& paramname);

std::string location_parameter(const Spine::LocationPtr loc,
                               const std::string paramName,
                               const Spine::ValueFormatter& valueformatter,
                               const std::string& timezone,
                               int precision);

Spine::TimeSeries::Value time_parameter(const std::string paramname,
                                        const boost::local_time::local_date_time& ldt,
                                        const boost::posix_time::ptime now,
                                        const Spine::Location& loc,
                                        const std::string& timezone,
                                        const Fmi::TimeZones& timezones,
                                        const std::locale& outlocale,
                                        const Fmi::TimeFormatter& timeformatter,
                                        const std::string& timestring);

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
