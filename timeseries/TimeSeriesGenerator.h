// ======================================================================
/*!
 * \brief Time series generator for forecasts and observations
 */
// ======================================================================

#pragma once

#include "TimeSeriesGeneratorOptions.h"

#include <boost/date_time/local_time/local_time.hpp>
#include <list>
#include <string>

namespace SmartMet
{
namespace TimeSeries
{
namespace TimeSeriesGenerator
{
using LocalTimeList = std::list<boost::local_time::local_date_time>;

LocalTimeList generate(const TimeSeriesGeneratorOptions& theOptions,
                       const boost::local_time::time_zone_ptr& theZone);

}  // namespace TimeSeriesGenerator
}  // namespace TimeSeries
}  // namespace SmartMet
