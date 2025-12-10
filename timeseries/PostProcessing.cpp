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
    if (const auto* ptr = std::get_if<TS::TimeSeriesPtr>(aggregatedData.data()))
    {
      TS::TimeSeriesPtr ts_first = *ptr;
      TS::TimeSeriesPtr ts_result(new TS::TimeSeries);
      // first merge timeseries of all levels of one parameter
      for (const auto& data : aggregatedData)
      {
        TS::TimeSeriesPtr ts = std::get<TS::TimeSeriesPtr>(data);
        ts_result->insert(ts_result->end(), ts->begin(), ts->end());
      }
      // update the latest timestep, so that next query (if exists) knows from where to continue
      update_latest_timestep(query, *ts_result);
      tsdata = ts_result;
    }
    else if (std::get_if<TS::TimeSeriesGroupPtr>(aggregatedData.data()))
    {
      TS::TimeSeriesGroupPtr tsg_result(new TS::TimeSeriesGroup);
      // first merge timeseries of all levels of one parameter
      for (const auto& data : aggregatedData)
      {
        TS::TimeSeriesGroupPtr tsg = *(std::get_if<TS::TimeSeriesGroupPtr>(&data));
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
                       const std::vector<SmartMet::TimeSeries::TimeSeriesData>& outdata,
                       int& startRow)
{
  try
  {
    unsigned int numberOfParameters = paramlist.size();
    // iterate different locations

    startRow = tf.getCurrentRow();

    // iterate columns (parameters)
    int j = 0;
    for (const TS::TimeSeriesData& tsdata : outdata)
    {
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

      if (const auto* ptr = std::get_if<TS::TimeSeriesPtr>(&tsdata))
      {
        TS::TimeSeriesPtr ts = *ptr;
        tf << *ts;
      }
      else if (const auto* ptr = std::get_if<TS::TimeSeriesVectorPtr>(&tsdata))
      {
        TS::TimeSeriesVectorPtr tsv = *ptr;
        for (unsigned int k = 0; k < tsv->size(); k++)
        {
          tf.setCurrentColumn(k);
          tf.setCurrentRow(startRow);
          tf << tsv->at(k);
        }
        startRow = tf.getCurrentRow();
      }
      else if (const auto* ptr = std::get_if<TS::TimeSeriesGroupPtr>(&tsdata))
      {
        TS::TimeSeriesGroupPtr tsg = *ptr;
        tf << *tsg;
      }

      // Reset formatting to the default value
      tf << TS::LonLatFormat::LONLAT;

      j++;
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

    // std::string locationName = outputData[0].first;

    // At first check whether observations exist and put them at begin of result set when found
    TS::OutputData::const_iterator it = outputData.begin();
    if (it->first == "_obs_")
    {
      add_data_to_table(query.poptions.parameters(), tf, it->second, startRow);
      it++;
    }

    // Make index for locations to speed up the search (exclude observations when found from index)
    std::multimap<std::string, TS::OutputData::const_iterator> locationIndex;
    for (; it != outputData.end(); ++it)
    {
      locationIndex.emplace(it->first, it);
    }

    // iterate locations
    for (const auto& tloc : query.loptions->locations())
    {
      std::string locationId = get_location_id(tloc.loc);
      auto range = locationIndex.equal_range(locationId);
      for (auto it = range.first; it != range.second; ++it)
      {
        add_data_to_table(query.poptions.parameters(), tf, it->second->second, startRow);
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

#ifndef WITHOUT_OBSERVATION

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
