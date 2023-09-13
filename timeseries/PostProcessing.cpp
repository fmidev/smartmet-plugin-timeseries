#include "PostProcessing.h"
#include "LocationTools.h"
#include "UtilityFunctions.h"
#include <timeseries/ParameterKeywords.h>
#include <timeseries/TableFeeder.h>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
namespace PostProcessing
{
// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void update_latest_timestep(Query& query, const TS::TimeSeriesVectorPtr& tsv)
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

void update_latest_timestep(Query& query, const TS::TimeSeries& ts)
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

void update_latest_timestep(Query& query, const TS::TimeSeriesGroup& tsg)
{
  try
  {
    // take first time series and last timestep thereof
    TS::TimeSeries ts = tsg[0].timeseries;

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

void store_data(TS::TimeSeriesVectorPtr aggregatedData, Query& query, TS::OutputData& outputData)
{
  try
  {
    if (aggregatedData->empty())
      return;

    // insert data to the end
    std::vector<TS::TimeSeriesData>& odata = (--outputData.end())->second;
    odata.emplace_back(TS::TimeSeriesData(aggregatedData));
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

void store_data(std::vector<TS::TimeSeriesData>& aggregatedData,
                Query& query,
                TS::OutputData& outputData)
{
  try
  {
    if (aggregatedData.empty())
      return;

    TS::TimeSeriesData tsdata;
    if (boost::get<TS::TimeSeriesPtr>(aggregatedData.data()))
    {
      TS::TimeSeriesPtr ts_first = *(boost::get<TS::TimeSeriesPtr>(aggregatedData.data()));
      TS::TimeSeriesPtr ts_result(new TS::TimeSeries(ts_first->getLocalTimePool()));
      // first merge timeseries of all levels of one parameter
      for (const auto& data : aggregatedData)
      {
        TS::TimeSeriesPtr ts = *(boost::get<TS::TimeSeriesPtr>(&data));
        ts_result->insert(ts_result->end(), ts->begin(), ts->end());
      }
      // update the latest timestep, so that next query (if exists) knows from where to continue
      update_latest_timestep(query, *ts_result);
      tsdata = ts_result;
    }
    else if (boost::get<TS::TimeSeriesGroupPtr>(aggregatedData.data()))
    {
      TS::TimeSeriesGroupPtr tsg_result(new TS::TimeSeriesGroup);
      // first merge timeseries of all levels of one parameter
      for (const auto& data : aggregatedData)
      {
        TS::TimeSeriesGroupPtr tsg = *(boost::get<TS::TimeSeriesGroupPtr>(&data));
        tsg_result->insert(tsg_result->end(), tsg->begin(), tsg->end());
      }
      // update the latest timestep, so that next query (if exists) knows from where to continue
      update_latest_timestep(query, *tsg_result);
      tsdata = tsg_result;
    }

    // insert data to the end
    std::vector<TS::TimeSeriesData>& odata = (--outputData.end())->second;
    odata.push_back(tsdata);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void add_data_to_table(const TS::OptionParsers::ParameterList& paramlist,
                       TS::TableFeeder& tf,
                       const TS::OutputData& outputData,
                       const std::string& location_name,
                       int& startRow)
{
  try
  {
    unsigned int numberOfParameters = paramlist.size();
    // iterate different locations
    for (const auto& output : outputData)
    {
      const auto& locationName = output.first;
      if (locationName != location_name)
        continue;

      startRow = tf.getCurrentRow();

      const std::vector<TS::TimeSeriesData>& outdata = output.second;

      // iterate columns (parameters)
      for (unsigned int j = 0; j < outdata.size(); j++)
      {
        const TS::TimeSeriesData& tsdata = outdata[j];
        tf.setCurrentRow(startRow);
        tf.setCurrentColumn(j);

        const auto& paramName = paramlist[j % numberOfParameters].name();
        if (paramName == LATLON_PARAM || paramName == NEARLATLON_PARAM)
        {
          tf << TS::LonLatFormat::LATLON;
        }
        else if (paramName == LONLAT_PARAM || paramName == NEARLONLAT_PARAM)
        {
          tf << TS::LonLatFormat::LONLAT;
        }

        if (boost::get<TS::TimeSeriesPtr>(&tsdata))
        {
          TS::TimeSeriesPtr ts = *(boost::get<TS::TimeSeriesPtr>(&tsdata));
          tf << *ts;
        }
        else if (boost::get<TS::TimeSeriesVectorPtr>(&tsdata))
        {
          TS::TimeSeriesVectorPtr tsv = *(boost::get<TS::TimeSeriesVectorPtr>(&tsdata));
          for (unsigned int k = 0; k < tsv->size(); k++)
          {
            tf.setCurrentColumn(k);
            tf.setCurrentRow(startRow);
            tf << tsv->at(k);
          }
          startRow = tf.getCurrentRow();
        }
        else if (boost::get<TS::TimeSeriesGroupPtr>(&tsdata))
        {
          TS::TimeSeriesGroupPtr tsg = *(boost::get<TS::TimeSeriesGroupPtr>(&tsdata));
          tf << *tsg;
        }

        // Reset formatting to the default value
        tf << TS::LonLatFormat::LONLAT;
      }
    }
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

// fills the table with data
void fill_table(Query& query, TS::OutputData& outputData, Spine::Table& table)
{
  try
  {
    if (outputData.empty())
      return;

    TS::TableFeeder tf(table, query.valueformatter, query.precisions);
    int startRow = tf.getCurrentRow();

    std::string locationName(outputData[0].first);

    // if observations exists they are first in the result set
    if (locationName == "_obs_")
      add_data_to_table(query.poptions.parameters(), tf, outputData, "_obs_", startRow);

    // iterate locations
    for (const auto& tloc : query.loptions->locations())
    {
      std::string locationId = get_location_id(tloc.loc);

      add_data_to_table(query.poptions.parameters(), tf, outputData, locationId, startRow);
    }
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

#ifndef WITHOUT_OBSERVATION

void add_missing_timesteps(TS::TimeSeries& ts, const TS::TimeSeriesGeneratorCache::TimeList& tlist)
{
  if (!tlist || tlist->empty())
    return;

  TS::TimeSeries ts2(ts.getLocalTimePool());

  auto it = tlist->begin();

  for (const auto& value : ts)
  {
    // Add missing timesteps
    while (it != tlist->end() && *it < value.time)
    {
      ts2.emplace_back(TS::TimedValue(*it, TS::None()));
      ++it;
    }
    ts2.emplace_back(value);
    // If list has been iterated to the end and
    // iteration time is same as observed timestep, go to next step
    if (it != tlist->end() && *it == value.time)
      ++it;
  }
  // If there are requested timesteps after last value, add them
  while (it != tlist->end())
  {
    ts2.emplace_back(TS::TimedValue(*it, TS::None()));
    ++it;
  }
  ts = ts2;
}

TS::TimeSeriesByLocation get_timeseries_by_fmisid(
    const std::string& producer,
    const TS::TimeSeriesVectorPtr& observation_result,
    const TS::TimeSeriesGeneratorCache::TimeList& tlist,
    int fmisid_index)
{
  try
  {
    TS::TimeSeriesByLocation ret;

    if (UtilityFunctions::is_flash_or_mobile_producer(producer))
    {
      ret.emplace_back(make_pair(0, observation_result));
      return ret;
    }

    // find fmisid time series
    const TS::TimeSeries& fmisid_ts = observation_result->at(fmisid_index);

    // find indexes for locations
    std::vector<std::pair<unsigned int, unsigned int>> location_indexes;

    unsigned int start_index = 0;
    unsigned int end_index = 0;
    for (unsigned int i = 1; i < fmisid_ts.size(); i++)
    {
      if (fmisid_ts[i].value == fmisid_ts[i - 1].value)
        continue;

      end_index = i;
      location_indexes.emplace_back(std::pair<unsigned int, unsigned int>(start_index, end_index));
      start_index = i;
    }
    end_index = fmisid_ts.size();
    location_indexes.emplace_back(std::pair<unsigned int, unsigned int>(start_index, end_index));

    // Iterate through locations
    for (const auto& location_index : location_indexes)
    {
      TS::TimeSeriesVectorPtr tsv(new TS::TimeSeriesVector());
      start_index = location_index.first;
      end_index = location_index.second;
      for (unsigned int k = 0; k < observation_result->size(); k++)
      {
        const TS::TimeSeries& ts_k = observation_result->at(k);
        if (ts_k.empty())
          tsv->push_back(ts_k);
        else
        {
          TS::TimeSeries ts_ik(fmisid_ts.getLocalTimePool());
          ts_ik.insert(ts_ik.begin(), ts_k.begin() + start_index, ts_k.begin() + end_index);
          // Add missing timesteps
          add_missing_timesteps(ts_ik, tlist);
          tsv->emplace_back(ts_ik);
        }
      }

      if (fmisid_ts.empty())
        continue;
      int fmisid = get_fmisid_value(fmisid_ts[start_index].value);
      ret.emplace_back(make_pair(fmisid, tsv));
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Set precision of special parameters such as fmisid to zero
 */
// ----------------------------------------------------------------------

void fix_precisions(Query& masterquery, const ObsParameters& obsParameters)
{
  try
  {
    for (unsigned int i = 0; i < obsParameters.size(); i++)
    {
      std::string paramname(obsParameters[i].param.name());
      if (paramname == WMO_PARAM || paramname == LPNN_PARAM || paramname == FMISID_PARAM ||
          paramname == RWSID_PARAM || paramname == SENSOR_NO_PARAM || paramname == LEVEL_PARAM ||
          paramname == GEOID_PARAM)
        masterquery.precisions[i] = 0;
    }
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}
#endif

}  // namespace PostProcessing
}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
