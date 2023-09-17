// ======================================================================
/*!
 * \brief Options for generating a time series
 */
// ======================================================================

#include "TimeSeriesGeneratorOptions.h"
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>
#include <macgyver/StringConversion.h>
#include <spine/Convenience.h>
#include <spine/HTTP.h>

namespace SmartMet
{
using Spine::duration_string_to_minutes;
using Spine::optional_int;
using Spine::optional_string;
using Spine::optional_time;
using Spine::required_string;

namespace TimeSeries
{
const int default_timestep = 60;

// ----------------------------------------------------------------------
/*!
 * \brief The default constructor marks most options as missing
 *
 * The moment of construction determines what "now" means as far
 * as time generation is concerned, unless the value is overwritten
 * after construction.
 */
// ----------------------------------------------------------------------

TimeSeriesGeneratorOptions::TimeSeriesGeneratorOptions(const boost::posix_time::ptime& now)
    : startTime(now), endTime(now), dataTimes(new TimeList::element_type())
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate a unique hash for the options
 */
// ----------------------------------------------------------------------

std::size_t TimeSeriesGeneratorOptions::hash_value() const
{
  try
  {
    std::size_t hash = 0;
    Fmi::hash_combine(hash, Fmi::hash_value(static_cast<int>(mode)));
    Fmi::hash_combine(hash, Fmi::hash_value(Fmi::to_iso_string(startTime)));
    Fmi::hash_combine(hash, Fmi::hash_value(Fmi::to_iso_string(endTime)));
    Fmi::hash_combine(hash, Fmi::hash_value(startTimeUTC));
    Fmi::hash_combine(hash, Fmi::hash_value(endTimeUTC));
    if (timeSteps)
    {
      Fmi::hash_combine(hash, Fmi::hash_value(*timeSteps));
    }
    if (timeStep)
    {
      Fmi::hash_combine(hash, Fmi::hash_value(*timeStep));
    }
    for (const auto& t : timeList)
      Fmi::hash_combine(hash, Fmi::hash_value(t));
    Fmi::hash_combine(hash, Fmi::hash_value(*dataTimes));
    Fmi::hash_combine(hash, Fmi::hash_value(startTimeData));
    Fmi::hash_combine(hash, Fmi::hash_value(endTimeData));
    Fmi::hash_combine(hash, Fmi::hash_value(isClimatology));
    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true if all available timesteps are to be output
 */
// ----------------------------------------------------------------------

bool TimeSeriesGeneratorOptions::all() const
{
  try
  {
    switch (mode)
    {
      case DataTimes:
      case GraphTimes:
        return true;
      case FixedTimes:
        return false;
      case TimeSteps:
        return (!timeStep || *timeStep == 0);
    }
    // Compilers cannot see the above handles all cases
    return false;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Print the options to the given stream
 *
 * This is needed for debugging purposes only.
 */
// ----------------------------------------------------------------------

std::ostream& operator<<(std::ostream& stream, const TimeSeriesGeneratorOptions& opt)
{
  try
  {
    stream << "TimeSeriesGeneratorOptions:\n";
    stream << "    mode          : ";  // << static_cast<int>(opt.mode) << "\n";
    switch (opt.mode)
    {
      case TimeSeriesGeneratorOptions::DataTimes:
        stream << "DataTimes";
        break;
      case TimeSeriesGeneratorOptions::GraphTimes:
        stream << "GraphTiems";
        break;
      case TimeSeriesGeneratorOptions::FixedTimes:
        stream << "FixedTimes";
        break;
      case TimeSeriesGeneratorOptions::TimeSteps:
        stream << "TimeSteps";
        break;
    }
    stream << "\n    startTime     : " << opt.startTime << "\n";
    stream << "    endTime       : " << opt.endTime << "\n";
    stream << "    startTimeUTC  : " << opt.startTimeUTC << "\n";
    stream << "    endTimeUTC    : " << opt.endTimeUTC << "\n";
    stream << "    timeSteps     : ";
    if (opt.timeSteps)
      stream << *opt.timeSteps;
    stream << "\n    timeStep      : ";
    if (opt.timeStep)
      stream << *opt.timeStep;
    stream << "\n    dataTimes     : ";
    stream << "\n    startTimeData : " << opt.startTimeData << "\n";
    stream << "\n    endTimeData   : " << opt.endTimeData << "\n";
    std::copy(opt.timeList.begin(), opt.timeList.end(), std::ostream_iterator<int>(stream, " "));
    stream << "\n    dataTimes     : ";
    for (const auto& t : *opt.getDataTimes())
    {
      stream << " '" << t << "'";
    }
    stream << "\n";

    return stream;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the data times
 */
// ----------------------------------------------------------------------

void TimeSeriesGeneratorOptions::setDataTimes(const TimeList& times, bool climatology)
{
  try
  {
    dataTimes = times;
    isClimatology = climatology;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the data times
 */
// ----------------------------------------------------------------------

const TimeSeriesGeneratorOptions::TimeList& TimeSeriesGeneratorOptions::getDataTimes() const
{
  return dataTimes;
}

// Parse hour option

void parse_hour(TimeSeriesGeneratorOptions& options, const Spine::HTTP::Request& theReq)
{
  if (theReq.getParameter("hour"))
  {
    options.mode = TimeSeriesGeneratorOptions::FixedTimes;

    const std::string hours = required_string(theReq.getParameter("hour"), "");

    std::vector<std::string> parts;
    boost::algorithm::split(parts, hours, boost::algorithm::is_any_of(","));
    for (const std::string& part : parts)
    {
      int hour = Fmi::stoi(part);
      if (hour < 0 || hour > 23)
        throw Fmi::Exception(BCP, "Invalid hour selection '" + part + "'!");

      options.timeList.insert(hour * 100);
    }
  }
}

// Parse time option
void parse_time(TimeSeriesGeneratorOptions& options, const Spine::HTTP::Request& theReq)
{
  if (theReq.getParameter("time"))
  {
    options.mode = TimeSeriesGeneratorOptions::FixedTimes;
    const std::string times = required_string(theReq.getParameter("time"), "");

    std::vector<std::string> parts;
    boost::algorithm::split(parts, times, boost::algorithm::is_any_of(","));

    for (const std::string& part : parts)
    {
      int th = Fmi::stoi(part);
      if (th < 0 || th > 2359)
        throw Fmi::Exception(BCP, "Invalid time selection '" + part + "'!");

      options.timeList.insert(th);
    }
  }
}

void parse_timestep(TimeSeriesGeneratorOptions& options, const Spine::HTTP::Request& theReq)
{
  if (theReq.getParameter("timestep"))
  {
    if (options.mode != TimeSeriesGeneratorOptions::TimeSteps)
      throw Fmi::Exception(
          BCP, "Cannot use timestep option when another time mode is implied by another option");

    const std::string step = optional_string(theReq.getParameter("timestep"), "");

    if (step == "data" || step == "all")
    {
      options.mode = TimeSeriesGeneratorOptions::DataTimes;
      options.timeStep = 0;
    }
    else if (step == "graph")
    {
      options.mode = TimeSeriesGeneratorOptions::GraphTimes;
      options.timeStep = 0;
    }
    else if (step != "current")
    {
      options.mode = TimeSeriesGeneratorOptions::TimeSteps;
      int num = duration_string_to_minutes(step);

      if (num < 0)
        throw Fmi::Exception(BCP, "The 'timestep' option cannot be negative!");

      if (num > 0 && 1440 % num != 0)
        throw Fmi::Exception(BCP, "Timestep must be a divisor of 24*60 or zero for all timesteps!");

      options.timeStep = num;
    }
  }
}

void parse_timesteps(TimeSeriesGeneratorOptions& options, const Spine::HTTP::Request& theReq)
{
  if (theReq.getParameter("timesteps"))
  {
    const std::string steps = optional_string(theReq.getParameter("timesteps"), "");

    int num = Fmi::stoi(steps);
    if (num < 0)
      throw Fmi::Exception(BCP, "The 'timesteps' option cannot be negative!");

    options.timeSteps = num;
  }
}

void parse_starttime(TimeSeriesGeneratorOptions& options, const Spine::HTTP::Request& theReq)
{
  if (theReq.getParameter("starttime"))
  {
    std::string stamp = required_string(theReq.getParameter("starttime"), "");

    if (stamp == "data")
      options.startTimeData = true;

    else
    {
      options.startTime = Fmi::TimeParser::parse(stamp);
      options.startTimeUTC = Fmi::TimeParser::looks_utc(stamp);
    }
  }
  else
  {
    options.startTimeUTC = true;  // generate from "now" in all locations
  }
}

void parse_startstep(TimeSeriesGeneratorOptions& options, const Spine::HTTP::Request& theReq)
{
  if (theReq.getParameter("startstep"))
  {
    int startstep = optional_int(theReq.getParameter("startstep"), 0);

    if (startstep < 0)
      throw Fmi::Exception(BCP, "The 'startstep' option cannot be negative!");
    if (startstep > 10000)
      throw Fmi::Exception(BCP, "Too large 'startstep' value!");

    int timestep = (options.timeStep ? *options.timeStep : default_timestep);

    options.startTime +=
        boost::posix_time::minutes(boost::numeric_cast<unsigned int>(startstep) * timestep);
  }
}

void parse_endtime(TimeSeriesGeneratorOptions& options, const Spine::HTTP::Request& theReq)
{
  if (theReq.getParameter("endtime"))
  {
    std::string stamp = required_string(theReq.getParameter("endtime"), "");
    if (stamp != "now")
    {
      if (!!options.timeSteps)
        throw Fmi::Exception(BCP, "Cannot specify 'timesteps' and 'endtime' simultaneously!");

      if (stamp == "data")
        options.endTimeData = true;
      else
      {
        // iso, sql, xml, timestamp obey tz, epoch is always UTC
        options.endTime = Fmi::TimeParser::parse(stamp);
        options.endTimeUTC = Fmi::TimeParser::looks_utc(stamp);
      }
    }
  }
  else if (!!options.timeSteps)
  {
    options.endTimeUTC = options.startTimeUTC;
    // If you give the number of timesteps, we must assume a default timestep unless you give
    // one
    if (!options.timeStep)
      options.timeStep = default_timestep;
    options.endTime =
        options.startTime + boost::posix_time::minutes(*options.timeStep * *options.timeSteps);
  }
  else
  {
    options.endTimeUTC = options.startTimeUTC;
    options.endTime = options.startTime + boost::posix_time::hours(24);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse time series generation options.
 *
 * Note: If you add a new parameter, please do not forget to change
 *       the timeseries-plugin hash_value method accordinly.
 */
// ----------------------------------------------------------------------

TimeSeriesGeneratorOptions parseTimes(const Spine::HTTP::Request& theReq)
{
  try
  {
    boost::posix_time::ptime now = optional_time(theReq.getParameter("now"),
                                                 boost::posix_time::second_clock::universal_time());

    TimeSeriesGeneratorOptions options(now);

    // We first parse the options which imply the TimeMode
    // because the behaviour of some other options depend on it

    parse_hour(options, theReq);
    parse_time(options, theReq);

    // Timestep should be parsed last so we can check if the defaults have been changed

    parse_timestep(options, theReq);

    // TIMEMODE HAS NOW BEEN DETERMINED

    // The number of timesteps is a spine option for various modes,
    // including DataTimes and GraphTimes

    parse_timesteps(options, theReq);

    // starttime option may modify "now" which is the default
    // value set by the constructor

    parse_starttime(options, theReq);

    // startstep must be handled after starttime and timestep options

    parse_startstep(options, theReq);

    // endtime must be handled last since it may depend on starttime,
    // timestep and timesteps options. The default is to set the end
    // time to the start time plus 24 hours.

    parse_endtime(options, theReq);

    return options;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace TimeSeries
}  // namespace SmartMet

// ======================================================================
