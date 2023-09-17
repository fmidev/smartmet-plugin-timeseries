#include "TimeSeriesUtility.h"
#include "TimeSeriesOutput.h"
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace SmartMet
{
namespace TimeSeries
{
// ----------------------------------------------------------------------
/*!
 * \brief Output operator for debugging purposes
 */
// ----------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, const TimeSeriesData& tsdata)
{
  try
  {
    if (boost::get<TimeSeriesPtr>(&tsdata) != nullptr)
      os << **(boost::get<TimeSeriesPtr>(&tsdata));
    else if (boost::get<TimeSeriesVectorPtr>(&tsdata) != nullptr)
      os << **(boost::get<TimeSeriesVectorPtr>(&tsdata));
    else if (boost::get<TimeSeriesGroupPtr>(&tsdata) != nullptr)
      os << **(boost::get<TimeSeriesGroupPtr>(&tsdata));

    return os;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::ostream& operator<<(std::ostream& os, const OutputData& odata)
{
  try
  {
    for (const auto& item : odata)
    {
      os << item.first << " -> " << std::endl;
      unsigned int counter = 0;
      for (const auto& item2 : item.second)
        os << "#" << counter++ << "\n" << item2 << std::endl;
    }

    return os;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

namespace
{

// ----------------------------------------------------------------------
/*!
 * \brief Remove redundant timesteps used for aggregation only from the time series
 */
// ----------------------------------------------------------------------

void erase_redundant_timesteps(TimeSeries& ts, const TimeSeriesGenerator::LocalTimeList& timesteps)
{
  try
  {
    // Fast special case exit
    if (ts.empty())
      return;

    // First check if all the timesteps are relevant. We assume both TimeSeries and LocalTimeList
    // are in sorted order.

    const auto n = ts.size();
    std::size_t valid_count = 0;
    std::vector<bool> keep_timestep(n, true);

    auto next_valid_time = timesteps.cbegin();
    const auto& last_valid_time = timesteps.cend();
    for (std::size_t i = 0; i < n; i++)
    {
      const auto& data = ts[i];

      // Skip valid times until data_time is greater than or equal to it
      while (next_valid_time != last_valid_time && data.time > *next_valid_time)
        ++next_valid_time;

      // Now the time is either valid (==) or not needed
      if (next_valid_time == last_valid_time || *next_valid_time != data.time)
        keep_timestep[i] = false;
      else
      {
        ++valid_count;
        ++next_valid_time;
      }
    }

    // Quick exit if nothing needs to be erased
    if (valid_count == n)
      return;

    // Create reduced timeseries and swap it with the input
    TimeSeries output(ts.getLocalTimePool());
    output.reserve(valid_count);

    for (std::size_t i = 0; i < n; i++)
    {
      if (keep_timestep[i])
        output.push_back(ts[i]);
    }

    std::swap(ts, output);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Erase timesteps used for aggregation only
 */
// ----------------------------------------------------------------------

TimeSeriesPtr erase_redundant_timesteps(TimeSeriesPtr ts,
                                        const TimeSeriesGenerator::LocalTimeList& timesteps)
{
  try
  {
    erase_redundant_timesteps(*ts, timesteps);
    return ts;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Erase timesteps used for aggregation only
 */
// ----------------------------------------------------------------------

TimeSeriesVectorPtr erase_redundant_timesteps(TimeSeriesVectorPtr tsv,
                                              const TimeSeriesGenerator::LocalTimeList& timesteps)
{
  try
  {
    for (auto& tv : *tsv)
      erase_redundant_timesteps(tv, timesteps);

    return tsv;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Erase timesteps used for aggregation only
 */
// ----------------------------------------------------------------------

TimeSeriesGroupPtr erase_redundant_timesteps(TimeSeriesGroupPtr tsg,
                                             const TimeSeriesGenerator::LocalTimeList& timesteps)
{
  try
  {
    for (auto& ts : *tsg)
      erase_redundant_timesteps(ts.timeseries, timesteps);
    return tsg;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

size_t number_of_elements(const OutputData& outputData)
{
  try
  {
    size_t ret = 0;
    for (const auto& output : outputData)
    {
      const std::vector<TimeSeriesData>& outdata = output.second;

      // iterate columns (parameters)
      for (unsigned int j = 0; j < outdata.size(); j++)
      {
        const TimeSeriesData& tsdata = outdata[j];

        if (boost::get<TimeSeriesPtr>(&tsdata))
        {
          TimeSeriesPtr ts = *(boost::get<TimeSeriesPtr>(&tsdata));
          if (ts && !ts->empty())
            ret += ts->size();
        }
        else if (boost::get<TimeSeriesVectorPtr>(&tsdata))
        {
          TimeSeriesVectorPtr tsv = *(boost::get<TimeSeriesVectorPtr>(&tsdata));
          if (tsv)
            for (unsigned int k = 0; k < tsv->size(); k++)
              ret += tsv->at(k).size();
        }
        else if (boost::get<TimeSeriesGroupPtr>(&tsdata))
        {
          TimeSeriesGroupPtr tsg = *(boost::get<TimeSeriesGroupPtr>(&tsdata));
          if (tsg)
            for (unsigned int k = 0; k < tsg->size(); k++)
              ret += tsg->at(k).timeseries.size();
        }
      }
    }
    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace TimeSeries
}  // namespace SmartMet
