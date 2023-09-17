// ======================================================================
/*!
 * \brief Declaration of Stat class
 *
 * Stat class implements statistical functions.
 * Data can be passed in a constructor call or in addData, SetData functions.
 * Both plain double values and double-timestamp pairs are accepted as input.
 * Timestap can be either boost::local_time::local_date_time or boost::posix_time::ptime.
 * If valid value-timestamp pairs are passed weighted version of statistical function is called
 * unless explicitly denied by calling useWeights(false) function before.
 * Exception is sum and integ-functions. In integ-function weights are always used.
 * In sum-function weight for all values is 1.0.
 * If passed values contain missing value, statistics can not be calculated and value of
 * itsMissingValue
 * data member is returned. Missing value can be passed in constructor call or in
 * setMissingValue(double) function.
 * If useDegrees(true) is called values are handled as degrees, in that case result value is always
 * between 0...360.
 *
 */
// ======================================================================

#pragma once

#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <limits>

namespace SmartMet
{
namespace TimeSeries
{
namespace Stat
{
using boost::posix_time::not_a_date_time;

struct DataItem
{
  DataItem(boost::posix_time::ptime t, double v, double w = 1.0) : time(t), value(v), weight(w) {}

  boost::posix_time::ptime time{not_a_date_time};
  double value = std::numeric_limits<double>::quiet_NaN();
  double weight = 1.0;
};

std::ostream& operator<<(std::ostream& os, const DataItem& item);

using DataVector = std::vector<DataItem>;
using TimeValue = std::pair<boost::posix_time::ptime, double>;
using TimeValueVector = std::vector<TimeValue>;
using LocalTimeValue = std::pair<boost::local_time::local_date_time, double>;
using LocalTimeValueVector = std::vector<LocalTimeValue>;

class Stat
{
 public:
  Stat(double theMissingValue = std::numeric_limits<double>::quiet_NaN());
  Stat(const std::vector<double>& theValues,
       double theMissingValue = std::numeric_limits<double>::quiet_NaN());
  Stat(const LocalTimeValueVector& theValues,
       double theMissingValue = std::numeric_limits<double>::quiet_NaN());
  Stat(const TimeValueVector& theValues,
       double theMissingValue = std::numeric_limits<double>::quiet_NaN());
  Stat(const DataVector& theValues,
       double theMissingValue = std::numeric_limits<double>::quiet_NaN());

  void setData(const DataVector& theValues);
  void addData(double theValue);
  void addData(const boost::local_time::local_date_time& theTime, double theValue);
  void addData(const boost::posix_time::ptime& theTime, double theValue);
  void addData(const std::vector<double>& theValues);
  void addData(const DataItem& theValue);
  void setMissingValue(double theMissingValue) { itsMissingValue = theMissingValue; }
  void useWeights(bool theWeights = true) { itsWeights = theWeights; }
  void useDegrees(bool theDegrees = true) { itsDegrees = theDegrees; }
  void clear();

  double integ(const boost::posix_time::ptime& startTime = not_a_date_time,
               const boost::posix_time::ptime& endTime = not_a_date_time) const;
  double sum(const boost::posix_time::ptime& startTime = not_a_date_time,
             const boost::posix_time::ptime& endTime = not_a_date_time) const;
  double min(const boost::posix_time::ptime& startTime = not_a_date_time,
             const boost::posix_time::ptime& endTime = not_a_date_time) const;
  double mean(const boost::posix_time::ptime& startTime = not_a_date_time,
              const boost::posix_time::ptime& endTime = not_a_date_time) const;
  double max(const boost::posix_time::ptime& startTime = not_a_date_time,
             const boost::posix_time::ptime& endTime = not_a_date_time) const;
  double change(const boost::posix_time::ptime& startTime = not_a_date_time,
                const boost::posix_time::ptime& endTime = not_a_date_time) const;
  double trend(const boost::posix_time::ptime& startTime = not_a_date_time,
               const boost::posix_time::ptime& endTime = not_a_date_time) const;
  unsigned int count(double lowerLimit,
                     double upperLimit,
                     const boost::posix_time::ptime& startTime = not_a_date_time,
                     const boost::posix_time::ptime& endTime = not_a_date_time) const;
  double percentage(double lowerLimit,
                    double upperLimit,
                    const boost::posix_time::ptime& startTime = not_a_date_time,
                    const boost::posix_time::ptime& endTime = not_a_date_time) const;
  double median(const boost::posix_time::ptime& startTime = not_a_date_time,
                const boost::posix_time::ptime& endTime = not_a_date_time) const;
  double variance(const boost::posix_time::ptime& startTime = not_a_date_time,
                  const boost::posix_time::ptime& endTime = not_a_date_time) const;
  double stddev(const boost::posix_time::ptime& startTime = not_a_date_time,
                const boost::posix_time::ptime& endTime = not_a_date_time) const;
  double nearest(const boost::posix_time::ptime& timestep,
                 const boost::posix_time::ptime& startTime = not_a_date_time,
                 const boost::posix_time::ptime& endTime = not_a_date_time) const;
  double interpolate(const boost::posix_time::ptime& timestep,
                     const boost::posix_time::ptime& startTime = not_a_date_time,
                     const boost::posix_time::ptime& endTime = not_a_date_time) const;

 private:
  double stddev_dir(const boost::posix_time::ptime& startTime = not_a_date_time,
                    const boost::posix_time::ptime& endTime = not_a_date_time) const;
  bool get_subvector(DataVector& subvector,
                     const boost::posix_time::ptime& startTime = not_a_date_time,
                     const boost::posix_time::ptime& endTime = not_a_date_time,
                     bool useWeights = true) const;
  void calculate_weights();
  bool invalid_timestamps() const;

  DataVector itsData;
  double itsMissingValue = 0;
  bool itsWeights = false;
  bool itsDegrees = false;
};

}  // namespace Stat
}  // namespace TimeSeries
}  // namespace SmartMet

// ======================================================================
