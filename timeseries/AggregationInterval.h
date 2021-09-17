// ======================================================================
/*!
 * \brief Structure for AggregationInterval
 *
 */
// ======================================================================

#pragma once

#include <iosfwd>
#include <map>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
struct AggregationInterval
{
  unsigned int behind;
  unsigned int ahead;

  AggregationInterval(unsigned int be = 0, unsigned int ah = 0) : behind(be), ahead(ah) {}
  friend std::ostream& operator<<(std::ostream& out, const AggregationInterval& interval);
};

using MaxAggregationIntervals = std::map<std::string, AggregationInterval>;

std::ostream& operator<<(std::ostream& out, const AggregationInterval& interval);

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
