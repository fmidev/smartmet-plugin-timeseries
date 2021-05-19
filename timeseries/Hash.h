// ======================================================================
/*!
 * \brief Hash calculation utilities
 */
// ======================================================================

#pragma once
#include "State.h"
#include <boost/shared_ptr.hpp>
#include <macgyver/Hash.h>
#include <macgyver/StringConversion.h>
#include <list>
#include <map>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
// Objects with a member hash_value implementation
template <typename T>
inline std::size_t hash_value(const T& obj, const State& theState)
{
  return obj.hash_value(theState);
}

// Optional objects with a member hash_value implementation
template <typename T>
inline std::size_t hash_value(const boost::optional<T>& obj, const State& theState)
{
  if (!obj)
    return Fmi::hash_value(false);
  else
  {
    std::size_t hash = Fmi::hash_value(true);
    Fmi::hash_combine(hash, obj->hash_value(theState));
    return hash;
  }
}

// Shared objects with a member hash_value implementation
template <typename T>
inline std::size_t hash_value(const boost::shared_ptr<T>& obj, const State& theState)
{
  if (!obj)
    return Fmi::hash_value(false);
  else
  {
    std::size_t hash = Fmi::hash_value(true);
    Fmi::hash_combine(hash, hash_value(*obj, theState));
    return hash;
  }
}

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
