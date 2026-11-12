// ======================================================================
/*!
 * \brief Smartmet TimeSeries data postprocessing functions
 */
// ======================================================================

#pragma once

#include "ObsParameter.h"
#include <timeseries/TimeSeries.h>
#include <timeseries/TimeSeriesInclude.h>
#include <timeseries/TimeSeriesUtility.h>
#include <vector>

namespace SmartMet::Plugin::TimeSeries
{
struct Query;
}  // namespace SmartMet::Plugin::TimeSeries
namespace SmartMet::Spine
{
class Table;
}  // namespace SmartMet::Spine

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
void fill_table(const Query& query, TS::OutputData& outputData, Spine::Table& table);
void fix_precisions(Query& masterquery, const ObsParameters& obsParameters);
}  // namespace PostProcessing
}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
