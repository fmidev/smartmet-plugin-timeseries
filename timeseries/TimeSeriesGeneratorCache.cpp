#include "TimeSeriesGeneratorCache.h"
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>

namespace SmartMet
{
namespace TimeSeries
{
// ----------------------------------------------------------------------
/*!
 * \brief Resize the cache
 *
 * The user is expected to resize the cache to a user configurable
 * value during startup.
 */
// ----------------------------------------------------------------------

void TimeSeriesGeneratorCache::resize(std::size_t theSize) const
{
  try
  {
    itsCache.resize(theSize);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the time series
 *
 * Use cache if possible, otherwise generate and cache
 */
// ----------------------------------------------------------------------

TimeSeriesGeneratorCache::TimeList TimeSeriesGeneratorCache::generate(
    const TimeSeriesGeneratorOptions& theOptions,
    const boost::local_time::time_zone_ptr& theZone) const
{
  try
  {
    // hash value for the query
    std::size_t hash = theOptions.hash_value();
    Fmi::hash_combine(hash, Fmi::hash_value(theZone));

    // use cached result if possible
    auto cached_result = itsCache.find(hash);
    if (cached_result)
      return *cached_result;

    // generate time series and cache it for future use
    TimeList series(
        new TimeSeriesGenerator::LocalTimeList(TimeSeriesGenerator::generate(theOptions, theZone)));
    itsCache.insert(hash, series);
    return series;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace TimeSeries
}  // namespace SmartMet
