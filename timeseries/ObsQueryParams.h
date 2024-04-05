
#pragma once

#include "State.h"
#include <spine/HTTP.h>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
struct ObsQueryParams
{
  ObsQueryParams(const Spine::HTTP::Request& req);
#ifndef WITHOUT_OBSERVATION
  std::string iot_producer_specifier;
  std::vector<int> wmos;
  std::vector<int> lpnns;
  std::vector<int> fmisids;
  std::vector<int> rwsids;
  std::vector<std::string> wsis;
  std::map<std::string, double> boundingBox;
  TS::DataFilter dataFilter;
  int numberofstations;
  std::set<std::string> stationgroups;
  bool allplaces;
  bool latestObservation;
  bool useDataCache;
#endif
  static std::set<std::string> getObsProducers(const State& state);
};

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
