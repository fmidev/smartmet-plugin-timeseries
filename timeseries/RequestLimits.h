// ======================================================================
/*!
 * \brief Interface of class RequestLimits
 */
// ======================================================================

#pragma once
#include <cstddef>

namespace SmartMet
{
namespace TimeSeries
{
struct RequestLimits
{
  std::size_t maxlocations{0};
  std::size_t maxparameters{0};
  std::size_t maxtimes{0};
  std::size_t maxlevels{0};
  std::size_t maxelements{0};
};

enum class RequestLimitMember
{
  LOCATIONS,
  PARAMETERS,
  TIMESTEPS,
  LEVELS,
  ELEMENTS
};

void check_request_limit(const RequestLimits &limits,
                         std::size_t amount,
                         RequestLimitMember member);

}  // namespace TimeSeries
}  // namespace SmartMet

// ======================================================================
