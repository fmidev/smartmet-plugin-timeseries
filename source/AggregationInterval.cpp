#include "AggregationInterval.h"
#include <iostream>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
std::ostream& operator<<(std::ostream& out, const AggregationInterval& interval)
{
  out << "aggregation =\t[" << interval.behind << "..." << interval.ahead << "]\n";
  return out;
}

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
