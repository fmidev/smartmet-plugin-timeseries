// ======================================================================
/*!
 * \brief Time series generator for forecasts and observations
 */
// ======================================================================

#include "TimeSeriesGenerator.h"
#include <macgyver/Exception.h>
#include <macgyver/TimeParser.h>

namespace bp = boost::posix_time;
namespace bg = boost::gregorian;
namespace bl = boost::local_time;

namespace SmartMet
{
namespace TimeSeries
{
namespace TimeSeriesGenerator
{
namespace
{
const int default_timestep = 60;

// ----------------------------------------------------------------------
/*!
 * \brief Generate fixed HHMM times
 */
// ----------------------------------------------------------------------

void generate_fixedtimes_until_endtime(std::set<bl::local_date_time>& theTimes,
                                       const TimeSeriesGeneratorOptions& theOptions,
                                       const bl::local_date_time& theStartTime,
                                       const bl::local_date_time& theEndTime,
                                       const bl::time_zone_ptr& theZone)
{
  try
  {
    bl::local_time_period period(theStartTime, theEndTime);

    bg::date today = theStartTime.local_time().date();
    bg::day_iterator day(today, 1);

    while (true)
    {
      for (unsigned int hhmm : theOptions.timeList)
      {
        unsigned int hh = hhmm / 100;
        unsigned int mm = hhmm % 100;

        bl::local_date_time d =
            Fmi::TimeParser::make_time(*day, bp::hours(hh) + bp::minutes(mm), theZone);

        if (d.is_not_a_date_time())
          continue;

        if (period.contains(d) || d == theEndTime)
          theTimes.insert(d);
        if (*day > theEndTime.local_time().date())
          return;
      }

      ++day;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate fixed HHMM times
 */
// ----------------------------------------------------------------------

void generate_fixedtimes_for_number_of_steps(std::set<bl::local_date_time>& theTimes,
                                             const TimeSeriesGeneratorOptions& theOptions,
                                             const bl::local_date_time& theStartTime,
                                             const bl::local_date_time& theEndTime,
                                             const bl::time_zone_ptr& theZone)
{
  try
  {
    bl::local_time_period period(theStartTime, theEndTime);

    bg::date today = theStartTime.local_time().date();
    bg::day_iterator day(today, 1);

    while (true)
    {
      for (unsigned int hhmm : theOptions.timeList)
      {
        unsigned int hh = hhmm / 100;
        unsigned int mm = hhmm % 100;

        bl::local_date_time d =
            Fmi::TimeParser::make_time(*day, bp::hours(hh) + bp::minutes(mm), theZone);

        if (d.is_not_a_date_time())
          continue;

        if (d >= theStartTime)
          theTimes.insert(d);
        if (theTimes.size() >= *theOptions.timeSteps)
          return;
      }

      ++day;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate fixed HHMM times
 */
// ----------------------------------------------------------------------

void generate_fixedtimes(std::set<bl::local_date_time>& theTimes,
                         const TimeSeriesGeneratorOptions& theOptions,
                         const bl::local_date_time& theStartTime,
                         const bl::local_date_time& theEndTime,
                         const bl::time_zone_ptr& theZone)
{
  try
  {
    if (theOptions.timeList.empty())
      return;

    if (!theOptions.timeSteps)
      generate_fixedtimes_until_endtime(theTimes, theOptions, theStartTime, theEndTime, theZone);
    else
      generate_fixedtimes_for_number_of_steps(
          theTimes, theOptions, theStartTime, theEndTime, theZone);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate time step series
 *
 * There may be a fixed number of timesteps or a fixed end time.
 */
// ----------------------------------------------------------------------

void generate_timesteps(std::set<bl::local_date_time>& theTimes,
                        const TimeSeriesGeneratorOptions& theOptions,
                        const bl::local_date_time& theStartTime,
                        const bl::local_date_time& theEndTime,
                        const bl::time_zone_ptr& theZone)
{
  try
  {
    unsigned int timestep = (!theOptions.timeStep ? default_timestep : *theOptions.timeStep);

    // Special case timestep=0: return {starttime,endtime}, or a single element list if they're the
    // same
    if (timestep == 0)
    {
      theTimes.insert(theStartTime);
      theTimes.insert(theEndTime);
      return;
    }

    // Normal case: timeStep > 0

    bl::local_time_period period(theStartTime, theEndTime);

    bg::date day = theStartTime.local_time().date();

    int mins = 0;

    while (true)
    {
      if (mins >= 24 * 60)
      {
        mins -= 24 * 60;
        day += bg::days(1);
      }

      bl::local_date_time t = Fmi::TimeParser::make_time(day, bp::minutes(mins), theZone);

      // In the first case we can test after the insert if
      // we should break based on the number of times, in
      // the latter case we must validate the time first
      // to prevent its inclusion.

      if (!!theOptions.timeSteps)
      {
        if (!t.is_not_a_date_time() && t >= theStartTime)
          theTimes.insert(t);

        if (theTimes.size() >= *theOptions.timeSteps)
          break;
      }
      else
      {
        if (!t.is_not_a_date_time() && (period.contains(t) || t == theEndTime))
          theTimes.insert(t);
        if (t > theEndTime)
          break;
      }

      mins += timestep;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate data steps
 */
// ----------------------------------------------------------------------

void generate_datatimes_climatology(std::set<bl::local_date_time>& theTimes,
                                    const TimeSeriesGeneratorOptions& theOptions,
                                    const bl::local_date_time& theStartTime,
                                    const bl::local_date_time& theEndTime,
                                    const bl::time_zone_ptr& theZone)
{
  try
  {
    bl::local_time_period period(theStartTime, theEndTime);

    // Climatology - must change years accordingly
    short unsigned int startyear = theStartTime.date().year();
    short unsigned int endyear = theEndTime.date().year();

    // Note that startyear<endyear is possible, hence we need to loop over possible years
    for (short unsigned int year = startyear; year <= endyear; ++year)
    {
      // Handle all climatology times
      for (const bp::ptime& t : *theOptions.getDataTimes())
      {
        // Done if a max number of timesteps was requested
        if (theOptions.timeSteps && theTimes.size() >= *theOptions.timeSteps)
          break;

        try
        {
          bp::ptime t2(bg::date(year, t.date().month(), t.date().day()), t.time_of_day());
          bl::local_date_time lt(t2, theZone);
          // Done if beyond the requested end time
          if (lt > theEndTime)
            break;

          if (period.contains(lt) || lt == theEndTime)
            theTimes.insert(lt);
        }
        catch (...)
        {
          // 29.2. does not exist for all years
        }
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
 * \brief Generate data steps
 */
// ----------------------------------------------------------------------

void generate_datatimes_normal(std::set<bl::local_date_time>& theTimes,
                               const TimeSeriesGeneratorOptions& theOptions,
                               const bl::local_date_time& theStartTime,
                               const bl::local_date_time& theEndTime,
                               const bl::time_zone_ptr& theZone)
{
  try
  {
    bl::local_time_period period(theStartTime, theEndTime);

    // The timesteps available in the data itself

    bool use_timesteps = !!theOptions.timeSteps;

    // Normal data - no need to fiddle with years
    if (use_timesteps)
    {
      for (const bp::ptime& t : *theOptions.getDataTimes())
      {
        bl::local_date_time lt(t, theZone);
        if (lt >= theStartTime && theTimes.size() < *theOptions.timeSteps)
          theTimes.insert(lt);
        if (theTimes.size() >= *theOptions.timeSteps)
          break;
      }
    }
    else
    {
      for (const bp::ptime& t : *theOptions.getDataTimes())
      {
        bl::local_date_time lt(t, theZone);

        if (period.contains(lt) || lt == theEndTime)
          theTimes.insert(lt);
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
 * \brief Generate data steps
 */
// ----------------------------------------------------------------------

void generate_datatimes(std::set<bl::local_date_time>& theTimes,
                        const TimeSeriesGeneratorOptions& theOptions,
                        const bl::local_date_time& theStartTime,
                        const bl::local_date_time& theEndTime,
                        const bl::time_zone_ptr& theZone)
{
  try
  {
    if (theOptions.isClimatology)
      generate_datatimes_climatology(theTimes, theOptions, theStartTime, theEndTime, theZone);
    else
      generate_datatimes_normal(theTimes, theOptions, theStartTime, theEndTime, theZone);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate data steps for graphs
 */
// ----------------------------------------------------------------------

void generate_graphtimes(std::set<bl::local_date_time>& theTimes,
                         const TimeSeriesGeneratorOptions& theOptions,
                         const bl::local_date_time& theStartTime,
                         const bl::local_date_time& theEndTime,
                         const bl::time_zone_ptr& theZone)
{
  try
  {
    // Wall clock time rounded up to the next hour so that
    // the graph doesn't have a gap at the beginning

    // Round the start up to the next hour

    long extraseconds = theStartTime.local_time().time_of_day().total_seconds() % 3600;
    if (extraseconds > 0)
    {
      bl::local_date_time t1(theStartTime);
      t1 += bp::seconds(3600 - extraseconds);
      theTimes.insert(t1);
    }

    generate_datatimes(theTimes, theOptions, theStartTime, theEndTime, theZone);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Generate time series for the given timezone
 */
// ----------------------------------------------------------------------

LocalTimeList generate(const TimeSeriesGeneratorOptions& theOptions,
                       const boost::local_time::time_zone_ptr& theZone)
{
  try
  {
    // Determine start and end times

    bl::local_date_time starttime(bl::not_a_date_time);
    bl::local_date_time endtime(bl::not_a_date_time);

    // Adjust to given timezone if input was not UTC. Note that if start and end times
    // are omitted, we use the data times for climatology data just like for normal data.

    if (theOptions.startTimeData)
    {
      if (theOptions.getDataTimes()->empty())
        return {};
      starttime = bl::local_date_time(theOptions.getDataTimes()->front(), theZone);
    }
    else if (!theOptions.startTimeUTC)
      starttime = Fmi::TimeParser::make_time(
          theOptions.startTime.date(), theOptions.startTime.time_of_day(), theZone);
    else
      starttime = bl::local_date_time(theOptions.startTime, theZone);

    if (theOptions.endTimeData)
    {
      if (theOptions.getDataTimes()->empty())
        return {};
      endtime = bl::local_date_time(theOptions.getDataTimes()->back(), theZone);
    }
    else if (!theOptions.endTimeUTC)
      endtime = Fmi::TimeParser::make_time(
          theOptions.endTime.date(), theOptions.endTime.time_of_day(), theZone);
    else
      endtime = bl::local_date_time(theOptions.endTime, theZone);

    // Start generating a set of unique local times

    std::set<bl::local_date_time> times;

    switch (theOptions.mode)
    {
      case TimeSeriesGeneratorOptions::FixedTimes:
        generate_fixedtimes(times, theOptions, starttime, endtime, theZone);
        break;
      case TimeSeriesGeneratorOptions::TimeSteps:
        generate_timesteps(times, theOptions, starttime, endtime, theZone);
        break;
      case TimeSeriesGeneratorOptions::DataTimes:
        generate_datatimes(times, theOptions, starttime, endtime, theZone);
        break;
      case TimeSeriesGeneratorOptions::GraphTimes:
        generate_graphtimes(times, theOptions, starttime, endtime, theZone);
        break;
    }

    // Copy the unique times into actual output container

    LocalTimeList ret;
    std::copy(times.begin(), times.end(), back_inserter(ret));
    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace TimeSeriesGenerator
}  // namespace TimeSeries
}  // namespace SmartMet
