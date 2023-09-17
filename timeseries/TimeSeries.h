// ======================================================================
/*!
 * \brief Interface of class ValueAggregator
 */
// ======================================================================

#pragma once

#include <boost/date_time/local_time/local_time.hpp>
#include <boost/variant.hpp>
#include <spine/LonLat.h>
#include <spine/None.h>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace SmartMet
{
namespace TimeSeries
{
using None = ::SmartMet::Spine::None;
using LonLat = ::SmartMet::Spine::LonLat;
using LonLatFormat = ::SmartMet::Spine::LonLatFormat;

// data variable for qengine, obsengine
using Value_ = boost::variant<Spine::None,
                              std::string,
                              double,
                              int,
                              Spine::LonLat,
                              boost::local_time::local_date_time>;

struct Value : public Value_
{
  Value() = default;

  inline Value(const Spine::None& /* none */) : Value() {}

  inline Value(const std::string& s) : Value_(s) {}

  inline Value(const char* s) : Value_(std::string(s)) {}

  template <typename ArgType>
  inline Value(ArgType x, typename std::enable_if<std::is_integral<ArgType>::value, int>::type = 0)
      : Value_(int(x))
  {
  }

  template <typename ArgType>
  inline Value(ArgType x,
               typename std::enable_if<std::is_floating_point<ArgType>::value, int>::type = 0)
      : Value_(double(x))
  {
  }

  inline Value(const Spine::LonLat& x) : Value_(x) {}

  inline Value(const boost::local_time::local_date_time& x) : Value_(x) {}

  inline Value(const Value&) = default;

  inline Value& operator=(const Value&) = default;

  bool operator==(const Value& x) const { return this->Value_::operator==((Value_&)x); }

  bool operator!=(const Value& x) const { return this->Value_::operator!=((Value_&)x); }
};

// Local time pool
class LocalTimePool
{
 public:
  const boost::local_time::local_date_time& create(const boost::posix_time::ptime& t,
                                                   const boost::local_time::time_zone_ptr& tz);
  size_t size() const;
  void print(std::ostream& os) const;

 private:
  std::unordered_map<std::size_t, boost::local_time::local_date_time> localtimes;
};

using LocalTimePoolPtr = boost::shared_ptr<LocalTimePool>;

struct TimedValue
{
  TimedValue(const boost::local_time::local_date_time& timestamp, const Value& val)
      : time(const_cast<boost::local_time::local_date_time&>(timestamp)), value(val)
  {
  }
  TimedValue(const TimedValue& tv)
      : time(const_cast<boost::local_time::local_date_time&>(tv.time)), value(tv.value)
  {
  }
  TimedValue& operator=(const TimedValue& tv)
  {
    if (this != &tv)
    {
      time = const_cast<boost::local_time::local_date_time&>(tv.time);
      value = tv.value;
    }
    return *this;
  }

  boost::local_time::local_date_time& time;
  Value value;
};

using TimedValueVector = std::vector<TimedValue>;

class TimeSeries : public TimedValueVector
{
 public:
  TimeSeries(LocalTimePoolPtr time_pool);
  TimeSeries(const TimeSeries&) = default;
  void emplace_back(const TimedValue& tv);
  void push_back(const TimedValue& tv);
  TimedValueVector::iterator insert(TimedValueVector::iterator pos, const TimedValue& tv);
  void insert(TimedValueVector::iterator pos,
              TimedValueVector::iterator first,
              TimedValueVector::iterator last);
  void insert(TimedValueVector::iterator pos,
              TimedValueVector::const_iterator first,
              TimedValueVector::const_iterator last);
  TimeSeries& operator=(const TimeSeries& ts);

  LocalTimePoolPtr getLocalTimePool() const { return local_time_pool; }

 private:
  LocalTimePoolPtr local_time_pool{nullptr};
};

// one time series
using TimeSeriesPtr = boost::shared_ptr<TimeSeries>;

// time series result variable for an area
struct LonLatTimeSeries
{
  LonLatTimeSeries(const Spine::LonLat& coord, const TimeSeries& ts) : lonlat(coord), timeseries(ts)
  {
  }
  Spine::LonLat lonlat;
  TimeSeries timeseries;
};

// several coordinate-time series pairs
using TimeSeriesGroup = std::vector<LonLatTimeSeries>;
using TimeSeriesGroupPtr = boost::shared_ptr<TimeSeriesGroup>;

// time series vector
using TimeSeriesVector = std::vector<TimeSeries>;
using TimeSeriesVectorPtr = boost::shared_ptr<TimeSeriesVector>;

std::ostream& operator<<(std::ostream& os, const LocalTimePool& localTimePool);

}  // namespace TimeSeries
}  // namespace SmartMet

// ======================================================================
