#include "Stat.h"
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/weighted_mean.hpp>
#include <boost/accumulators/statistics/weighted_median.hpp>
#include <boost/accumulators/statistics/weighted_variance.hpp>
#include <macgyver/Exception.h>
#include <cmath>
#include <iterator>
#include <numeric>
#include <stdexcept>

using namespace boost::posix_time;

namespace SmartMet
{
namespace TimeSeries
{
namespace Stat
{
#define MODULO_VALUE_360 360.0
using DataIterator = DataVector::iterator;
using DataConstIterator = DataVector::const_iterator;

using namespace boost::accumulators;
using namespace std;

namespace
{

bool comp_value(const DataItem& data1, const DataItem& data2)
{
  try
  {
    return data1.value < data2.value;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool comp_time(const DataItem& data1, const DataItem& data2)
{
  try
  {
    return data1.time < data2.time;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

double interpolate_mod_360(double first_value,
                           double second_value,
                           double time_diff_sec,
                           double time_diff_to_timestep_sec)
{
  double value_diff = second_value - first_value;
  double slope = 0.0;
  if (value_diff > 180)  // as in 10 --> 350
    slope = (value_diff - 360) / time_diff_sec;
  else if (value_diff < -180)  // as in 350 --> 10
    slope = (value_diff + 360) / time_diff_sec;
  else
    slope = value_diff / time_diff_sec;

  double interpolated_value = first_value + slope * time_diff_to_timestep_sec;
  if (interpolated_value < 0)
    interpolated_value += 360;
  if (interpolated_value >= 360)
    interpolated_value -= 360;
  return interpolated_value;
}

double interpolate_normal(double first_value,
                          double second_value,
                          double time_diff_sec,
                          double time_diff_to_timestep_sec)
{
  double value_diff = second_value - first_value;
  double slope = value_diff / time_diff_sec;
  double interpolated_value = (first_value + (slope * time_diff_to_timestep_sec));

#ifdef MYDEBUG
  std::cout << "first_time: " << first_time << ",second_time: " << second_time
            << ", timestep: " << timestep << ", first_value: " << first_value
            << ", second_value: " << second_value << ",slope: " << slope
            << ", time_diff_to_timestep: " << time_diff_to_timestep_sec << " -> "
            << interpolated_value << std::endl;
#endif
  return interpolated_value;
}

double interpolate_value(bool normal,
                         double first_value,
                         double second_value,
                         double time_diff_sec,
                         double time_diff_to_timestep_sec)
{
  if (normal)
    return interpolate_normal(first_value, second_value, time_diff_sec, time_diff_to_timestep_sec);

  return interpolate_mod_360(first_value, second_value, time_diff_sec, time_diff_to_timestep_sec);
}

time_period extract_subvector_timeperiod(const DataVector& data,
                                         const boost::posix_time::ptime& startTime,
                                         const boost::posix_time::ptime& endTime)
{
  auto firstTimestamp =
      ((startTime == not_a_date_time || startTime < data[0].time) ? data[0].time : startTime);
  auto lastTimestamp = ((endTime == not_a_date_time || endTime > data[data.size() - 1].time)
                            ? data[data.size() - 1].time
                            : endTime);

  return {firstTimestamp, lastTimestamp + microseconds(1)};
}

void extract_subvector_weighted_segment(DataVector& subvector,
                                        const time_period& query_period,
                                        const DataItem& item1,
                                        const DataItem& item2)
{
  // iterate through the data vector and sort out periods and corresponding weights

  // period between two timesteps
  time_period timestep_period(item1.time, item2.time + microseconds(1));
  // if timestep period is inside the queried period handle it

  if (query_period.intersects(timestep_period))
  {
    // first find out intersecting period
    time_period intersection_period(query_period.intersection(timestep_period));
    if (intersection_period.length().total_seconds() == 0)
      return;

    // value changes halfway of timestep, so we have to handle first half and second half of
    // separately
    boost::posix_time::ptime halfway_time(timestep_period.begin() +
                                          seconds(timestep_period.length().total_seconds() / 2));
    if (intersection_period.contains(halfway_time))
    {
      time_period first_part_period(intersection_period.begin(), halfway_time + microseconds(1));
      time_period second_part_period(halfway_time, intersection_period.end());
      subvector.push_back(
          DataItem(item1.time, item1.value, first_part_period.length().total_seconds()));
      subvector.push_back(
          DataItem(item2.time, item2.value, second_part_period.length().total_seconds()));
    }
    else
    {
      if (intersection_period.begin() > halfway_time)  // intersection_period is in the second half
        subvector.push_back(
            DataItem(item2.time, item2.value, intersection_period.length().total_seconds()));
      else  // intersection_period must be in the first half
        subvector.push_back(
            DataItem(item1.time, item1.value, intersection_period.length().total_seconds()));
    }
  }
}

bool extract_subvector(const DataVector& data,
                       DataVector& subvector,
                       const boost::posix_time::ptime& startTime,
                       const boost::posix_time::ptime& endTime,
                       double itsMissingValue,
                       bool itsWeights)
{
  try
  {
    // if startTime is later than endTime return empty vector
    if ((startTime != not_a_date_time && endTime != not_a_date_time) && startTime > endTime)
      return false;

    auto query_period = extract_subvector_timeperiod(data, startTime, endTime);

    // if there is only one element in the vector
    if (data.size() == 1)
    {
      if (query_period.contains(data[0].time))
      {
        if (data[0].value == itsMissingValue)
          return false;
        subvector.push_back(DataItem(data[0].time, data[0].value, 1.0));
      }
      return true;
    }

#ifdef MYDEBUG
    std::cout << "query_period: " << query_period << std::endl;
#endif

    for (auto iter = data.begin(); iter != data.end(); iter++)
    {
      if (iter->value == itsMissingValue)
      {
        subvector.clear();
        return false;
      }

      if (!itsWeights)
      {
        if (query_period.contains(iter->time))
          subvector.push_back(*iter);
      }
      else
      {
        if ((iter + 1) != data.end())
        {
          const auto& item1 = *iter;
          const auto& item2 = *(iter + 1);
          extract_subvector_weighted_segment(subvector, query_period, item1, item2);
        }
      }
    }

    return !subvector.empty();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace

Stat::Stat(double theMissingValue /*= numeric_limits<double>::quiet_NaN()*/)
    : itsMissingValue(theMissingValue), itsWeights(true)
{
}

Stat::Stat(const DataVector& theValues,
           double theMissingValue /*= numeric_limits<double>::quiet_NaN()*/)
    : itsData(theValues), itsMissingValue(theMissingValue), itsWeights(true)
{
  try
  {
    calculate_weights();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Stat::Stat(const std::vector<double>& theValues,
           double theMissingValue /*= std::numeric_limits<double>::quiet_NaN()*/)
    : itsMissingValue(theMissingValue)
{
  try
  {
    addData(theValues);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Stat::Stat(const LocalTimeValueVector& theValues,
           double theMissingValue /*= std::numeric_limits<double>::quiet_NaN()*/)
    : itsMissingValue(theMissingValue), itsWeights(true)

{
  try
  {
    for (const LocalTimeValue& item : theValues)
    {
      itsData.push_back(DataItem(item.first.utc_time(), item.second));
    }
    calculate_weights();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Stat::Stat(const TimeValueVector& theValues,
           double theMissingValue /*= std::numeric_limits<double>::quiet_NaN(*/)
    : itsMissingValue(theMissingValue), itsWeights(true)
{
  try
  {
    for (const TimeValue& item : theValues)
    {
      itsData.push_back(DataItem(item.first, item.second));
    }
    calculate_weights();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Stat::setData(const DataVector& theValues)
{
  try
  {
    itsData = theValues;
    calculate_weights();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Stat::addData(double theValue)
{
  try
  {
    itsData.push_back(DataItem(not_a_date_time, theValue));
    calculate_weights();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Stat::addData(const boost::local_time::local_date_time& theTime, double theValue)
{
  try
  {
    itsData.push_back(DataItem(theTime.utc_time(), theValue));
    calculate_weights();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Stat::addData(const boost::posix_time::ptime& theTime, double theValue)
{
  try
  {
    itsData.push_back(DataItem(theTime, theValue));
    calculate_weights();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Stat::addData(const vector<double>& theValues)
{
  try
  {
    for (double value : theValues)
      itsData.push_back(DataItem(not_a_date_time, value));

    calculate_weights();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Stat::addData(const DataItem& theData)
{
  try
  {
    itsData.push_back(theData);
    calculate_weights();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Stat::clear()
{
  try
  {
    itsData.clear();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
// time in seconds is used as a weight
double Stat::integ(const boost::posix_time::ptime& startTime /*= not_a_date_time */,
                   const boost::posix_time::ptime& endTime /*= not_a_date_time */) const
{
  try
  {
#ifdef MYDEBUG
    std::cout << "integ(" << startTime << ", " << endTime << ")" << std::endl;
#endif
    DataVector subvector;

    if (!get_subvector(subvector, startTime, endTime))
      return itsMissingValue;

    accumulator_set<double, stats<tag::sum>, double> acc;

    for (const DataItem& item : subvector)
    {
      acc(item.value, weight = item.weight);
    }

    if (itsDegrees)
      return fmod(boost::accumulators::sum(acc), MODULO_VALUE_360) / 3600.0;

    return boost::accumulators::sum(acc) / 3600.0;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// weight is alwaus 1.0
double Stat::sum(const boost::posix_time::ptime& startTime /*= not_a_date_time */,
                 const boost::posix_time::ptime& endTime /*= not_a_date_time */) const
{
  try
  {
#ifdef MYDEBUG
    std::cout << "sum(" << startTime << ", " << endTime << ")" << std::endl;
#endif
    DataVector subvector;

    if (!get_subvector(subvector, startTime, endTime))
      return itsMissingValue;

    accumulator_set<double, stats<tag::sum>, double> acc;

    for (const DataItem& item : subvector)
    {
      acc(item.value, weight = 1.0);
    }

    if (itsDegrees)
      return fmod(boost::accumulators::sum(acc), MODULO_VALUE_360);

    return boost::accumulators::sum(acc);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

double Stat::min(const boost::posix_time::ptime& startTime /*= not_a_date_time */,
                 const boost::posix_time::ptime& endTime /*= not_a_date_time */) const
{
  try
  {
#ifdef MYDEBUG
    std::cout << "min(" << startTime << ", " << endTime << ")" << std::endl;
#endif
    DataVector subvector;

    if (!get_subvector(subvector, startTime, endTime) || subvector.empty())
      return itsMissingValue;

    return std::min_element(subvector.begin(), subvector.end(), comp_value)->value;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

double Stat::mean(const boost::posix_time::ptime& startTime /*= not_a_date_time */,
                  const boost::posix_time::ptime& endTime /*= not_a_date_time */) const
{
  try
  {
#ifdef MYDEBUG
    std::cout << "mean(" << startTime << ", " << endTime << ")" << std::endl;
#endif
    DataVector subvector;

    if (!get_subvector(subvector, startTime, endTime))
      return itsMissingValue;

    if (itsDegrees)
    {
      double previousDirection = 0;
      double directionSum = 0;
      for (unsigned int i = 0; i < subvector.size(); i++)
      {
        if (i == 0)
        {
          directionSum = subvector[i].value;
          previousDirection = subvector[i].value;
        }
        else
        {
          double diff = subvector[i].value - previousDirection;
          double direction = previousDirection + diff;

          if (diff < -MODULO_VALUE_360 / 2.0)
            direction += MODULO_VALUE_360;
          else if (diff > MODULO_VALUE_360 / 2.0)
            direction -= MODULO_VALUE_360;

          directionSum += direction;
          previousDirection = direction;
        }
      }

      double mean = directionSum / subvector.size();
      mean -= (MODULO_VALUE_360 * floor(mean / MODULO_VALUE_360));
      return mean;
    }

    accumulator_set<double, stats<tag::mean>, double> acc;
    for (const DataItem& item : subvector)
    {
      acc(item.value, weight = (itsWeights ? item.weight : 1.0));
    }
    return boost::accumulators::mean(acc);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

double Stat::max(const boost::posix_time::ptime& startTime /*= not_a_date_time */,
                 const boost::posix_time::ptime& endTime /*= not_a_date_time */) const
{
  try
  {
#ifdef MYDEBUG
    std::cout << "max(" << startTime << ", " << endTime << ")" << std::endl;
#endif
    DataVector subvector;

    if (!get_subvector(subvector, startTime, endTime))
      return itsMissingValue;

    return std::max_element(subvector.begin(), subvector.end(), comp_value)->value;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

double Stat::change(const boost::posix_time::ptime& startTime /*= not_a_date_time */,
                    const boost::posix_time::ptime& endTime /*= not_a_date_time */) const
{
  try
  {
#ifdef MYDEBUG
    std::cout << "change(" << startTime << ", " << endTime << ")" << std::endl;
#endif
    DataVector subvector;

    if (!get_subvector(subvector, startTime, endTime) || subvector.empty())
      return itsMissingValue;

    if (itsDegrees)
    {
      double previousValue = 0;
      double cumulativeChange = 0;
      for (unsigned int i = 0; i < subvector.size(); i++)
      {
        if (i > 0)
        {
          double diff = subvector[i].value - previousValue;
          if (diff < -MODULO_VALUE_360 / 2.0)
            cumulativeChange += diff + MODULO_VALUE_360;
          else if (diff > MODULO_VALUE_360 / 2.0)
            cumulativeChange += diff - MODULO_VALUE_360;
          else
            cumulativeChange += diff;
        }
        previousValue = subvector[i].value;
      }
      return cumulativeChange;
    }

    double firstValue(subvector.begin()->value);
    double lastValue((subvector.end() - 1)->value);

    return lastValue - firstValue;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

double Stat::trend(const boost::posix_time::ptime& startTime /*= not_a_date_time */,
                   const boost::posix_time::ptime& endTime /*= not_a_date_time */) const
{
  try
  {
#ifdef MYDEBUG
    std::cout << "trend(" << startTime << ", " << endTime << ")" << std::endl;
#endif
    DataVector subvector;

    if (!get_subvector(subvector, startTime, endTime) || subvector.size() <= 1)
      return itsMissingValue;

    long positiveChanges(0);
    long negativeChanges(0);
    double lastValue = 0;

    for (auto iter = subvector.begin(); iter != subvector.end(); iter++)
    {
      if (iter != subvector.begin())
      {
        if (itsDegrees)
        {
          const double diff = iter->value - lastValue;
          if (diff < -MODULO_VALUE_360 / 2.0)
            ++positiveChanges;
          else if (diff > MODULO_VALUE_360 / 2.0)
            ++negativeChanges;
          else if (diff < 0)
            ++negativeChanges;
          else if (diff > 0)
            ++positiveChanges;
        }
        else
        {
          if (iter->value > lastValue)
            ++positiveChanges;
          else if (iter->value < lastValue)
            ++negativeChanges;
        }
      }
      lastValue = iter->value;
    }

    return static_cast<double>(positiveChanges - negativeChanges) /
           static_cast<double>(subvector.size() - 1) * 100.0;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

unsigned int Stat::count(double lowerLimit,
                         double upperLimit,
                         const boost::posix_time::ptime& startTime /*= not_a_date_time */,
                         const boost::posix_time::ptime& endTime /*= not_a_date_time */) const
{
  try
  {
#ifdef MYDEBUG
    std::cout << "count(" << lowerLimit << ", " << upperLimit << ", " << startTime << ", "
              << endTime << ")" << std::endl;
#endif
    DataVector subvector;

    if (!get_subvector(subvector, startTime, endTime, false))
      return static_cast<unsigned int>(itsMissingValue);

    unsigned int occurances(0);

    for (const DataItem& item : subvector)
      if (item.value >= lowerLimit && item.value <= upperLimit)
        occurances++;

    return occurances;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

double Stat::percentage(double lowerLimit,
                        double upperLimit,
                        const boost::posix_time::ptime& startTime /*= not_a_date_time */,
                        const boost::posix_time::ptime& endTime /*= not_a_date_time */) const
{
  try
  {
#ifdef MYDEBUG
    std::cout << "percentage(" << lowerLimit << ", " << upperLimit << ", " << startTime << ", "
              << endTime << ")" << std::endl;
#endif
    DataVector subvector;

    if (!get_subvector(subvector, startTime, endTime))
      return itsMissingValue;

    int occurances(0);
    int total_count(0);

    for (const DataItem& item : subvector)
    {
      if (item.value >= lowerLimit && item.value <= upperLimit)
        occurances += (itsWeights ? item.weight : 1);
      total_count += (itsWeights ? item.weight : 1);
    }

    if (occurances == 0)
      return 0.0;

    return static_cast<double>(
        (static_cast<double>(occurances) / static_cast<double>(total_count)) * 100.0);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

double Stat::median(const boost::posix_time::ptime& startTime /*= not_a_date_time */,
                    const boost::posix_time::ptime& endTime /*= not_a_date_time */) const
{
  try
  {
#ifdef MYDEBUG
    std::cout << "median(" << startTime << ", " << endTime << ")" << std::endl;
#endif
    DataVector subvector;

    if (!get_subvector(subvector, startTime, endTime) || subvector.empty())
      return itsMissingValue;

    if (subvector.size() == 1)
      return subvector[0].value;

    vector<double> double_vector;

    for (const DataItem& item : subvector)
    {
      double_vector.insert(double_vector.end(),
                           static_cast<std::size_t>(itsWeights ? item.weight : 1.0),
                           item.value);
    }

    std::size_t vector_size = double_vector.size();

    std::size_t middle_index = (vector_size / 2) + 1;

    auto middle_pos = double_vector.begin();
    std::advance(middle_pos, middle_index);
    partial_sort(double_vector.begin(), middle_pos, double_vector.end());

    if (double_vector.size() % 2 == 0)
    {
      return ((double_vector[static_cast<std::size_t>((vector_size / 2) - 1)] +
               double_vector[(vector_size / 2)]) /
              2.0);
    }

    return double_vector[static_cast<std::size_t>(((vector_size + 1) / 2) - 1)];
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

double Stat::variance(const boost::posix_time::ptime& startTime /*= not_a_date_time */,
                      const boost::posix_time::ptime& endTime /*= not_a_date_time */) const
{
  try
  {
#ifdef MYDEBUG
    std::cout << "variance(" << startTime << ", " << endTime << ")" << std::endl;
#endif
    DataVector subvector;

    if (!get_subvector(subvector, startTime, endTime) || subvector.empty())
      return itsMissingValue;

    accumulator_set<double, stats<tag::variance>, double> acc;

    for (const DataItem& item : subvector)
    {
      acc(item.value, weight = (itsWeights ? item.weight : 1.0));
    }

    return boost::accumulators::variance(acc);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

double Stat::stddev(const boost::posix_time::ptime& startTime /*= not_a_date_time */,
                    const boost::posix_time::ptime& endTime /*= not_a_date_time */) const
{
  try
  {
#ifdef MYDEBUG
    std::cout << "stddev(" << startTime << ", " << endTime << ")" << std::endl;
#endif
    if (itsDegrees)
      return stddev_dir(startTime, endTime);

    return sqrt(variance(startTime, endTime));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

double Stat::stddev_dir(const boost::posix_time::ptime& startTime /*= not_a_date_time */,
                        const boost::posix_time::ptime& endTime /*= not_a_date_time */) const
{
  try
  {
#ifdef MYDEBUG
    std::cout << "stddev_dir(" << startTime << ", " << endTime << ")" << std::endl;
#endif

    DataVector subvector;

    if (!get_subvector(subvector, startTime, endTime) || subvector.empty())
      return itsMissingValue;

    double sum = 0;
    double squaredSum = 0;
    double previousDirection = 0;
    for (unsigned int i = 0; i < subvector.size(); i++)
    {
      if (i == 0)
      {
        sum = subvector[i].value;
        squaredSum = subvector[i].value * subvector[i].value;
        previousDirection = subvector[i].value;
      }
      else
      {
        double diff = subvector[i].value - previousDirection;
        double dir = previousDirection + diff;
        if (diff < -MODULO_VALUE_360 / 2.0)
        {
          while (dir < MODULO_VALUE_360 / 2.0)
            dir += MODULO_VALUE_360;
        }
        else if (diff > MODULO_VALUE_360 / 2.0)
        {
          while (dir > MODULO_VALUE_360 / 2.0)
            dir -= MODULO_VALUE_360;
        }
        sum += dir;
        squaredSum += dir * dir;
        previousDirection = dir;
      }
    }
    double tmp = squaredSum - sum * sum / subvector.size();
    if (tmp < 0)
      return 0.0;

    return sqrt(tmp / subvector.size());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

double Stat::nearest(const boost::posix_time::ptime& timestep,
                     const boost::posix_time::ptime& startTime /*= not_a_date_time */,
                     const boost::posix_time::ptime& endTime /*= not_a_date_time */) const
{
  try
  {
#ifdef MYDEBUG
    std::cout << "nearest(" << timestep << ", " << startTime << ", " << endTime << ")" << std::endl;
#endif

    if (timestep == not_a_date_time)
      return itsMissingValue;

    DataVector subvector;

    if (!get_subvector(subvector, startTime, endTime, false) || subvector.empty())
      return itsMissingValue;

    if (subvector.size() == 1)
      return subvector[0].value;

    double nearest_value = itsMissingValue;
    long nearest_seconds = -1;

    for (unsigned int i = 0; i < subvector.size(); i++)
    {
      if (i == 0 || abs((subvector[i].time - timestep).total_seconds()) < nearest_seconds)
      {
        nearest_value = subvector[i].value;
        nearest_seconds = abs((subvector[i].time - timestep).total_seconds());
      }
    }

    return nearest_value;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// Do linear inter-/extrapolation
double Stat::interpolate(const boost::posix_time::ptime& timestep,
                         const boost::posix_time::ptime& startTime /*= not_a_date_time */,
                         const boost::posix_time::ptime& endTime /*= not_a_date_time */) const
{
  try
  {
#ifdef MYDEBUG
    std::cout << "interpolate(" << timestep << ", " << startTime << ", " << endTime << ")"
              << std::endl;
#endif

    if (timestep == not_a_date_time)
      return itsMissingValue;

    DataVector subvector;

    if (!get_subvector(subvector, startTime, endTime, false))
      return itsMissingValue;

    // If there is only one timestep in the data and it is the same as requested we return the value
    if (subvector.size() == 1 && subvector.at(0).time == timestep)
      return subvector.at(0).value;

    // There must be at least two timesteps with valid values in order to interpolate
    if (subvector.size() == 2 &&
        (subvector.at(0).value == itsMissingValue || subvector.at(1).value == itsMissingValue))
      return itsMissingValue;

    std::vector<int> indicator_vector;  // -1 indicates earliter, + 1 later than requested timestep

    for (const auto& val : subvector)
    {
      if (val.value == itsMissingValue)
        continue;
      if (val.time < timestep)
        indicator_vector.push_back(-1);
      else if (val.time > timestep)
        indicator_vector.push_back(1);
      else if (val.time == timestep)
        return val.value;  // Exact timestep found
    }

    if (indicator_vector.size() < 2)
      return itsMissingValue;

    boost::posix_time::ptime first_time = boost::posix_time::not_a_date_time;
    boost::posix_time::ptime second_time = boost::posix_time::not_a_date_time;
    double first_value = itsMissingValue;
    double second_value = itsMissingValue;
    double time_diff_to_timestep_sec = 0.0;
    if (indicator_vector.back() == -1)
    {
      // All timesteps are earlier -> extrapolate to the future
      int last_index = indicator_vector.size() - 1;
      first_time = subvector.at(last_index - 1).time;
      second_time = subvector.at(last_index).time;
      first_value = subvector.at(last_index - 1).value;
      second_value = subvector.at(last_index).value;
      time_diff_to_timestep_sec = (timestep - first_time).total_seconds();
    }
    else if (indicator_vector.front() == 1)
    {
      // All timesteps are later -> extrapolate to the past
      first_time = subvector.at(0).time;
      second_time = subvector.at(1).time;
      first_value = subvector.at(0).value;
      second_value = subvector.at(1).value;
      time_diff_to_timestep_sec = (timestep - first_time).total_seconds();
    }
    else
    {
      // Interpolate between timesteps
      for (unsigned int i = 1; i < indicator_vector.size(); i++)
      {
        if (indicator_vector.at(i - 1) == -1 && indicator_vector.at(i) == 1)
        {
          first_time = subvector.at(i - 1).time;
          second_time = subvector.at(i).time;
          first_value = subvector.at(i - 1).value;
          second_value = subvector.at(i).value;
          time_diff_to_timestep_sec = (timestep - first_time).total_seconds();
          break;
        }
      }
    }

    double time_diff_sec = (second_time - first_time).total_seconds();
    bool normal = !itsDegrees;
    return interpolate_value(
        normal, first_value, second_value, time_diff_sec, time_diff_to_timestep_sec);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool Stat::get_subvector(DataVector& subvector,
                         const boost::posix_time::ptime& startTime /*= not_a_date_time*/,
                         const boost::posix_time::ptime& endTime /*= not_a_date_time*/,
                         bool useWeights /*= true*/) const
{
  try
  {
    if (invalid_timestamps())
    {
      if (startTime != not_a_date_time || endTime != not_a_date_time)
      {
        throw Fmi::Exception(
            BCP, "Error: startTime or endTime defined, but data contains invalid timestamps!");
      }

      for (const auto& val : itsData)
      {
        if (val.value == itsMissingValue)
          return false;
        subvector.push_back(val);
      }
      return true;
    }
    bool applyWeights = (useWeights ? itsWeights : false);
    return extract_subvector(itsData, subvector, startTime, endTime, itsMissingValue, applyWeights);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Stat::calculate_weights()
{
  try
  {
    if (itsData.size() == 1)
    {
      itsData[0].weight = 1.0;
      return;
    }

    bool invalidTimestampsFound = invalid_timestamps();

    if (!invalidTimestampsFound)
      sort(itsData.begin(), itsData.end(), comp_time);

    for (unsigned int i = 0; i < itsData.size(); i++)
    {
      if (invalidTimestampsFound)
      {
        itsData[i].weight = 1.0;
      }
      else
      {
        if (i == 0)
        {
          time_duration dur(itsData[i + 1].time - itsData[i].time);
          itsData[i].weight = dur.total_seconds() * 0.5;
        }
        else if (i == itsData.size() - 1)
        {
          time_duration dur(itsData[i].time - itsData[i - 1].time);
          itsData[i].weight = dur.total_seconds() * 0.5;
        }
        else
        {
          time_duration dur_prev(itsData[i].time - itsData[i - 1].time);
          time_duration dur_next(itsData[i + 1].time - itsData[i].time);
          itsData[i].weight = (dur_prev.total_seconds() * 0.5) + (dur_next.total_seconds() * 0.5);
        }
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool Stat::invalid_timestamps() const
{
  try
  {
    for (const DataItem& item : itsData)
    {
      if (item.time == not_a_date_time)
      {
        return true;
      }
    }

    return false;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::ostream& operator<<(std::ostream& os, const DataItem& item)
{
  try
  {
    os << "timestamp = " << item.time << std::endl
       << "value = " << item.value << std::endl
       << "weight = " << item.weight << std::endl;

    return os;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Stat
}  // namespace TimeSeries
}  // namespace SmartMet
