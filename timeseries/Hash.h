// ======================================================================
/*!
 * \brief Hash calculation utilities
 */
// ======================================================================

#pragma once
#include "State.h"
#include <macgyver/String.h>
#include <boost/functional/hash.hpp>
#include <boost/shared_ptr.hpp>
#include <list>
#include <map>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
// Times
inline std::size_t hash_value(const boost::posix_time::ptime& time)
{
  return boost::hash_value(Fmi::to_iso_string(time));
}

inline std::size_t hash_value(const boost::local_time::local_date_time& time)
{
#if 0
	  // to_posix_string uses string streams and is hence slow
	  return boost::hash_value( Fmi::to_iso_string(time.local_time()) + time.zone()->to_posix_string() );
#else
  return boost::hash_value(Fmi::to_iso_string(time.local_time()) + time.zone()->std_zone_name());
#endif
}

// Normal objects

template <typename T>
inline std::size_t hash_value(const T& obj)
{
  return boost::hash_value(obj);
}

// Optional objects with a normal hash_value implementation
template <typename T>
inline std::size_t hash_value(const boost::optional<T>& obj)
{
  if (!obj)
    return boost::hash_value(false);
  else
  {
    std::size_t hash = boost::hash_value(true);
    boost::hash_combine(hash, hash_value(*obj));
    return hash;
  }
}

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
    return boost::hash_value(false);
  else
  {
    std::size_t hash = boost::hash_value(true);
    boost::hash_combine(hash, obj->hash_value(theState));
    return hash;
  }
}

// Shared objects with a member hash_value implementation
template <typename T>
inline std::size_t hash_value(const boost::shared_ptr<T>& obj, const State& theState)
{
  if (!obj)
    return boost::hash_value(false);
  else
  {
    std::size_t hash = boost::hash_value(true);
    boost::hash_combine(hash, hash_value(*obj, theState));
    return hash;
  }
}

// Vectors of objects
template <typename T>
inline std::size_t hash_value(const std::vector<T>& objs)
{
  std::size_t hash = 0;
  for (const auto& obj : objs)
    boost::hash_combine(hash, hash_value(obj));
  return hash;
}

// Lists of objects
template <typename T>
inline std::size_t hash_value(const std::list<T>& objs)
{
  std::size_t hash = 0;
  for (const auto& obj : objs)
    boost::hash_combine(hash, hash_value(obj));
  return hash;
}

// Maps
template <typename T, typename S>
inline std::size_t hash_value(const std::map<T, S>& objs)
{
  std::size_t hash = 0;
  for (const auto& obj : objs)
  {
    boost::hash_combine(hash, hash_value(obj.first));
    boost::hash_combine(hash, hash_value(obj.second));
  }
  return hash;
}

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
