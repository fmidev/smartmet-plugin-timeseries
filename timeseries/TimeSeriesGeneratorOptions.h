// ======================================================================
/*!
 * \brief Options for generating a time series
 */
// ======================================================================

#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <list>
#include <ostream>
#include <set>

#include <spine/HTTP.h>

namespace SmartMet
{
namespace TimeSeries
{
struct TimeSeriesGeneratorOptions
{
 public:
  enum Mode
  {
    DataTimes,   // timesteps in querydata
    GraphTimes,  // above plus "now"
    FixedTimes,  // fixed hours etc
    TimeSteps    // fixed timestep
  };

  // Timesteps established from the outside
  using TimeList = boost::shared_ptr<std::list<boost::posix_time::ptime>>;

  // Methods

  TimeSeriesGeneratorOptions(
      const boost::posix_time::ptime& now = boost::posix_time::second_clock::universal_time());

  std::size_t hash_value() const;

  // All timesteps are to be used?
  bool all() const;

  // Handle data times
  void setDataTimes(const TimeList& times, bool climatology = false);
  const TimeList& getDataTimes() const;

  Mode mode = Mode::TimeSteps;              // algorithm selection
  boost::posix_time::ptime startTime;       // start time
  boost::posix_time::ptime endTime;         // end time
  bool startTimeUTC = true;                 // timestamps can be interpreted to be in
  bool endTimeUTC = true;                   // UTC time or in some specific time zone
  boost::optional<unsigned int> timeSteps;  // number of time steps
  boost::optional<unsigned int> timeStep;   // Mode:TimeSteps, timestep in minutes
  std::set<unsigned int> timeList;          // Mode:FixedTimes,  integers of form HHMM
 private:
  TimeList dataTimes;  // Mode:DataTimes, Fixed times set from outside
 public:
  bool startTimeData = false;  // Take start time from data
  bool endTimeData = false;    // Take end time from data
  bool isClimatology = false;
};

TimeSeriesGeneratorOptions parseTimes(const Spine::HTTP::Request& theReq);

std::ostream& operator<<(std::ostream& stream, const TimeSeriesGeneratorOptions& opt);

}  // namespace TimeSeries
}  // namespace SmartMet

// ======================================================================
