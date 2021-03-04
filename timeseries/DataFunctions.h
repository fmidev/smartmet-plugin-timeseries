// ======================================================================
/*!
 * \brief Smartmet TimeSeries data functions
 */
// ======================================================================

#pragma once

#include "ObsParameter.h"
#include "Query.h"

#include <spine/ParameterFunction.h>
#include <spine/TimeSeriesAggregator.h>
#include <spine/TimeSeriesGenerator.h>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
/*** typedefs ***/
typedef boost::variant<Spine::TimeSeries::TimeSeriesPtr,
                       Spine::TimeSeries::TimeSeriesVectorPtr,
                       Spine::TimeSeries::TimeSeriesGroupPtr>
    TimeSeriesData;
// typedef std::vector<std::pair<std::string, std::vector<TimeSeriesData> > > OutputData;
using OutputData = std::vector<std::pair<std::string, std::vector<TimeSeriesData> > >;
using PressureLevelParameterPair = std::pair<int, std::string>;
using ParameterTimeSeriesMap =
    std::map<PressureLevelParameterPair, Spine::TimeSeries::TimeSeriesPtr>;
using ParameterTimeSeriesGroupMap =
    std::map<PressureLevelParameterPair, Spine::TimeSeries::TimeSeriesGroupPtr>;
using ObsParameters = std::vector<ObsParameter>;
using FmisidTSVectorPair = std::pair<int, Spine::TimeSeries::TimeSeriesVectorPtr>;
using TimeSeriesByLocation = std::vector<FmisidTSVectorPair>;
namespace DataFunctions
{
/*** functions ***/
Spine::TimeSeries::TimeSeriesPtr erase_redundant_timesteps(
    Spine::TimeSeries::TimeSeriesPtr ts,
    const Spine::TimeSeriesGenerator::LocalTimeList& timesteps);
Spine::TimeSeries::TimeSeriesVectorPtr erase_redundant_timesteps(
    Spine::TimeSeries::TimeSeriesVectorPtr tsv,
    const Spine::TimeSeriesGenerator::LocalTimeList& timesteps);
Spine::TimeSeries::TimeSeriesGroupPtr erase_redundant_timesteps(
    Spine::TimeSeries::TimeSeriesGroupPtr tsg,
    const Spine::TimeSeriesGenerator::LocalTimeList& timesteps);
void store_data(Spine::TimeSeries::TimeSeriesVectorPtr aggregatedData,
                Query& query,
                OutputData& outputData);
void store_data(std::vector<TimeSeriesData>& aggregatedData, Query& query, OutputData& outputData);

std::ostream& operator<<(std::ostream& os, const TimeSeriesData& tsdata);

template <typename T>
T aggregate(const T& raw_data, const Spine::ParameterFunctions& pf)
{
  try
  {
    T aggregated_data = Spine::TimeSeries::aggregate(*raw_data, pf);

    return aggregated_data;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace DataFunctions
}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
