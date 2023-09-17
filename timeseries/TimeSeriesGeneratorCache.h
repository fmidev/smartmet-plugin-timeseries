// ======================================================================
/*!
 * \brief Cache for generated timeseries
 */
// ======================================================================

#pragma once

#include "TimeSeriesGenerator.h"
#include "TimeSeriesGeneratorOptions.h"
#include <macgyver/Cache.h>

namespace SmartMet
{
namespace TimeSeries
{
class TimeSeriesGeneratorCache
{
 public:
  using TimeList = std::shared_ptr<TimeSeriesGenerator::LocalTimeList>;

  void resize(std::size_t theSize) const;

  TimeList generate(const TimeSeriesGeneratorOptions& theOptions,
                    const boost::local_time::time_zone_ptr& theZone) const;

  Fmi::Cache::CacheStats getCacheStats() const { return itsCache.statistics(); }

 private:
  mutable Fmi::Cache::Cache<std::size_t, TimeList> itsCache;
};

}  // namespace TimeSeries
}  // namespace SmartMet
