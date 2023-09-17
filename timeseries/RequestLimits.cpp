#include "RequestLimits.h"
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>

namespace SmartMet
{
namespace TimeSeries
{
void check_request_limit(const RequestLimits &limits, std::size_t amount, RequestLimitMember member)
{
  std::string description;
  if (member == RequestLimitMember::LOCATIONS)
  {
    if (limits.maxlocations > 0 && limits.maxlocations < amount)
      description =
          ("Too many locations: Max " + Fmi::to_string(limits.maxlocations) + " allowed!");
  }
  else if (member == RequestLimitMember::PARAMETERS)
  {
    if (limits.maxparameters > 0 && limits.maxparameters < amount)
      description =
          ("Too many parameters: Max " + Fmi::to_string(limits.maxparameters) + " allowed!");
  }
  else if (member == RequestLimitMember::TIMESTEPS)
  {
    if (limits.maxtimes > 0 && limits.maxtimes < amount)
      description = ("Too many timesteps: Max " + Fmi::to_string(limits.maxtimes) + " allowed!");
  }
  else if (member == RequestLimitMember::LEVELS)
  {
    if (limits.maxlevels > 0 && limits.maxlevels < amount)
      description = ("Too many levels: Max " + Fmi::to_string(limits.maxlevels) + " allowed!");
  }
  else if (member == RequestLimitMember::ELEMENTS)
  {
    if (limits.maxelements > 0 && limits.maxelements < amount)
      description = ("Too many elements: Max " + Fmi::to_string(limits.maxelements) + " allowed!");
  }

  if (!description.empty())
    throw Fmi::Exception::Trace(BCP, "RequestLimitError").addParameter("description", description);
}

}  // namespace TimeSeries
}  // namespace SmartMet

// ======================================================================
