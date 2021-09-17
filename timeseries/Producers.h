// ======================================================================

#pragma once

#include <list>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
using AreaProducers = std::list<std::string>;
using TimeProducers = std::list<AreaProducers>;

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
