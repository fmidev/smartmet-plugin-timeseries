
#include "ObsQueryParams.h"
#include <boost/algorithm/string/split.hpp>
#include <macgyver/StringConversion.h>
#include <spine/Convenience.h>
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/Engine.h>
#endif

using namespace std;

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
namespace
{
void parse_ids(const std::optional<std::string>& string_param, std::vector<int>& id_vector)
{
  try
  {
    if (string_param)
    {
      std::vector<string> parts;
      boost::algorithm::split(parts, *string_param, boost::algorithm::is_any_of(","));

      for (const string& id_string : parts)
      {
        auto id_int = Fmi::stoi(id_string);
        id_vector.push_back(id_int);
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void parse_ids(const std::optional<std::string>& string_param,
               std::vector<std::string>& id_vector)
{
  try
  {
    if (string_param)
      boost::algorithm::split(id_vector, *string_param, boost::algorithm::is_any_of(","));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void add_sql_data_filter(const Spine::HTTP::Request& req,
                         const std::string& param_name,
                         TS::DataFilter& dataFilter)
{
  try
  {
    auto param = req.getParameter(param_name);

    if (param)
      dataFilter.setDataFilter(param_name, *param);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace

ObsQueryParams::ObsQueryParams(const Spine::HTTP::Request& req)
{
  try
  {
#ifndef WITHOUT_OBSERVATION
    std::string endtime = Spine::optional_string(req.getParameter("endtime"), "");
    latestObservation = (endtime == "now");
    allplaces = (Spine::optional_string(req.getParameter("places"), "") == "all");
    // observation params
    numberofstations = Spine::optional_int(req.getParameter("numberofstations"), 1);
    std::string opt_stationgroups = Spine::optional_string(req.getParameter("stationgroups"), "");
    if (!opt_stationgroups.empty())
      boost::algorithm::split(stationgroups, opt_stationgroups, boost::algorithm::is_any_of(","));

    // FMISIDs
    auto name = req.getParameter("fmisid");
    parse_ids(name, fmisids);
    // sid is an alias for fmisid
    name = req.getParameter("sid");
    parse_ids(name, fmisids);
    // WMOs
    name = req.getParameter("wmo");
    parse_ids(name, wmos);
    // RWSIDs
    name = req.getParameter("rwsid");
    parse_ids(name, rwsids);
    // LPNNs
    name = req.getParameter("lpnn");
    parse_ids(name, lpnns);
    // WIGOS Station identifiers
    name = req.getParameter("wsi");
    parse_ids(name, wsis);

    if (!!req.getParameter("bbox"))
    {
      // First remove a possible radius setting
      string bbox = *req.getParameter("bbox");
      auto radius_pos = bbox.find(':');
      if (radius_pos != string::npos)
        bbox.resize(radius_pos);

      vector<string> parts;
      boost::algorithm::split(parts, bbox, boost::algorithm::is_any_of(","));

      // Bounding box must contain exactly 4 elements
      if (parts.size() != 4 || parts[0].empty() || parts[1].empty() || parts[2].empty() ||
          parts[3].empty())
        throw Fmi::Exception(BCP, "Invalid bounding box '" + bbox + "'!");

      boundingBox["minx"] = Fmi::stod(parts[0]);
      boundingBox["miny"] = Fmi::stod(parts[1]);
      boundingBox["maxx"] = Fmi::stod(parts[2]);
      boundingBox["maxy"] = Fmi::stod(parts[3]);
    }
    // Data filter options
    add_sql_data_filter(req, "station_id", dataFilter);
    add_sql_data_filter(req, "data_quality", dataFilter);
    // Sounding filtering options
    add_sql_data_filter(req, "sounding_type", dataFilter);
    add_sql_data_filter(req, "significance", dataFilter);

    std::string dataCache = Spine::optional_string(req.getParameter("useDataCache"), "true");
    useDataCache = (dataCache == "true" || dataCache == "1");
#endif
  }
  catch (...)
  {
    // Stack traces in journals are useless when the user has made a typo
    throw Fmi::Exception::Trace(BCP, "TimeSeries plugin failed to parse query string options!")
        .disableLogging();
  }
}

std::set<std::string> ObsQueryParams::getObsProducers(const State& state)
{
  try
  {
    std::set<std::string> ret;
#ifndef WITHOUT_OBSERVATION
    auto* obsengine = state.getObsEngine();
    if (obsengine != nullptr)
      ret = obsengine->getValidStationTypes();
#endif
    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
