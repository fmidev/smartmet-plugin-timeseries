#include "TimeSeries.h"
#include <boost/make_shared.hpp>
#include <macgyver/Hash.h>

namespace SmartMet
{
namespace TimeSeries
{
std::ostream& operator<<(std::ostream& os, const LocalTimePool& localTimePool)
{
  localTimePool.print(os);
  return os;
}

const boost::local_time::local_date_time& LocalTimePool::create(
    const boost::posix_time::ptime& t, const boost::local_time::time_zone_ptr& tz)
{
  auto key = Fmi::hash_value(t);
  Fmi::hash_combine(key, Fmi::hash_value(tz));

  // TODO: In C++17 should use try_emplace

  auto pos = localtimes.find(key);
  if (pos != localtimes.end())
    return pos->second;

  // Note: iterators may be invalidated by a rehash caused by this, but references are not
  auto pos_bool = localtimes.emplace(key, boost::local_time::local_date_time(t, tz));

  assert(pos_bool.second == true);
  return pos_bool.first->second;
}

size_t LocalTimePool::size() const
{
  return localtimes.size();
}

void LocalTimePool::print(std::ostream& os) const
{
  for (const auto& item : localtimes)
    os << item.first << " -> " << item.second << std::endl;
}

TimeSeries::TimeSeries(LocalTimePoolPtr time_pool) : local_time_pool(time_pool) {}

void TimeSeries::emplace_back(const TimedValue& tv)
{
  TimedValueVector::emplace_back(local_time_pool->create(tv.time.utc_time(), tv.time.zone()),
                                 tv.value);
}

void TimeSeries::push_back(const TimedValue& tv)
{
  TimedValueVector::push_back(
      TimedValue(local_time_pool->create(tv.time.utc_time(), tv.time.zone()), tv.value));
}

TimedValueVector::iterator TimeSeries::insert(TimedValueVector::iterator pos, const TimedValue& tv)
{
  return TimedValueVector::insert(
      pos, TimedValue(local_time_pool->create(tv.time.utc_time(), tv.time.zone()), tv.value));
}

void TimeSeries::insert(TimedValueVector::iterator pos,
                        TimedValueVector::iterator first,
                        TimedValueVector::iterator last)
{
  TimedValueVector::insert(pos, first, last);
}

void TimeSeries::insert(TimedValueVector::iterator pos,
                        TimedValueVector::const_iterator first,
                        TimedValueVector::const_iterator last)
{
  TimedValueVector::insert(pos, first, last);
}

TimeSeries& TimeSeries::operator=(const TimeSeries& ts)
{
  if (this != &ts)
  {
    clear();
    TimedValueVector::insert(end(), ts.begin(), ts.end());
    local_time_pool = ts.local_time_pool;
  }

  return *this;
}

}  // namespace TimeSeries
}  // namespace SmartMet
