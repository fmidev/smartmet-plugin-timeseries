// ======================================================================
/*!
 * \brief Structure for parameter precisions
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
struct Precision
{
  typedef std::map<std::string, int> Map;

  int default_precision;
  Map parameter_precisions;

  Precision() : default_precision(0), parameter_precisions() {}
};

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
