// ======================================================================
/*!
 * \brief Smartmet TimeSeries data postprocessing functions
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
namespace PostProcessing
{
void store_data(TS::TimeSeriesVectorPtr aggregatedData, Query& query, TS::OutputData& outputData);
void store_data(std::vector<TS::TimeSeriesData>& aggregatedData,
                Query& query,
                TS::OutputData& outputData);
void fill_table(Query& query, TS::OutputData& outputData, Spine::Table& table);
void fix_precisions(Query& masterquery, const ObsParameters& obsParameters);
}  // namespace PostProcessing
}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
