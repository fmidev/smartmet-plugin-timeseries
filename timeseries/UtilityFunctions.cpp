#include "UtilityFunctions.h"
#include "State.h"
#include <timeseries/ParameterTools.h>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
namespace UtilityFunctions
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
    if (boost::get<TS::TimeSeriesPtr>(&aggregatedData[0]))
    {
      TS::TimeSeriesPtr ts_first = *(boost::get<TS::TimeSeriesPtr>(&aggregatedData[0]));
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
    else if (boost::get<TS::TimeSeriesGroupPtr>(&aggregatedData[0]))
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

bool is_special_parameter(const std::string& paramname)
{
  if(paramname == ORIGINTIME_PARAM || paramname == LEVEL_PARAM)
	return false;

  return (TS::is_time_parameter(paramname) || TS::is_location_parameter(paramname));
}

void get_special_parameter_values(const std::string& paramname,
								  int precision,
								  const TS::TimeSeriesGenerator::LocalTimeList& tlist,
								  const Spine::LocationPtr& loc,
								  const Query& query,
								  const State& state,
								  const Fmi::TimeZones& timezones,
								  TS::TimeSeriesPtr& result)
{
  bool is_time_parameter = TS::is_time_parameter(paramname);
  bool is_location_parameter = TS::is_location_parameter(paramname);

  for (const auto& timestep : tlist)
	{
	  if(is_time_parameter)
		{
		  TS::Value value = TS::time_parameter(paramname,
											   timestep,
											   state.getTime(),
											   *loc,
											   query.timezone,
											   timezones,
											   query.outlocale,
											   *query.timeformatter,
											   query.timestring);
		  result->emplace_back(TS::TimedValue(timestep, value));

		}
	  if(is_location_parameter)
		{
		  TS::Value value = TS::location_parameter(loc,
												   paramname,
												   query.valueformatter,
												   query.timezone,
												   precision,
												   query.crs);
					
		  result->emplace_back(TS::TimedValue(timestep, value));
		}
	}
}

void get_special_parameter_values(const std::string& paramname,
								  int precision,
								  const TS::TimeSeriesGenerator::LocalTimeList& tlist,
								  const Spine::LocationList& llist,
								  const Query& query,
								  const State& state,
								  const Fmi::TimeZones& timezones,
								  TS::TimeSeriesGroupPtr& result)
{
  bool is_time_parameter = TS::is_time_parameter(paramname);
  bool is_location_parameter = TS::is_location_parameter(paramname);
  for (const auto& loc : llist)
	{
	  auto timeseries = TS::TimeSeries(state.getLocalTimePool()); 
	  for (const auto& timestep : tlist)
		{
		  if(is_time_parameter)
			{
			  TS::Value value = TS::time_parameter(paramname,
												   timestep,
												   state.getTime(),
												   *loc,
												   query.timezone,
												   timezones,
												   query.outlocale,
												   *query.timeformatter,
												   query.timestring);
			  timeseries.emplace_back(TS::TimedValue(timestep, value));
			  
			}
		  if(is_location_parameter)
			{
			  TS::Value value = TS::location_parameter(loc,
													   paramname,
													   query.valueformatter,
													   query.timezone,
													   precision,
													   query.crs);
			  
			  timeseries.emplace_back(TS::TimedValue(timestep, value));
			}
		}
	  if(!timeseries.empty())
		result->push_back(TS::LonLatTimeSeries(Spine::LonLat(loc->longitude, loc->latitude), timeseries));
	}
}


}  // namespace UtilityFunctions
}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
