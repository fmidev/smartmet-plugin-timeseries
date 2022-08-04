// ======================================================================
/*!
 * \brief Smartmet TimeSeries data functions
 */
// ======================================================================

#pragma once

#include "ObsParameter.h"
#include "Query.h"
#include <timeseries/TimeSeriesInclude.h>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
namespace UtilityFunctions
{
void store_data(TS::TimeSeriesVectorPtr aggregatedData, Query& query, TS::OutputData& outputData);
void store_data(std::vector<TS::TimeSeriesData>& aggregatedData,
                Query& query,
                TS::OutputData& outputData);

}  // namespace UtilityFunctions
}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
