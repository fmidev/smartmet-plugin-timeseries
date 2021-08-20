#include "DataFunctions.h"
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
namespace DataFunctions
{
// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, const TimeSeriesData& tsdata)
{
  try
  {
    if (boost::get<Spine::TimeSeries::TimeSeriesPtr>(&tsdata) != nullptr)
      os << **(boost::get<Spine::TimeSeries::TimeSeriesPtr>(&tsdata));
    else if (boost::get<Spine::TimeSeries::TimeSeriesVectorPtr>(&tsdata) != nullptr)
      os << **(boost::get<Spine::TimeSeries::TimeSeriesVectorPtr>(&tsdata));
    else if (boost::get<Spine::TimeSeries::TimeSeriesGroupPtr>(&tsdata) != nullptr)
      os << **(boost::get<Spine::TimeSeries::TimeSeriesGroupPtr>(&tsdata));

    return os;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void erase_redundant_timesteps(Spine::TimeSeries::TimeSeries& ts,
                               const Spine::TimeSeriesGenerator::LocalTimeList& timesteps)
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
    Spine::TimeSeries::TimeSeries output(ts.getLocalTimePool());
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

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

Spine::TimeSeries::TimeSeriesPtr erase_redundant_timesteps(
    Spine::TimeSeries::TimeSeriesPtr ts, const Spine::TimeSeriesGenerator::LocalTimeList& timesteps)
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
 * \brief
 */
// ----------------------------------------------------------------------

Spine::TimeSeries::TimeSeriesVectorPtr erase_redundant_timesteps(
    Spine::TimeSeries::TimeSeriesVectorPtr tsv,
    const Spine::TimeSeriesGenerator::LocalTimeList& timesteps)
{
  try
  {
    for (unsigned int i = 0; i < tsv->size(); i++)
      erase_redundant_timesteps(tsv->at(i), timesteps);

    return tsv;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

Spine::TimeSeries::TimeSeriesGroupPtr erase_redundant_timesteps(
    Spine::TimeSeries::TimeSeriesGroupPtr tsg,
    const Spine::TimeSeriesGenerator::LocalTimeList& timesteps)
{
  try
  {
    for (size_t i = 0; i < tsg->size(); i++)
      erase_redundant_timesteps(tsg->at(i).timeseries, timesteps);
    return tsg;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void update_latest_timestep(Query& query, const Spine::TimeSeries::TimeSeriesVectorPtr& tsv)
{
  try
  {
    if (tsv->empty())
      return;

    // Sometimes some of the timeseries are empty. However, should we iterate
    // backwards until we find a valid one instead of exiting??

    if (tsv->back().empty())
      return;

    const auto& last_time = tsv->back().back().time;

    if (query.toptions.startTimeUTC)
      query.latestTimestep = last_time.utc_time();
    else
      query.latestTimestep = last_time.local_time();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void update_latest_timestep(Query& query, const Spine::TimeSeries::TimeSeries& ts)
{
  try
  {
    if (ts.empty())
      return;

    const auto& last_time = ts[ts.size() - 1].time;

    if (query.toptions.startTimeUTC)
      query.latestTimestep = last_time.utc_time();
    else
      query.latestTimestep = last_time.local_time();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void update_latest_timestep(Query& query, const Spine::TimeSeries::TimeSeriesGroup& tsg)
{
  try
  {
    // take first time series and last timestep thereof
    Spine::TimeSeries::TimeSeries ts = tsg[0].timeseries;

    if (ts.empty())
      return;

    // update the latest timestep, so that next query (is exists) knows from where to continue
    update_latest_timestep(query, ts);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void store_data(Spine::TimeSeries::TimeSeriesVectorPtr aggregatedData,
                Query& query,
                OutputData& outputData)
{
  try
  {
    if (aggregatedData->empty())
      return;

    // insert data to the end
    std::vector<TimeSeriesData>& odata = (--outputData.end())->second;
    odata.emplace_back(TimeSeriesData(aggregatedData));
    update_latest_timestep(query, aggregatedData);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void store_data(std::vector<TimeSeriesData>& aggregatedData, Query& query, OutputData& outputData)
{
  try
  {
    if (aggregatedData.empty())
      return;

    TimeSeriesData tsdata;
    if (boost::get<Spine::TimeSeries::TimeSeriesPtr>(&aggregatedData[0]))
    {
      Spine::TimeSeries::TimeSeriesPtr ts_first =
          *(boost::get<Spine::TimeSeries::TimeSeriesPtr>(&aggregatedData[0]));
      Spine::TimeSeries::TimeSeriesPtr ts_result(
          new Spine::TimeSeries::TimeSeries(ts_first->getLocalTimePool()));
      // first merge timeseries of all levels of one parameter
      for (unsigned int i = 0; i < aggregatedData.size(); i++)
      {
        Spine::TimeSeries::TimeSeriesPtr ts =
            *(boost::get<Spine::TimeSeries::TimeSeriesPtr>(&aggregatedData[i]));
        ts_result->insert(ts_result->end(), ts->begin(), ts->end());
      }
      // update the latest timestep, so that next query (if exists) knows from where to continue
      update_latest_timestep(query, *ts_result);
      tsdata = ts_result;
    }
    else if (boost::get<Spine::TimeSeries::TimeSeriesGroupPtr>(&aggregatedData[0]))
    {
      Spine::TimeSeries::TimeSeriesGroupPtr tsg_result(new Spine::TimeSeries::TimeSeriesGroup);
      // first merge timeseries of all levels of one parameter
      for (unsigned int i = 0; i < aggregatedData.size(); i++)
      {
        Spine::TimeSeries::TimeSeriesGroupPtr tsg =
            *(boost::get<Spine::TimeSeries::TimeSeriesGroupPtr>(&aggregatedData[i]));
        tsg_result->insert(tsg_result->end(), tsg->begin(), tsg->end());
      }
      // update the latest timestep, so that next query (if exists) knows from where to continue
      update_latest_timestep(query, *tsg_result);
      tsdata = tsg_result;
    }

    // insert data to the end
    std::vector<TimeSeriesData>& odata = (--outputData.end())->second;
    odata.push_back(tsdata);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace DataFunctions
}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
