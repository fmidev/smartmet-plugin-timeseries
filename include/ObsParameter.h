// ======================================================================
/*!
 * \brief Structure for Obsengine parameter
 *
 */
// ======================================================================

#pragma once

#include <spine/ParameterFactory.h>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
struct ObsParameter
{
  SmartMet::Spine::Parameter param;
  SmartMet::Spine::ParameterFunctions functions;
  unsigned int data_column;
  bool duplicate;

  ObsParameter(SmartMet::Spine::Parameter p,
               SmartMet::Spine::ParameterFunctions funcs,
               unsigned int dcol,
               bool dup)
      : param(p), functions(funcs), data_column(dcol), duplicate(dup)
  {
  }
};

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
