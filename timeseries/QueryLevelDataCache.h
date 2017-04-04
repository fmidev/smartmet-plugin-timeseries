// ======================================================================
/*!
 * \brief Structures for query level cache. From QEngine we fetch results
 * parameter by parameter. During the query parameters are stored in cache,
 * to prevent multiple queries per parameter, e.g. t2m, min_t(t2m:1h)
 *
 */
// ======================================================================

#pragma once

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
struct QueryLevelDataCache
{
  typedef std::pair<float, std::string> PressureLevelParameterPair;
  typedef std::map<PressureLevelParameterPair, SmartMet::Spine::TimeSeries::TimeSeriesPtr>
      ParameterTimeSeriesMap;
  typedef std::map<PressureLevelParameterPair, SmartMet::Spine::TimeSeries::TimeSeriesGroupPtr>
      ParameterTimeSeriesGroupMap;

  ParameterTimeSeriesMap itsTimeSeries;
  ParameterTimeSeriesGroupMap itsTimeSeriesGroups;
};

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
