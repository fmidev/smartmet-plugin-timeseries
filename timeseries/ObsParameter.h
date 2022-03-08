// ======================================================================
/*!
 * \brief Structure for Obsengine parameter
 *
 */
// ======================================================================

#pragma once

#include <spine/ParameterFactory.h>
#include <timeseries/TimeSeriesInclude.h>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
struct ObsParameter
{
  Spine::Parameter param;
  TS::DataFunctions functions;
  unsigned int data_column;
  bool duplicate;

  ObsParameter(Spine::Parameter p,
               TS::DataFunctions funcs,
               unsigned int dcol,
               bool dup)
      : param(p), functions(funcs), data_column(dcol), duplicate(dup)
  {
  }
};

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
