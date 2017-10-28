// ======================================================================
/*!
 * \brief SmartMet TimeSeries plugin implementation
 */
// ======================================================================

#include "Plugin.h"
#include "Hash.h"
#include "Query.h"
#include "QueryLevelDataCache.h"
#include "State.h"

#include <engines/gis/Engine.h>
#include <engines/querydata/OriginTime.h>
#include <spine/Exception.h>
#include <spine/SmartMet.h>

#include <spine/Convenience.h>
#include <spine/ParameterFactory.h>
#include <spine/Table.h>
#include <spine/TableFeeder.h>
#include <spine/TableFormatterFactory.h>
#include <spine/TimeSeriesAggregator.h>
#include <spine/TimeSeriesOutput.h>
#include <spine/ValueFormatter.h>

#include <macgyver/Astronomy.h>
#include <macgyver/CharsetTools.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeFormatter.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/foreach.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/timer/timer.hpp>
#include <boost/tokenizer.hpp>

#include <newbase/NFmiIndexMask.h>
#include <newbase/NFmiIndexMaskTools.h>
#include <newbase/NFmiMultiQueryInfo.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiSvgTools.h>

#include <gis/OGR.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>

using namespace std;
using namespace boost::posix_time;
using namespace boost::gregorian;
using namespace boost::local_time;
using boost::numeric_cast;
using boost::numeric::bad_numeric_cast;
using boost::numeric::positive_overflow;
using boost::numeric::negative_overflow;

// #define MYDEBUG ON

namespace ts = SmartMet::Spine::TimeSeries;

namespace SmartMet
{
bool special(const Spine::Parameter& theParam)
{
  try
  {
    switch (theParam.type())
    {
      case Spine::Parameter::Type::Data:
      case Spine::Parameter::Type::Landscaped:
        return false;
      case Spine::Parameter::Type::DataDerived:
      case Spine::Parameter::Type::DataIndependent:
        return true;
    }
    // ** NOT REACHED **
    return true;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

namespace Plugin
{
namespace TimeSeries
{
namespace
{
const std::string FMISID_PARAM = "fmisid";
const std::string WMO_PARAM = "wmo";
const std::string LPNN_PARAM = "lpnn";
const std::string GEOID_PARAM = "geoid";
const std::string RWSID_PARAM = "rwsid";
const std::string LEVEL_PARAM = "level";
const std::string SENSOR_NO_PARAM = "sensor_no";
const std::string NAME_PARAM = "name";
const std::string STATIONNAME_PARAM = "stationname";
const std::string STATION_NAME_PARAM = "station_name";
const std::string LON_PARAM = "lon";
const std::string LAT_PARAM = "lat";
const std::string LATLON_PARAM = "latlon";
const std::string NEARLATLON_PARAM = "nearlatlon";
const std::string LONLAT_PARAM = "lonlat";
const std::string NEARLONLAT_PARAM = "nearlonlat";
const std::string PLACE_PARAM = "place";
const std::string STATIONLON_PARAM = "stationlon";
const std::string STATIONLAT_PARAM = "stationlat";
const std::string STATIONLONGITUDE_PARAM = "stationlongitude";
const std::string STATIONLATITUDE_PARAM = "stationlatitude";
const std::string STATION_ELEVATION_PARAM = "station_elevation";
const std::string STATIONARY_PARAM = "stationary";
const std::string DISTANCE_PARAM = "distance";
const std::string DIRECTION_PARAM = "direction";
const std::string LONGITUDE_PARAM = "longitude";
const std::string LATITUDE_PARAM = "latitude";
const std::string ISO2_PARAM = "iso2";
const std::string REGION_PARAM = "region";
const std::string COUNTRY_PARAM = "country";
const std::string ELEVATION_PARAM = "elevation";
const std::string TIME_PARAM = "time";
const std::string ISOTIME_PARAM = "isotime";
const std::string XMLTIME_PARAM = "xmltime";
const std::string LOCALTIME_PARAM = "localtime";
const std::string ORIGINTIME_PARAM = "origintime";
const std::string UTC_PARAM = "utc";
const std::string UTCTIME_PARAM = "utctime";
const std::string EPOCHTIME_PARAM = "epochtime";
const std::string SUNELEVATION_PARAM = "sunelevation";
const std::string SUNDECLINATION_PARAM = "sundeclination";
const std::string SUNATZIMUTH_PARAM = "sunatzimuth";
const std::string DARK_PARAM = "dark";
const std::string MOONPHASE_PARAM = "moonphase";
const std::string MOONRISE_PARAM = "moonrise";
const std::string MOONRISE2_PARAM = "moonrise2";
const std::string MOONSET_PARAM = "moonset";
const std::string MOONSET2_PARAM = "moonset2";
const std::string MOONRISETODAY_PARAM = "moonrisetoday";
const std::string MOONRISE2TODAY_PARAM = "moonrise2today";
const std::string MOONSETTODAY_PARAM = "moonsettoday";
const std::string MOONSET2TODAY_PARAM = "moonset2today";
const std::string MOONUP24H_PARAM = "moonup24h";
const std::string MOONDOWN24H_PARAM = "moondown24h";
const std::string SUNRISE_PARAM = "sunrise";
const std::string SUNSET_PARAM = "sunset";
const std::string NOON_PARAM = "noon";
const std::string SUNRISETODAY_PARAM = "sunrisetoday";
const std::string SUNSETTODAY_PARAM = "sunsettoday";
const std::string DAYLENGTH_PARAM = "daylength";
const std::string TIMESTRING_PARAM = "timestring";
const std::string WDAY_PARAM = "wday";
const std::string WEEKDAY_PARAM = "weekday";
const std::string MON_PARAM = "mon";
const std::string MONTH_PARAM = "month";
const std::string HOUR_PARAM = "hour";
const std::string FEATURE_PARAM = "feature";
const std::string LOCALTZ_PARAM = "localtz";
const std::string TZ_PARAM = "tz";
const std::string POPULATION_PARAM = "population";
const std::string PRODUCER_PARAM = "producer";
const std::string STATIONTYPE_PARAM = "stationtype";
const std::string FLASH_PRODUCER = "flash";
const std::string SYKE_PRODUCER = "syke";
const std::string MODEL_PARAM = "model";

// ----------------------------------------------------------------------
/*!
 * \brief Return true if the parameter if of aggregatable type
 */
// ----------------------------------------------------------------------

bool parameterIsArithmetic(const Spine::Parameter& theParameter)
{
  try
  {
    switch (theParameter.type())
    {
      case Spine::Parameter::Type::Data:
      case Spine::Parameter::Type::Landscaped:
      case Spine::Parameter::Type::DataDerived:
        return true;
      case Spine::Parameter::Type::DataIndependent:
        return false;
    }
    // NOT REACHED //
    return false;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true if the given parameters depend on location only
 */
// ----------------------------------------------------------------------

bool is_plain_location_query(const Spine::OptionParsers::ParameterList& theParams)
{
  try
  {
    for (const auto& param : theParams)
    {
      const auto& name = param.name();
      if (name != NAME_PARAM && name != ISO2_PARAM && name != GEOID_PARAM && name != REGION_PARAM &&
          name != COUNTRY_PARAM && name != FEATURE_PARAM && name != LOCALTZ_PARAM &&
          name != LATITUDE_PARAM && name != LONGITUDE_PARAM && name != LATLON_PARAM &&
          name != LONLAT_PARAM && name != POPULATION_PARAM && name != ELEVATION_PARAM)
        return false;
    }
    return true;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Find producer data with matching coordinates
 */
// ----------------------------------------------------------------------

Engine::Querydata::Producer select_producer(const Engine::Querydata::Engine& querydata,
                                            const Spine::Location& location,
                                            const Query& query,
                                            const AreaProducers& areaproducers)
{
  try
  {
    // Allow querydata to use data specific max distances if the maxdistance option
    // was not given in the query string

    bool use_data_max_distance = !query.maxdistanceOptionGiven;

    if (areaproducers.empty())
      return querydata.find(location.longitude,
                            location.latitude,
                            query.maxdistance,
                            use_data_max_distance,
                            query.leveltype);

    // Allow listed producers only
    return querydata.find(areaproducers,
                          location.longitude,
                          location.latitude,
                          query.maxdistance,
                          use_data_max_distance,
                          query.leveltype);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void make_point_path(NFmiSvgPath& thePath, const std::pair<double, double>& thePoint)
{
  try
  {
    NFmiSvgPath::Element element1(NFmiSvgPath::kElementMoveto, thePoint.first, thePoint.second);
    NFmiSvgPath::Element element2(NFmiSvgPath::kElementClosePath, 0, 0);
    thePath.push_back(element1);
    thePath.push_back(element2);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

// for bounding box
void make_rectangle_path(NFmiSvgPath& thePath,
                         const std::pair<double, double>& theFirstCorner,
                         const std::pair<double, double>& theSecondCorner)
{
  try
  {
    std::pair<double, double> point1(theFirstCorner);
    std::pair<double, double> point2(theFirstCorner.first, theSecondCorner.second);
    std::pair<double, double> point3(theSecondCorner);
    std::pair<double, double> point4(theSecondCorner.first, theFirstCorner.second);

    NFmiSvgPath::Element element1(NFmiSvgPath::kElementMoveto, point1.first, point1.second);
    NFmiSvgPath::Element element2(NFmiSvgPath::kElementLineto, point2.first, point2.second);
    NFmiSvgPath::Element element3(NFmiSvgPath::kElementLineto, point3.first, point3.second);
    NFmiSvgPath::Element element4(NFmiSvgPath::kElementLineto, point4.first, point4.second);
    NFmiSvgPath::Element element5(NFmiSvgPath::kElementClosePath, point1.first, point1.second);
    thePath.push_back(element1);
    thePath.push_back(element2);
    thePath.push_back(element3);
    thePath.push_back(element4);
    thePath.push_back(element5);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

std::string get_name_base(const std::string& theName)
{
  try
  {
    std::string place = theName;

    // remove radius if exists
    if (place.find(':') != std::string::npos)
      place = place.substr(0, place.find(':'));

    return place;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void get_svg_path(const Spine::TaggedLocation& tloc,
                  Spine::PostGISDataSource& postGISDataSource,
                  NFmiSvgPath& svgPath)
{
  try
  {
    Spine::LocationPtr loc = tloc.loc;
    std::string place = get_name_base(loc->name);

    if (loc->type == Spine::Location::Place || loc->type == Spine::Location::CoordinatePoint)
    {
      std::pair<double, double> thePoint = make_pair(loc->longitude, loc->latitude);
      make_point_path(svgPath, thePoint);
    }
    else if (loc->type == Spine::Location::Area)
    {
      if (postGISDataSource.isPolygon(place))
      {
        stringstream svg_string_stream(postGISDataSource.getSVGPath(place));
        svgPath.Read(svg_string_stream);
      }
      else if (postGISDataSource.isPoint(place))
      {
        std::pair<double, double> thePoint(postGISDataSource.getPoint(place).first,
                                           postGISDataSource.getPoint(place).second);
        make_point_path(svgPath, thePoint);
      }
      else
      {
        throw Spine::Exception(BCP, "Area '" + place + "' not found in PostGIS database!");
      }
    }
    else if (loc->type == Spine::Location::Path)
    {
      if (place.find(',') != std::string::npos)
      {
        // path given as a query parameter in format "lon,lat,lon,lat,lon,lat,..."
        std::vector<string> lonlat_vector;
        boost::algorithm::split(lonlat_vector, place, boost::algorithm::is_any_of(","));
        for (unsigned int i = 0; i < lonlat_vector.size(); i += 2)
        {
          double longitude = Fmi::stod(lonlat_vector[i]);
          double latitude = Fmi::stod(lonlat_vector[i + 1]);
          if (svgPath.size() == 0)
            svgPath.push_back(
                NFmiSvgPath::Element(NFmiSvgPath::kElementMoveto, longitude, latitude));
          else
            svgPath.push_back(
                NFmiSvgPath::Element(NFmiSvgPath::kElementLineto, longitude, latitude));
        }
      }
      else
      {
        // path fetched from PostGIS database
        if (postGISDataSource.isPolygon(place) || postGISDataSource.isLine(place))
        {
          stringstream svg_string_stream(postGISDataSource.getSVGPath(place));
          svgPath.Read(svg_string_stream);
        }
        else if (postGISDataSource.isPoint(place))
        {
          std::pair<double, double> thePoint(postGISDataSource.getPoint(place).first,
                                             postGISDataSource.getPoint(place).second);
          make_point_path(svgPath, thePoint);
        }
        else
        {
          throw Spine::Exception(BCP, "Path '" + place + "' not found in PostGIS database!");
        }
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

Spine::LocationList get_location_list(const NFmiSvgPath& thePath,
                                      const std::string& thePathName,
                                      const double& stepInKm,
                                      Engine::Geonames::Engine& geonames)
{
  try
  {
    Spine::LocationList locationList;

    std::pair<double, double> from(thePath.begin()->itsX, thePath.begin()->itsY);
    std::pair<double, double> to(thePath.begin()->itsX, thePath.begin()->itsY);

    double leftoverDistanceKmFromPreviousLeg = 0.0;
    std::string theTimezone = "";

    for (NFmiSvgPath::const_iterator it = thePath.begin(); it != thePath.end(); ++it)
    {
      to = std::pair<double, double>(it->itsX, it->itsY);

      // first round
      if (it == thePath.begin())
      {
        // fetch geoinfo only for the first coordinate, because geosearch is so slow
        // reuse name and timezone for rest of the locations
        Spine::LocationPtr locFirst = geonames.lonlatSearch(it->itsX, it->itsY, "");

        Spine::LocationPtr loc = Spine::LocationPtr(new Spine::Location(locFirst->geoid,
                                                                        thePathName,
                                                                        locFirst->iso2,
                                                                        locFirst->municipality,
                                                                        locFirst->area,
                                                                        locFirst->feature,
                                                                        locFirst->country,
                                                                        locFirst->longitude,
                                                                        locFirst->latitude,
                                                                        locFirst->timezone,
                                                                        locFirst->population,
                                                                        locFirst->elevation));

        theTimezone = loc->timezone;
        locationList.push_back(loc);
      }
      else
      {
        double intermediateDistance = distance_in_kilometers(from, to);
        if ((intermediateDistance + leftoverDistanceKmFromPreviousLeg) <= stepInKm)
        {
          leftoverDistanceKmFromPreviousLeg += intermediateDistance;
        }
        else
        {
          // missing distance from step
          double missingDistance = stepInKm - leftoverDistanceKmFromPreviousLeg;
          std::pair<double, double> intermediatePoint =
              destination_point(from, to, missingDistance);
          locationList.push_back(Spine::LocationPtr(new Spine::Location(
              intermediatePoint.first, intermediatePoint.second, thePathName, theTimezone)));
          from = intermediatePoint;
          while (distance_in_kilometers(from, to) > stepInKm)
          {
            intermediatePoint = destination_point(from, to, stepInKm);
            locationList.push_back(Spine::LocationPtr(new Spine::Location(
                intermediatePoint.first, intermediatePoint.second, thePathName, theTimezone)));
            from = intermediatePoint;
          }
          leftoverDistanceKmFromPreviousLeg = distance_in_kilometers(from, to);
        }
      }
      from = to;
    }

    // last leg is not full
    if (leftoverDistanceKmFromPreviousLeg > 0.001)
    {
      locationList.push_back(
          Spine::LocationPtr(new Spine::Location(to.first, to.second, thePathName, theTimezone)));
    }

    Spine::LocationPtr front = locationList.front();
    Spine::LocationPtr back = locationList.back();

    if (locationList.size() > 1 && front->longitude == back->longitude &&
        front->latitude == back->latitude)
      locationList.pop_back();

    return locationList;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void erase_redundant_timesteps(ts::TimeSeries& ts,
                               const Spine::TimeSeriesGenerator::LocalTimeList& timesteps)
{
  try
  {
    ts::TimeSeries no_redundant;
    no_redundant.reserve(ts.size());
    std::set<local_date_time> timestep_set(timesteps.begin(), timesteps.end());

    for (auto it = ts.begin(); it != ts.end(); ++it)
    {
      if (timestep_set.find(it->time) != timestep_set.end())
      {
        no_redundant.push_back(*it);
      }
    }

    ts = no_redundant;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

ts::TimeSeriesPtr erase_redundant_timesteps(
    ts::TimeSeriesPtr ts, const Spine::TimeSeriesGenerator::LocalTimeList& timesteps)
{
  try
  {
    erase_redundant_timesteps(*ts, timesteps);
    return ts;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

ts::TimeSeriesVectorPtr erase_redundant_timesteps(
    ts::TimeSeriesVectorPtr tsv, const Spine::TimeSeriesGenerator::LocalTimeList& timesteps)
{
  try
  {
    for (unsigned int i = 0; i < tsv->size(); i++)
      erase_redundant_timesteps(tsv->at(i), timesteps);

    return tsv;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

ts::TimeSeriesGroupPtr erase_redundant_timesteps(
    ts::TimeSeriesGroupPtr tsg, const Spine::TimeSeriesGenerator::LocalTimeList& timesteps)
{
  try
  {
    for (size_t i = 0; i < tsg->size(); i++)
      erase_redundant_timesteps(tsg->at(i).timeseries, timesteps);
    return tsg;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

template <typename T>
T aggregate(const T& raw_data, const Spine::ParameterFunctions& pf)
{
  try
  {
#ifdef MYDEBUG
    std::cout << "data before aggregation :" << std::endl;
    std::cout << *raw_data << std::endl;
#endif

    T aggregated_data = ts::aggregate(*raw_data, pf);

#ifdef MYDEBUG
    std::cout << "aggregated data : " << std::endl;
    std::cout << *aggregated_data << std::endl;
#endif
    return aggregated_data;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void update_latest_timestep(Query& query, const ts::TimeSeriesVectorPtr& tsv)
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void update_latest_timestep(Query& query, const ts::TimeSeries& ts)
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void update_latest_timestep(Query& query, const ts::TimeSeriesGroup& tsg)
{
  try
  {
    // take first time series and last timestep thereof
    ts::TimeSeries ts = tsg[0].timeseries;

    if (ts.empty())
      return;

    // update the latest timestep, so that next query (is exists) knows from where to continue
    update_latest_timestep(query, ts);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void store_data(ts::TimeSeriesVectorPtr aggregatedData, Query& query, OutputData& outputData)
{
  try
  {
    if (aggregatedData->size() == 0)
      return;

    // insert data to the end
    std::vector<TimeSeriesData>& odata = (--outputData.end())->second;
    odata.push_back(TimeSeriesData(aggregatedData));
    update_latest_timestep(query, aggregatedData);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    if (aggregatedData.size() == 0)
      return;

    TimeSeriesData tsdata;
    if (boost::get<ts::TimeSeriesPtr>(&aggregatedData[0]))
    {
      ts::TimeSeriesPtr ts_result(new ts::TimeSeries);
      // first merge timeseries of all levels of one parameter
      for (unsigned int i = 0; i < aggregatedData.size(); i++)
      {
        ts::TimeSeriesPtr ts = *(boost::get<ts::TimeSeriesPtr>(&aggregatedData[i]));
        ts_result->insert(ts_result->end(), ts->begin(), ts->end());
      }
      // update the latest timestep, so that next query (if exists) knows from where to continue
      update_latest_timestep(query, *ts_result);
      tsdata = ts_result;
    }
    else if (boost::get<ts::TimeSeriesGroupPtr>(&aggregatedData[0]))
    {
      ts::TimeSeriesGroupPtr tsg_result(new ts::TimeSeriesGroup);
      // first merge timeseries of all levels of one parameter
      for (unsigned int i = 0; i < aggregatedData.size(); i++)
      {
        ts::TimeSeriesGroupPtr tsg = *(boost::get<ts::TimeSeriesGroupPtr>(&aggregatedData[i]));
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void add_data_to_table(const Spine::OptionParsers::ParameterList& paramlist,
                       ts::TableFeeder& tf,
                       OutputData& outputData,
                       const std::string& location_name,
                       int& startRow)
{
  try
  {
    unsigned int numberOfParameters = paramlist.size();
    // iterate different locations
    for (unsigned int i = 0; i < outputData.size(); i++)
    {
      std::string locationName = outputData[i].first;
      if (locationName != location_name)
        continue;

      startRow = tf.getCurrentRow();

      std::vector<TimeSeriesData>& outdata = outputData[i].second;
      // iterate columns (parameters)
      for (unsigned int j = 0; j < outdata.size(); j++)
      {
        TimeSeriesData tsdata = outdata[j];
        tf.setCurrentRow(startRow);
        tf.setCurrentColumn(j);

        std::string paramName = paramlist[j % numberOfParameters].name();
        if (paramName == LATLON_PARAM || paramName == NEARLATLON_PARAM)
        {
          tf << Spine::TimeSeries::LonLatFormat::LATLON;
        }
        else if (paramName == LONLAT_PARAM || paramName == NEARLONLAT_PARAM)
        {
          tf << Spine::TimeSeries::LonLatFormat::LONLAT;
        }

        if (boost::get<ts::TimeSeriesPtr>(&tsdata))
        {
          ts::TimeSeriesPtr ts = *(boost::get<ts::TimeSeriesPtr>(&tsdata));
          tf << *ts;
        }
        else if (boost::get<ts::TimeSeriesVectorPtr>(&tsdata))
        {
          ts::TimeSeriesVectorPtr tsv = *(boost::get<ts::TimeSeriesVectorPtr>(&tsdata));
          for (unsigned int k = 0; k < tsv->size(); k++)
          {
            tf.setCurrentColumn(0 + k);  //<-- WTF??
            tf.setCurrentRow(startRow);
            tf << tsv->at(k);
          }
          startRow = tf.getCurrentRow();
        }
        else if (boost::get<ts::TimeSeriesGroupPtr>(&tsdata))
        {
          ts::TimeSeriesGroupPtr tsg = *(boost::get<ts::TimeSeriesGroupPtr>(&tsdata));
          tf << *tsg;
        }

        // Reset formatting to the default value
        tf << Spine::TimeSeries::LonLatFormat::LONLAT;
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

std::string get_location_id(Spine::LocationPtr loc)
{
  try
  {
    std::ostringstream ss;

    ss << loc->name << " " << fixed << setprecision(7) << loc->longitude << " " << loc->latitude;

    return ss.str();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

// fills the table with data
void fill_table(Query& query, OutputData& outputData, Spine::Table& table)
{
  try
  {
    ts::TableFeeder tf(table, query.valueformatter, query.precisions);
    int startRow = tf.getCurrentRow();

    if (outputData.size() == 0)
      return;

    std::string locationName(outputData[0].first);

    // if observations exists they are first in the result set
    if (locationName == "_obs_")
      add_data_to_table(query.poptions.parameters(), tf, outputData, "_obs_", startRow);

    // iterate locations
    BOOST_FOREACH (const auto& tloc, query.loptions->locations())
    {
      std::string locationId = get_location_id(tloc.loc);

      add_data_to_table(query.poptions.parameters(), tf, outputData, locationId, startRow);
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

bool is_location_parameter(const std::string& paramname)
{
  try
  {
    return (paramname == LEVEL_PARAM || paramname == LATITUDE_PARAM ||
            paramname == LONGITUDE_PARAM || paramname == LAT_PARAM || paramname == LON_PARAM ||
            paramname == LATLON_PARAM || paramname == LONLAT_PARAM || paramname == GEOID_PARAM ||
            // paramname == PLACE_PARAM||
            paramname == FEATURE_PARAM ||
            paramname == LOCALTZ_PARAM || paramname == NAME_PARAM || paramname == ISO2_PARAM ||
            paramname == REGION_PARAM || paramname == COUNTRY_PARAM ||
            paramname == ELEVATION_PARAM || paramname == POPULATION_PARAM ||
            paramname == STATION_ELEVATION_PARAM);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

std::string location_parameter(const Spine::LocationPtr loc,
                               const std::string paramName,
                               const Spine::ValueFormatter& valueformatter,
                               const std::string& timezone,
                               int precision)
{
  try
  {
    if (!loc)
      return valueformatter.missing();
    else if (paramName == NAME_PARAM)
      return loc->name;
    else if (paramName == STATIONNAME_PARAM)
      return loc->name;
    else if (paramName == ISO2_PARAM)
      return loc->iso2;
    else if (paramName == GEOID_PARAM)
    {
      if (loc->geoid == 0)
        return valueformatter.missing();
      else
        return Fmi::to_string(loc->geoid);
    }
    else if (paramName == REGION_PARAM)
    {
      if (loc->area.empty())
      {
        if (loc->name.empty())
        {
          // No area (administrative region) nor name known.
          return valueformatter.missing();
        }
        else
        {
          // Place name known, administrative region unknown.
          return loc->name;
        }
      }
      else
      {
        // Administrative region known.
        return loc->area;
      }
    }
    else if (paramName == COUNTRY_PARAM)
      return loc->country;
    else if (paramName == FEATURE_PARAM)
      return loc->feature;
    else if (paramName == LOCALTZ_PARAM)
      return loc->timezone;
    else if (paramName == TZ_PARAM)
      return (timezone == "localtime" ? loc->timezone : timezone);
    else if (paramName == LATITUDE_PARAM || paramName == LAT_PARAM)
      return Fmi::to_string(loc->latitude);
    else if (paramName == LONGITUDE_PARAM || paramName == LON_PARAM)
      return valueformatter.format(loc->longitude, precision);
    else if (paramName == LATLON_PARAM)
      return (valueformatter.format(loc->latitude, precision) + ", " +
              valueformatter.format(loc->longitude, precision));
    else if (paramName == LONLAT_PARAM)
      return (valueformatter.format(loc->longitude, precision) + ", " +
              valueformatter.format(loc->latitude, precision));
    else if (paramName == POPULATION_PARAM)
      return Fmi::to_string(loc->population);
    else if (paramName == ELEVATION_PARAM || paramName == STATION_ELEVATION_PARAM)
      return valueformatter.format(loc->elevation, precision);

    throw Spine::Exception(BCP, "Unknown location parameter: '" + paramName + "'");
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

bool is_time_parameter(const std::string& paramname)
{
  try
  {
    return (
        paramname == TIME_PARAM || paramname == ISOTIME_PARAM || paramname == XMLTIME_PARAM ||
        paramname == ORIGINTIME_PARAM || paramname == LOCALTIME_PARAM ||
        paramname == UTCTIME_PARAM || paramname == EPOCHTIME_PARAM ||
        paramname == SUNELEVATION_PARAM || paramname == SUNDECLINATION_PARAM ||
        paramname == SUNATZIMUTH_PARAM || paramname == DARK_PARAM || paramname == MOONPHASE_PARAM ||
        paramname == MOONRISE_PARAM || paramname == MOONRISE2_PARAM || paramname == MOONSET_PARAM ||
        paramname == MOONSET2_PARAM || paramname == MOONRISETODAY_PARAM ||
        paramname == MOONRISE2TODAY_PARAM || paramname == MOONSETTODAY_PARAM ||
        paramname == MOONSET2TODAY_PARAM || paramname == MOONUP24H_PARAM ||
        paramname == MOONDOWN24H_PARAM || paramname == SUNRISE_PARAM || paramname == SUNSET_PARAM ||
        paramname == NOON_PARAM || paramname == SUNRISETODAY_PARAM ||
        paramname == SUNSETTODAY_PARAM || paramname == DAYLENGTH_PARAM ||
        paramname == TIMESTRING_PARAM || paramname == WDAY_PARAM || paramname == WEEKDAY_PARAM ||
        paramname == MON_PARAM || paramname == MONTH_PARAM || paramname == HOUR_PARAM ||
        paramname == TZ_PARAM || paramname == ORIGINTIME_PARAM ||
        (paramname.substr(0, 5) == "date(" && paramname[paramname.size() - 1] == ')'));
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Time formatter
 */
// ----------------------------------------------------------------------

std::string format_date(const boost::local_time::local_date_time& ldt,
                        const std::locale& llocale,
                        const std::string& fmt)
{
  try
  {
    typedef boost::date_time::time_facet<boost::local_time::local_date_time, char> tfacet;
    std::ostringstream os;
    os.imbue(std::locale(llocale, new tfacet(fmt.c_str())));
    os << ldt;
    return Fmi::latin1_to_utf8(os.str());
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

std::string time_parameter(const std::string paramname,
                           const boost::local_time::local_date_time& ldt,
                           const boost::posix_time::ptime now,
                           const Spine::Location& loc,
                           const std::string& timezone,
                           const Fmi::TimeZones& timezones,
                           const std::locale& outlocale,
                           const Fmi::TimeFormatter& timeformatter,
                           const std::string& timestring)
{
  try
  {
    std::string retval;

    if (paramname == TIME_PARAM)
    {
      boost::local_time::time_zone_ptr tz = timezones.time_zone_from_string(timezone);
      retval = timeformatter.format(local_date_time(ldt.utc_time(), tz));
    }

    if (paramname == ORIGINTIME_PARAM)
    {
      boost::local_time::time_zone_ptr tz = timezones.time_zone_from_string(timezone);
      local_date_time ldt_now(now, tz);
      retval = timeformatter.format(ldt_now);
    }

    else if (paramname == ISOTIME_PARAM)
      retval = Fmi::to_iso_string(ldt.local_time());

    else if (paramname == XMLTIME_PARAM)
      retval = Fmi::to_iso_extended_string(ldt.local_time());

    else if (paramname == LOCALTIME_PARAM)
    {
      boost::local_time::time_zone_ptr localtz = timezones.time_zone_from_string(loc.timezone);

      boost::posix_time::ptime utc = ldt.utc_time();
      boost::local_time::local_date_time localt(utc, localtz);
      retval = timeformatter.format(localt);
    }

    else if (paramname == UTCTIME_PARAM)
      retval = timeformatter.format(ldt.utc_time());

    else if (paramname == EPOCHTIME_PARAM)
    {
      boost::posix_time::ptime time_t_epoch(boost::gregorian::date(1970, 1, 1));
      boost::posix_time::time_duration diff = ldt.utc_time() - time_t_epoch;
      retval = Fmi::to_string(diff.total_seconds());
    }

    else if (paramname == ORIGINTIME_PARAM)
    {
      retval = timeformatter.format(now);
    }

    else if (paramname == TZ_PARAM)
    {
      retval = timezone;
    }

    else if (paramname == SUNELEVATION_PARAM)
    {
      Fmi::Astronomy::solar_position_t sp =
          Fmi::Astronomy::solar_position(ldt, loc.longitude, loc.latitude);
      retval = sp.elevation;
    }

    else if (paramname == SUNDECLINATION_PARAM)
    {
      Fmi::Astronomy::solar_position_t sp =
          Fmi::Astronomy::solar_position(ldt, loc.longitude, loc.latitude);
      retval = sp.declination;
    }

    else if (paramname == SUNATZIMUTH_PARAM)
    {
      Fmi::Astronomy::solar_position_t sp =
          Fmi::Astronomy::solar_position(ldt, loc.longitude, loc.latitude);
      retval = sp.azimuth;
    }

    else if (paramname == DARK_PARAM)
    {
      Fmi::Astronomy::solar_position_t sp =
          Fmi::Astronomy::solar_position(ldt, loc.longitude, loc.latitude);
      retval = Fmi::to_string(sp.dark());
    }

    else if (paramname == MOONPHASE_PARAM)
      retval = Fmi::Astronomy::moonphase(ldt.utc_time());

    else if (paramname == MOONRISE_PARAM)
    {
      Fmi::Astronomy::lunar_time_t lt =
          Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
      retval = Fmi::to_iso_string(lt.moonrise.local_time());
    }
    else if (paramname == MOONRISE2_PARAM)
    {
      Fmi::Astronomy::lunar_time_t lt =
          Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);

      if (lt.moonrise2_today())
        retval = Fmi::to_iso_string(lt.moonrise2.local_time());
      else
        retval = std::string("");
    }
    else if (paramname == MOONSET_PARAM)
    {
      Fmi::Astronomy::lunar_time_t lt =
          Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
      retval = Fmi::to_iso_string(lt.moonset.local_time());
    }
    else if (paramname == MOONSET2_PARAM)
    {
      Fmi::Astronomy::lunar_time_t lt =
          Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);

      if (lt.moonset2_today())
      {
        retval = Fmi::to_iso_string(lt.moonset2.local_time());
      }
      else
      {
        retval = std::string("");
      }
    }
    else if (paramname == MOONRISETODAY_PARAM)
    {
      Fmi::Astronomy::lunar_time_t lt =
          Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);

      retval = Fmi::to_string(lt.moonrise_today());
    }
    else if (paramname == MOONRISE2TODAY_PARAM)
    {
      Fmi::Astronomy::lunar_time_t lt =
          Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
      retval = Fmi::to_string(lt.moonrise2_today());
    }
    else if (paramname == MOONSETTODAY_PARAM)
    {
      Fmi::Astronomy::lunar_time_t lt =
          Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
      retval = Fmi::to_string(lt.moonset_today());
    }
    else if (paramname == MOONSET2TODAY_PARAM)
    {
      Fmi::Astronomy::lunar_time_t lt =
          Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
      retval = Fmi::to_string(lt.moonset2_today());
    }
    else if (paramname == MOONUP24H_PARAM)
    {
      Fmi::Astronomy::lunar_time_t lt =
          Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
      retval = Fmi::to_string(lt.above_horizont_24h());
    }
    else if (paramname == MOONDOWN24H_PARAM)
    {
      Fmi::Astronomy::lunar_time_t lt =
          Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
      retval =
          Fmi::to_string(!lt.moonrise_today() && !lt.moonset_today() && !lt.above_horizont_24h());
    }
    else if (paramname == SUNRISE_PARAM)
    {
      Fmi::Astronomy::solar_time_t st =
          Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
      retval = Fmi::to_iso_string(st.sunrise.local_time());
    }
    else if (paramname == SUNSET_PARAM)
    {
      Fmi::Astronomy::solar_time_t st =
          Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
      retval = Fmi::to_iso_string(st.sunset.local_time());
    }
    else if (paramname == NOON_PARAM)
    {
      Fmi::Astronomy::solar_time_t st =
          Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
      retval = Fmi::to_iso_string(st.noon.local_time());
    }
    else if (paramname == SUNRISETODAY_PARAM)
    {
      Fmi::Astronomy::solar_time_t st =
          Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
      retval = Fmi::to_string(st.sunrise_today());
    }
    else if (paramname == SUNSETTODAY_PARAM)
    {
      Fmi::Astronomy::solar_time_t st =
          Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
      retval = Fmi::to_string(st.sunset_today());
    }
    else if (paramname == DAYLENGTH_PARAM)
    {
      Fmi::Astronomy::solar_time_t st =
          Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
      auto seconds = st.daylength().total_seconds();
      auto minutes = boost::numeric_cast<long>(round(seconds / 60.0));
      retval = Fmi::to_string(minutes);
    }
    else if (paramname == TIMESTRING_PARAM)
      retval = format_date(ldt, outlocale, timestring.c_str());

    else if (paramname == WDAY_PARAM)
      retval = format_date(ldt, outlocale, "%a");

    else if (paramname == WEEKDAY_PARAM)
      retval = format_date(ldt, outlocale, "%A");

    else if (paramname == MON_PARAM)
      retval = format_date(ldt, outlocale, "%b");

    else if (paramname == MONTH_PARAM)
      retval = format_date(ldt, outlocale, "%B");

    else if (paramname == HOUR_PARAM)
      retval = Fmi::to_string(ldt.local_time().time_of_day().hours());
    else if (paramname.substr(0, 5) == "date(" && paramname[paramname.size() - 1] == ')')
      retval = format_date(ldt, outlocale, paramname.substr(5, paramname.size() - 6));

    return retval;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION

std::vector<int> getGeoids(Engine::Observation::Engine* observation,
                           const std::string& producer,
                           const std::string& wktstring)
{
  try
  {
    std::vector<int> geoids;

    Spine::Stations stations;
    Engine::Observation::Settings mysettings;
    mysettings.stationtype = producer;

    stations = observation->getStationsByArea(mysettings, wktstring);
    for (unsigned int i = 0; i < stations.size(); i++)
      geoids.push_back(stations[i].geoid);

    return geoids;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
#endif

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

Spine::LocationPtr getLocation(const Engine::Geonames::Engine* geonames,
                               const int id,
                               const std::string& idtype,
                               const std::string& language)
{
  try
  {
    if (idtype == GEOID_PARAM)
      return geonames->idSearch(id, language);

    Locus::QueryOptions opts;
    opts.SetCountries("all");
    opts.SetSearchVariants(true);
    opts.SetLanguage(idtype);
    opts.SetResultLimit(1);
    opts.SetFeatures({"SYNOP", "FINAVIA", "STUK"});

    Spine::LocationList ll = geonames->nameSearch(opts, Fmi::to_string(id));

    Spine::LocationPtr loc;

    // lets just take the first one
    if (ll.size() > 0)
      loc = geonames->idSearch((*ll.begin())->geoid, language);

    return loc;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION

int get_fmisid_index(const Engine::Observation::Settings& settings)
{
  try
  {
    int fmisid_index(-1);
    // find out fmisid column
    for (unsigned int i = 0; i < settings.parameters.size(); i++)
      if (settings.parameters[i].name() == FMISID_PARAM)
      {
        fmisid_index = i;
        break;
      }

    return fmisid_index;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

#endif

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION

int get_fmisid_value(const ts::Value& value)
{
  try
  {
    int fmisid = -1;

    // fmisid can be std::string or double
    if (boost::get<std::string>(&value))
    {
      std::string fmisidstr = boost::get<std::string>(value);
      boost::algorithm::trim(fmisidstr);
      if (!fmisidstr.empty())
        fmisid = std::stod(boost::get<std::string>(value));
    }
    else if (boost::get<double>(&value))
      fmisid = boost::get<double>(value);

    return fmisid;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

#endif

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION

TimeSeriesByLocation get_timeseries_by_fmisid(const std::string& producer,
                                              const ts::TimeSeriesVectorPtr observation_result,
                                              const Engine::Observation::Settings& settings,
                                              const Query& query,
                                              int fmisid_index)

{
  try
  {
    TimeSeriesByLocation ret;

    if (producer == FLASH_PRODUCER)
    {
      ret.push_back(make_pair(0, observation_result));
      return ret;
    }

    // find fmisid time series
    const ts::TimeSeries& fmisid_ts = observation_result->at(fmisid_index);

    // find indexes for locations
    std::vector<std::pair<unsigned int, unsigned int> > location_indexes;

    unsigned int start_index = 0;
    unsigned int end_index = 0;
    for (unsigned int i = 1; i < fmisid_ts.size(); i++)
    {
      if (fmisid_ts[i].value == fmisid_ts[i - 1].value)
      {
        continue;
      }
      else
      {
        end_index = i;
        location_indexes.push_back(std::pair<unsigned int, unsigned int>(start_index, end_index));
        start_index = end_index = i;
      }
    }
    end_index = fmisid_ts.size();
    location_indexes.push_back(std::pair<unsigned int, unsigned int>(start_index, end_index));

    // iterate through locations and do aggregation
    for (unsigned int i = 0; i < location_indexes.size(); i++)
    {
      ts::TimeSeriesVectorPtr tsv(new ts::TimeSeriesVector());
      ts::TimeSeries ts;
      start_index = location_indexes[i].first;
      end_index = location_indexes[i].second;
      for (unsigned int k = 0; k < observation_result->size(); k++)
      {
        const ts::TimeSeries ts_k = observation_result->at(k);
        if (ts_k.empty())
          tsv->push_back(ts_k);
        else
        {
          ts::TimeSeries ts_ik(ts_k.begin() + start_index, ts_k.begin() + end_index);
          tsv->push_back(ts_ik);
        }
      }

      if (fmisid_ts.empty())
        continue;
      int fmisid = get_fmisid_value(fmisid_ts[start_index].value);
      ret.push_back(make_pair(fmisid, tsv));
    }

    return ret;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

#endif

}  // anonymous

// ----------------------------------------------------------------------
/*!
 * \brief Calculate a hash value for the query
 *
 * A zero value implies the product cannot be cached, for example because
 * it uses observations.
 *
 * The hash value includes all query options, the querydata selected
 * for the locations, plus the time series generated for the locations.
 *
 * Note that it is much easier to generate the hash value from the query
 * string instead of the values generated by the parser from it (Query-object).
 * It is wishful thinking we could cache things better that way. Simply consider
 * these queries
 *
 *     places=Helsinki,Vantaa
 *     places=Vantaa,Helsinki
 *
 * Also note that the user should not generate the time options like this:
 *
 *     $query .= "&starttime=" . time();
 *
 * Instead one should omit the starttime option altogether, or use this instead:
 *
 *     $query .= "&starttime=0"
 *
 * Since we actually generate the time series to verify the the times
 * are still the same, using starttime=0 is safe as far as caching goes.
 * Same goes for starttime=data and the other variations. However, using a
 * varying starttime based on the wall clock effectively disables caching.
 *
 * Note: We do not need to generate the time series for all locations.
 *       It is sufficient to generate it once for each querydata / timezone
 *       combination.
 */
// ----------------------------------------------------------------------

std::size_t Plugin::hash_value(const State& state,
                               Query masterquery,
                               const Spine::HTTP::Request& request)
{
  try
  {
    // Initial hash value = geonames hash (may change during reloads) + querystring hash

    auto hash = state.getGeoEngine().hash_value();

    // Calculate a hash for the query. We can ignore the time series options
    // since we later on generate a hash for the generated time series.
    // In particular this permits us to ignore the endtime=x setting, which
    // Ilmanet sets to a precision of one second.

    // Using all options would be done like this:
    // boost::hash_combine( hash, boost::hash_value(request.getParameterMap()) );

    for (const auto& name_value : request.getParameterMap())
    {
      const auto& name = name_value.first;
      if (name != "hour" && name != "time" && name != "timestep" && name != "timesteps" &&
          name != "starttime" && name != "startstep" && name != "endtime")
      {
        boost::hash_combine(hash, boost::hash_value(name_value.second));
      }
    }

    // If the query depends on locations only, that's it!

    if (is_plain_location_query(masterquery.poptions.parameters()))
      return hash;

#ifndef WITHOUT_OBSERVATION

    // Check here first if any of the producers is an observation.
    // If so, return zero

    BOOST_FOREACH (const AreaProducers& areaproducers, masterquery.timeproducers)
    {
      BOOST_FOREACH (const auto& areaproducer, areaproducers)
      {
        if (isObsProducer(areaproducer))
          return 0;
      }
    }
#endif

    // Maintain a list of handled querydata/timezone combinations

    std::set<std::size_t> handled_timeseries;

    // Basically we mimic what is done in processQuery as far as is needed

    ProducerDataPeriod producerDataPeriod;

// producerDataPeriod contains information of data periods of different producers

#ifndef WITHOUT_OBSERVATION
    producerDataPeriod.init(state, *itsQEngine, itsObsEngine, masterquery.timeproducers);
#else
    producerDataPeriod.init(state, *itsQEngine, masterquery.timeproducers);
#endif

    // the result data is stored here during the query

    bool producerMissing = (masterquery.timeproducers.empty());

    if (producerMissing)
    {
      masterquery.timeproducers.push_back(AreaProducers());
    }

#ifndef WITHOUT_OBSERVATION
    ObsParameters obsParameters = getObsParameters(masterquery);
#endif

    boost::posix_time::ptime latestTimestep = masterquery.latestTimestep;

    bool startTimeUTC = masterquery.toptions.startTimeUTC;

    // This loop will iterate through the producers.

    std::size_t producer_group = 0;
    bool firstProducer = true;

    BOOST_FOREACH (const AreaProducers& areaproducers, masterquery.timeproducers)
    {
      Query query = masterquery;

      query.timeproducers.clear();  // not used
      // set latestTimestep for the query
      query.latestTimestep = latestTimestep;

      if (producer_group != 0)
        query.toptions.startTimeUTC = startTimeUTC;
      query.toptions.endTimeUTC = masterquery.toptions.endTimeUTC;

#ifndef WITHOUT_OBSERVATION
      if (!areaproducers.empty() && !itsConfig.obsEngineDisabled() &&
          isObsProducer(areaproducers.front()))
      {
        // Cannot cache observations! Safety check only, this has already been checked at the start
        return 0;
      }
      else
#endif
      {
        // Here we emulate processQEngineQuery
        // Note name changes: masterquery --> query, and query-->subquery

        // first timestep is here in utc
        boost::posix_time::ptime first_timestep = query.latestTimestep;

        BOOST_FOREACH (const auto& tloc, query.loptions->locations())
        {
          Query subquery = query;
          QueryLevelDataCache queryLevelDataCache;

          if (subquery.timezone == LOCALTIME_PARAM)
            subquery.timezone = tloc.loc->timezone;

          subquery.toptions.startTime = first_timestep;

          if (!firstProducer)
            subquery.toptions.startTime += minutes(1);  // WHY???????
          firstProducer = false;

          // producer can be alias, get actual producer
          std::string producer(select_producer(*itsQEngine, *(tloc.loc), subquery, areaproducers));
          bool isClimatologyProducer =
              (producer.empty() ? false : itsQEngine->getProducerConfig(producer).isclimatology);

          boost::local_time::local_date_time data_period_endtime(
              producerDataPeriod.getLocalEndTime(producer, subquery.timezone, getTimeZones()));

          // We do not need to iterate over the parameters here like processQEngineQuery does

          // every parameter starts from the same row
          if (subquery.toptions.endTime > data_period_endtime.local_time() &&
              !data_period_endtime.is_not_a_date_time() && !isClimatologyProducer)
          {
            subquery.toptions.endTime = data_period_endtime.local_time();
          }

          {
            // Emulate fetchQEngineValues here

            Spine::LocationPtr loc = tloc.loc;
            std::string place = get_name_base(loc->name);
            NFmiSvgPath svgPath;
            if (loc->type == Spine::Location::Path || loc->type == Spine::Location::Area)
            {
              get_svg_path(tloc, itsPostGISDataSource, svgPath);
              loc = getLocationForArea(tloc, query, svgPath, itsPostGISDataSource);
            }
            else if (loc->type == Spine::Location::BoundingBox)
            {
              // find geoinfo for the corner coordinate
              vector<string> parts;
              boost::algorithm::split(parts, place, boost::algorithm::is_any_of(","));

              double lon1 = Fmi::stod(parts[0]);
              double lat1 = Fmi::stod(parts[1]);
              double lon2 = Fmi::stod(parts[2]);
              double lat2 = Fmi::stod(parts[3]);

              // get location info for center coordinate
              std::pair<double, double> lonlatCenter((lon1 + lon2) / 2.0, (lat1 + lat2) / 2.0);
              Spine::LocationPtr locCenter = itsGeoEngine->lonlatSearch(
                  lonlatCenter.first, lonlatCenter.second, subquery.language);

              std::unique_ptr<Spine::Location> tmp(new Spine::Location(locCenter->geoid,
                                                                       tloc.tag,
                                                                       locCenter->iso2,
                                                                       locCenter->municipality,
                                                                       locCenter->area,
                                                                       locCenter->feature,
                                                                       locCenter->country,
                                                                       locCenter->longitude,
                                                                       locCenter->latitude,
                                                                       locCenter->timezone,
                                                                       locCenter->population,
                                                                       locCenter->elevation));
              tmp->type = tloc.loc->type;
              tmp->radius = tloc.loc->radius;
              loc.reset(tmp.release());
            }

            if (subquery.timezone == LOCALTIME_PARAM)
              subquery.timezone = loc->timezone;

            // Select the producer for the coordinate
            auto producer = select_producer(*itsQEngine, *loc, subquery, areaproducers);

            if (producer.empty())
            {
              Spine::Exception ex(BCP, "No data available for '" + loc->name + "'!");
              ex.disableLogging();
              throw ex;
            }

            auto qi = (subquery.origintime ? state.get(producer, *subquery.origintime)
                                           : state.get(producer));

            // Generated timeseries may depend on the available querydata
            auto querydata_hash = Engine::Querydata::hash_value(qi);

            // No need to generate the timeseries again if the combination has already been handled
            // Note that timeseries_hash is used only for caching the generated timeseries,
            // the value should not be used for calculating the actual product hash. Instead,
            // we use the hash of the generated timeseries itself.

            auto timeseries_hash = querydata_hash;
            boost::hash_combine(timeseries_hash, boost::hash_value(subquery.timezone));
            boost::hash_combine(timeseries_hash, subquery.toptions.hash_value());

            if (handled_timeseries.find(timeseries_hash) == handled_timeseries.end())
            {
              handled_timeseries.insert(timeseries_hash);

              const auto validtimes = qi->validTimes();
              if (validtimes->empty())
                throw Spine::Exception(BCP, "Producer '" + producer + "' has no valid timesteps!");
              subquery.toptions.setDataTimes(validtimes, qi->isClimatology());

              // no area operations allowed for non-grid data
              if (!qi->isGrid() && ((loc->type != Spine::Location::Place &&
                                     loc->type != Spine::Location::CoordinatePoint) ||
                                    ((loc->type == Spine::Location::Place ||
                                      loc->type == Spine::Location::CoordinatePoint) &&
                                     loc->radius > 0)))
                return 0;

#ifdef BREAKS_REQUESTS_WHICH_START_BEFORE_QUERYDATA_BY_SHIFTING_DATA
              bool isClimatologyProducer =
                  (producer.empty() ? false
                                    : itsQEngine->getProducerConfig(producer).isclimatology);
              bool isMultiFile =
                  (producer.empty() ? false : itsQEngine->getProducerConfig(producer).ismultifile);

              // timeseries start time can not be earlier than first time in data
              // time series end time can not be later than last time in data

              if (!isClimatologyProducer &&
                  !isMultiFile)  // we don't care about parameter names in here!
              {
                if (subquery.toptions.startTime < validtimes->front())
                {
                  subquery.toptions.startTime = validtimes->front();
                  subquery.toptions.startTimeUTC = true;
                }
                if (subquery.toptions.endTime > validtimes->back())
                {
                  subquery.toptions.endTime = validtimes->back();
                  subquery.toptions.endTimeUTC = true;
                }
              }
#endif

              auto tz = getTimeZones().time_zone_from_string(subquery.timezone);
              auto tlist = itsTimeSeriesCache->generate(subquery.toptions, tz);

              // This is enough to generate an unique hash for the request, even
              // though the timesteps may not be exactly the same as those used
              // in generating the result.

              boost::hash_combine(hash, querydata_hash);
              boost::hash_combine(hash, SmartMet::Plugin::TimeSeries::hash_value(*tlist));
            }
          }

          // get the latest_timestep from previous query
          query.latestTimestep = subquery.latestTimestep;
          query.lastpoint = subquery.lastpoint;
        }
      }

      // get the latestTimestep from previous query
      latestTimestep = query.latestTimestep;

      startTimeUTC = query.toptions.startTimeUTC;
      ++producer_group;
    }

    return hash;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

Spine::LocationPtr Plugin::getLocationForArea(const Spine::TaggedLocation& tloc,
                                              const Query& query,
                                              const NFmiSvgPath& svgPath,
                                              Spine::PostGISDataSource& postGISDataSource) const
{
  try
  {
    // get location info for center coordinate
    double bottom(svgPath.begin()->itsY), top(svgPath.begin()->itsY), left(svgPath.begin()->itsX),
        right(svgPath.begin()->itsX);
    for (NFmiSvgPath::const_iterator it = svgPath.begin(); it != svgPath.end(); ++it)
    {
      if (it->itsX < left)
        left = it->itsX;
      if (it->itsX > right)
        right = it->itsX;
      if (it->itsY < bottom)
        bottom = it->itsY;
      if (it->itsY > top)
        top = it->itsY;
    }
    std::pair<double, double> lonlatCenter((right + left) / 2.0, (top + bottom) / 2.0);
    ;

    Spine::LocationPtr locCenter =
        itsGeoEngine->lonlatSearch(lonlatCenter.first, lonlatCenter.second, query.language);

    // Spine::LocationPtr contains a const Location, so some trickery is used here
    std::unique_ptr<Spine::Location> tmp(new Spine::Location(locCenter->geoid,
                                                             tloc.tag,
                                                             locCenter->iso2,
                                                             locCenter->municipality,
                                                             locCenter->area,
                                                             locCenter->feature,
                                                             locCenter->country,
                                                             locCenter->longitude,
                                                             locCenter->latitude,
                                                             locCenter->timezone,
                                                             locCenter->population,
                                                             locCenter->elevation));
    tmp->type = tloc.loc->type;
    tmp->radius = tloc.loc->radius;

    return Spine::LocationPtr(tmp.release());
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

Spine::TimeSeriesGenerator::LocalTimeList Plugin::generateQEngineQueryTimes(
    const Engine::Querydata::Q& q, const Query& query, const std::string paramname) const
{
  try
  {
    auto tz = getTimeZones().time_zone_from_string(query.timezone);
    auto tlist = itsTimeSeriesCache->generate(query.toptions, tz);

    // time list may be empty for example due to bad query string options
    if (tlist->empty())
      return *tlist;

    if (query.maxAggregationIntervals.find(paramname) == query.maxAggregationIntervals.end())
      return *tlist;

    unsigned int aggregationIntervalBehind = query.maxAggregationIntervals.at(paramname).behind;
    unsigned int aggregationIntervalAhead = query.maxAggregationIntervals.at(paramname).ahead;

    // If no aggregation happens just return tlist
    if (aggregationIntervalBehind == 0 && aggregationIntervalAhead == 0)
      return *tlist;

    // If we are going to do aggregation, we get all timesteps between starttime and endtime from
    // query data file. After aggregation only the requested timesteps are shown to user
    std::set<boost::local_time::local_date_time> qdtimesteps;

    // time frame is extended by aggregation interval
    boost::local_time::local_date_time timeseriesStartTime = *(tlist->begin());
    boost::local_time::local_date_time timeseriesEndTime = *(--tlist->end());
    timeseriesStartTime -= boost::posix_time::minutes(aggregationIntervalBehind);
    timeseriesEndTime += boost::posix_time::minutes(aggregationIntervalAhead);

#ifdef MYDEBUG
    std::cout << "parameter name: " << paramname << std::endl;
    std::cout << "aggregationIntervalBehind: " << aggregationIntervalBehind << std::endl;
    std::cout << "aggregationIntervalAhead: " << aggregationIntervalAhead << std::endl;
    std::cout << "timeseriesStartTime: " << timeseriesStartTime << std::endl;
    std::cout << "timeseriesEndTime: " << timeseriesEndTime << std::endl;
#endif

    Spine::TimeSeriesGeneratorOptions topt = query.toptions;

    topt.startTime = (query.toptions.startTimeUTC ? timeseriesStartTime.utc_time()
                                                  : timeseriesStartTime.local_time());
    topt.endTime =
        (query.toptions.endTimeUTC ? timeseriesEndTime.utc_time() : timeseriesEndTime.local_time());

    // generate timelist for aggregation
    tlist = itsTimeSeriesCache->generate(topt, tz);

    qdtimesteps.insert(tlist->begin(), tlist->end());

    // for aggregation generate timesteps also between fixed times
    if (topt.mode == Spine::TimeSeriesGeneratorOptions::FixedTimes ||
        topt.mode == Spine::TimeSeriesGeneratorOptions::TimeSteps)
    {
      topt.mode = Spine::TimeSeriesGeneratorOptions::DataTimes;
      topt.timeSteps = boost::none;
      topt.timeStep = boost::none;
      topt.timeList.clear();
    }
    // generate timelist for aggregation
    tlist = itsTimeSeriesCache->generate(topt, tz);

    qdtimesteps.insert(tlist->begin(), tlist->end());

    // add timesteps to LocalTimeList
    Spine::TimeSeriesGenerator::LocalTimeList ret;
    for (const auto& ldt : qdtimesteps)
      ret.push_back(ldt);

#ifdef MYDEBUG
    std::cout << "Timesteps for timeseries: " << std::endl;
    for (const boost::local_time::local_date_time& ldt : ret)
    {
      std::cout << ldt << std::endl;
    }
#endif

    return ret;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void Plugin::fetchLocationValues(Query& query,
                                 Spine::PostGISDataSource& postGISDataSource,
                                 Spine::Table& data,
                                 unsigned int column_index,
                                 unsigned int row_index)
{
  try
  {
    unsigned int column = column_index;
    unsigned int row = row_index;

    BOOST_FOREACH (const Spine::Parameter& param, query.poptions.parameters())
    {
      row = row_index;
      std::string pname = param.name();
      BOOST_FOREACH (const auto& tloc, query.loptions->locations())
      {
        Spine::LocationPtr loc = tloc.loc;
        NFmiSvgPath svgPath;
        get_svg_path(tloc, postGISDataSource, svgPath);
        if (loc->type == Spine::Location::Path || loc->type == Spine::Location::Area)
          loc = getLocationForArea(tloc, query, svgPath, postGISDataSource);

        std::string val = location_parameter(
            tloc.loc, pname, query.valueformatter, query.timezone, query.precisions[column]);
        data.set(column, row++, val);
      }
      column++;
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void Plugin::fetchQEngineValues(const State& state,
                                const Spine::ParameterAndFunctions& paramfunc,
                                const Spine::TaggedLocation& tloc,
                                Query& query,
                                const AreaProducers& areaproducers,
                                const ProducerDataPeriod& producerDataPeriod,
                                Spine::PostGISDataSource& postGISDataSource,
                                QueryLevelDataCache& queryLevelDataCache,
                                OutputData& outputData)

{
  try
  {
    Spine::LocationPtr loc = tloc.loc;
    std::string place = get_name_base(loc->name);
    std::string paramname = paramfunc.parameter.name();

    NFmiSvgPath svgPath;
    if (loc->type == Spine::Location::Path || loc->type == Spine::Location::Area)
    {
      get_svg_path(tloc, postGISDataSource, svgPath);
      loc = getLocationForArea(tloc, query, svgPath, postGISDataSource);
    }
    else if (loc->type == Spine::Location::BoundingBox)
    {
      // find geoinfo for the corner coordinate
      vector<string> parts;
      boost::algorithm::split(parts, place, boost::algorithm::is_any_of(","));

      double lon1 = Fmi::stod(parts[0]);
      double lat1 = Fmi::stod(parts[1]);
      double lon2 = Fmi::stod(parts[2]);
      double lat2 = Fmi::stod(parts[3]);

      // get location info for center coordinate
      std::pair<double, double> lonlatCenter((lon1 + lon2) / 2.0, (lat1 + lat2) / 2.0);
      Spine::LocationPtr locCenter =
          itsGeoEngine->lonlatSearch(lonlatCenter.first, lonlatCenter.second, query.language);

      std::unique_ptr<Spine::Location> tmp(new Spine::Location(locCenter->geoid,
                                                               tloc.tag,
                                                               locCenter->iso2,
                                                               locCenter->municipality,
                                                               locCenter->area,
                                                               locCenter->feature,
                                                               locCenter->country,
                                                               locCenter->longitude,
                                                               locCenter->latitude,
                                                               locCenter->timezone,
                                                               locCenter->population,
                                                               locCenter->elevation));
      tmp->type = tloc.loc->type;
      tmp->radius = tloc.loc->radius;
      loc.reset(tmp.release());
    }

    if (query.timezone == LOCALTIME_PARAM)
      query.timezone = loc->timezone;

    // Select the producer for the coordinate
    auto producer = select_producer(*itsQEngine, *loc, query, areaproducers);

    bool isClimatologyProducer =
        (producer.empty() ? false : itsQEngine->getProducerConfig(producer).isclimatology);

    if (producer.empty())
    {
      Spine::Exception ex(BCP, "No data available for " + loc->name);
      ex.disableLogging();
      throw ex;
    }

    auto qi = (query.origintime ? state.get(producer, *query.origintime) : state.get(producer));

    const auto validtimes = qi->validTimes();
    if (validtimes->empty())
      throw Spine::Exception(BCP, "Producer " + producer + " has no valid time steps");
    query.toptions.setDataTimes(validtimes, qi->isClimatology());

    // no area operations allowed for non-grid data
    if (!qi->isGrid() &&
        ((loc->type != Spine::Location::Place && loc->type != Spine::Location::CoordinatePoint) ||
         ((loc->type == Spine::Location::Place || loc->type == Spine::Location::CoordinatePoint) &&
          loc->radius > 0)))
      return;

#ifdef BREAKS_REQUESTS_WHICH_START_BEFORE_QUERYDATA_BY_SHIFTING_DATA

    bool isMultiFile =
        (producer.empty() ? false : itsQEngine->getProducerConfig(producer).ismultifile);

    // timeseries start time can not be earlier than first time in data
    // time series end time can not be later than last time in data

    if (!isClimatologyProducer && !isMultiFile && !special(paramfunc.parameter))
    {
      if (query.toptions.startTime < validtimes->front())
      {
        query.toptions.startTime = validtimes->front();
        query.toptions.startTimeUTC = true;
      }
      if (query.toptions.endTime > validtimes->back())
      {
        query.toptions.endTime = validtimes->back();
        query.toptions.endTimeUTC = true;
      }
    }
#endif

    // If we accept nearest valid points, find it now for this location
    // This is fast if the nearest point is already valid
    NFmiPoint nearestpoint(kFloatMissing, kFloatMissing);
    if (query.findnearestvalidpoint)
    {
      NFmiPoint latlon(loc->longitude, loc->latitude);
      nearestpoint = qi->validPoint(latlon, query.maxdistance);
    }

    std::string country = itsGeoEngine->countryName(loc->iso2, query.language);

    std::vector<TimeSeriesData> aggregatedData;  // store here data of all levels

    // If no pressures/heights are chosen, loading all or just chosen data levels.
    //
    // Otherwise loading chosen data levels if any, then the chosen pressure
    // and/or height levels (interpolated values at chosen pressures/heights)

    bool loadDataLevels =
        (!query.levels.empty() || (query.pressures.empty() && query.heights.empty()));
    string levelType(loadDataLevels ? "data:" : "");
    Query::Pressures::const_iterator itPressure = query.pressures.begin();
    Query::Heights::const_iterator itHeight = query.heights.begin();

    // Loop over the levels
    for (qi->resetLevel();;)
    {
      boost::optional<float> pressure, height;
      float levelValue;

      if (loadDataLevels)
      {
        if (!qi->nextLevel())
          // No more native/data levels; load/interpolate pressure and height levels if any
          loadDataLevels = false;
        else
        {
          // check if only some levels are chosen
          if (!query.levels.empty())
          {
            int level = static_cast<int>(qi->levelValue());
            if (query.levels.find(level) == query.levels.end())
              continue;
          }
        }
      }

      if (!loadDataLevels)
      {
        if (itPressure != query.pressures.end())
        {
          levelType = "pressure:";
          pressure = levelValue = *(itPressure++);
        }
        else if (itHeight != query.heights.end())
        {
          levelType = "height:";
          height = levelValue = *(itHeight++);
        }
        else
          break;
      }

      // Generate the desired time steps as a new copy, since we'll modify the list (???)
      // TODO: Why not just use a proper ending time???

      auto tz = getTimeZones().time_zone_from_string(query.timezone);
      auto tlist = *itsTimeSeriesCache->generate(query.toptions, tz);
#ifdef MYDEBUG
      std::cout << "Generated timesteps:" << std::endl;
      for (const auto& t : tlist)
        std::cout << t << std::endl;
#endif

      // remove timesteps that are later than last timestep in query data file
      // except from climatology
      if (tlist.size() > 0 && !isClimatologyProducer)
      {
        boost::local_time::local_date_time data_period_endtime =
            producerDataPeriod.getLocalEndTime(producer, query.timezone, getTimeZones());

        while (tlist.size() > 0 && !data_period_endtime.is_not_a_date_time() &&
               *(--tlist.end()) > data_period_endtime)
        {
          tlist.pop_back();
        }
      }

      if (tlist.size() == 0)
        return;

      // store original timestep
      boost::optional<unsigned int> timeStepOriginal = query.toptions.timeSteps;
      // no timestep limitation for the query
      // redundant steps are removed later
      if (query.toptions.timeSteps)
      {
        if (query.toptions.timeList.size() > 0)
          query.toptions.timeSteps = (*query.toptions.timeSteps) * 24;
        else
        {
          if (query.toptions.mode != Spine::TimeSeriesGeneratorOptions::DataTimes)
          {
            // In case of datatimes, setting timeSteps to zero results in empty times
            // due to start and end-times being identical
            query.toptions.timeSteps = boost::optional<unsigned int>();
          }
        }
      }

      auto querydata_tlist = generateQEngineQueryTimes(qi, query, paramname);

      // restore original timestep
      query.toptions.timeSteps = timeStepOriginal;

      ts::Value missing_value = Spine::TimeSeries::None();

#ifdef MYDEBUG
      cout << endl << "producer: " << producer << endl;
      cout << "data period start time: "
           << producerDataPeriod.getLocalStartTime(producer, query.timezone, getTimeZones())
           << endl;
      cout << "data period end time: "
           << producerDataPeriod.getLocalEndTime(producer, query.timezone, getTimeZones()) << endl;
      cout << "paramname: " << paramname << endl;
      cout << "query.timezone: " << query.timezone << endl;
      cout << query.toptions;

      cout << "generated timesteps: " << endl;
      BOOST_FOREACH (const local_date_time& ldt, tlist)
      {
        cout << ldt << endl;
      }
#endif

      std::pair<float, std::string> cacheKey(loadDataLevels ? qi->levelValue() : levelValue,
                                             levelType + paramname);

      if ((loc->type == Spine::Location::Place || loc->type == Spine::Location::CoordinatePoint) &&
          loc->radius == 0)
      {
        ts::TimeSeriesPtr querydata_result;
        // if we have fetched the data for this parameter earlier, use it
        if (queryLevelDataCache.itsTimeSeries.find(cacheKey) !=
            queryLevelDataCache.itsTimeSeries.end())
        {
          querydata_result = queryLevelDataCache.itsTimeSeries[cacheKey];
        }
        else
        {
          Engine::Querydata::ParameterOptions querydata_param(paramfunc.parameter,
                                                              producer,
                                                              *loc,
                                                              country,
                                                              tloc.tag,
                                                              *query.timeformatter,
                                                              query.timestring,
                                                              query.language,
                                                              query.outlocale,
                                                              query.timezone,
                                                              query.findnearestvalidpoint,
                                                              nearestpoint,
                                                              query.lastpoint);

          // one location, list of local times (no radius -> pointforecast)
          querydata_result =
              loadDataLevels
                  ? qi->values(querydata_param, querydata_tlist)
                  : pressure ? qi->valuesAtPressure(querydata_param, querydata_tlist, *pressure)
                             : qi->valuesAtHeight(querydata_param, querydata_tlist, *height);
          if (querydata_result->size() > 0)
            queryLevelDataCache.itsTimeSeries.insert(make_pair(cacheKey, querydata_result));
        }

        aggregatedData.push_back(TimeSeriesData(
            erase_redundant_timesteps(aggregate(querydata_result, paramfunc.functions), tlist)));
      }
      else
      {
        ts::TimeSeriesGroupPtr querydata_result;
        ts::TimeSeriesGroupPtr aggregated_querydata_result;

        if (queryLevelDataCache.itsTimeSeriesGroups.find(cacheKey) !=
            queryLevelDataCache.itsTimeSeriesGroups.end())
        {
          querydata_result = queryLevelDataCache.itsTimeSeriesGroups[cacheKey];
        }
        else
        {
          if (loc->type == Spine::Location::Path)
          {
            Spine::LocationList llist =
                get_location_list(svgPath, tloc.tag, query.step, *itsGeoEngine);

            Engine::Querydata::ParameterOptions querydata_param(paramfunc.parameter,
                                                                producer,
                                                                *loc,
                                                                country,
                                                                tloc.tag,
                                                                *query.timeformatter,
                                                                query.timestring,
                                                                query.language,
                                                                query.outlocale,
                                                                query.timezone,
                                                                query.findnearestvalidpoint,
                                                                nearestpoint,
                                                                query.lastpoint);

            // list of locations, list of local times
            querydata_result =
                loadDataLevels
                    ? qi->values(querydata_param, llist, querydata_tlist, query.maxdistance)
                    : pressure
                          ? qi->valuesAtPressure(querydata_param,
                                                 llist,
                                                 querydata_tlist,
                                                 query.maxdistance,
                                                 *pressure)
                          : qi->valuesAtHeight(querydata_param,
                                               llist,
                                               querydata_tlist,
                                               query.maxdistance,
                                               *height);
            if (querydata_result->size() > 0)
            {
              // if the value is not dependent on location inside area we just need to have the
              // first
              // one
              if (!parameterIsArithmetic(paramfunc.parameter))
              {
                auto dataIndependentValue = querydata_result->at(0);
                querydata_result->clear();
                querydata_result->push_back(dataIndependentValue);
              }

              queryLevelDataCache.itsTimeSeriesGroups.insert(make_pair(cacheKey, querydata_result));
            }

            queryLevelDataCache.itsTimeSeriesGroups.insert(make_pair(cacheKey, querydata_result));
          }
          else if (qi->isGrid() &&
                   (loc->type == Spine::Location::BoundingBox ||
                    loc->type == Spine::Location::Area || loc->type == Spine::Location::Place ||
                    loc->type == Spine::Location::CoordinatePoint))
          {
            NFmiIndexMask mask;

            if (loc->type == Spine::Location::BoundingBox)
            {
              vector<string> coordinates;
              boost::algorithm::split(coordinates, place, boost::algorithm::is_any_of(","));
              if (coordinates.size() != 4)
                throw Spine::Exception(BCP,
                                       "Invalid bbox parameter " + place +
                                           ", should be in format 'lon,lat,lon,lat[:radius]'!");

              string lonstr1 = coordinates[0];
              string latstr1 = coordinates[1];
              string lonstr2 = coordinates[2];
              string latstr2 = coordinates[3];

              if (latstr2.find(':') != std::string::npos)
                latstr2.erase(latstr2.begin() + latstr2.find(':'), latstr2.end());

              double lon1 = Fmi::stod(lonstr1);
              double lat1 = Fmi::stod(latstr1);
              double lon2 = Fmi::stod(lonstr2);
              double lat2 = Fmi::stod(latstr2);

              NFmiSvgPath boundingBoxPath;
              std::pair<double, double> firstCorner(lon1, lat1);
              std::pair<double, double> secondCorner(lon2, lat2);
              make_rectangle_path(boundingBoxPath, firstCorner, secondCorner);

              mask = NFmiIndexMaskTools::MaskExpand(qi->grid(), boundingBoxPath, loc->radius);
            }
            else if (loc->type == Spine::Location::Area || loc->type == Spine::Location::Place ||
                     loc->type == Spine::Location::CoordinatePoint)
            {
              get_svg_path(tloc, postGISDataSource, svgPath);
              mask = NFmiIndexMaskTools::MaskExpand(qi->grid(), svgPath, loc->radius);
            }

            Engine::Querydata::ParameterOptions querydata_param(paramfunc.parameter,
                                                                producer,
                                                                *loc,
                                                                country,
                                                                tloc.tag,
                                                                *query.timeformatter,
                                                                query.timestring,
                                                                query.language,
                                                                query.outlocale,
                                                                query.timezone,
                                                                query.findnearestvalidpoint,
                                                                nearestpoint,
                                                                query.lastpoint);

            // indexmask (indexed locations on the area), list of local times
            querydata_result =
                loadDataLevels
                    ? qi->values(querydata_param, mask, querydata_tlist)
                    : pressure
                          ? qi->valuesAtPressure(querydata_param, mask, querydata_tlist, *pressure)
                          : qi->valuesAtHeight(querydata_param, mask, querydata_tlist, *height);

            if (querydata_result->size() > 0)
            {
              // if the value is not dependent on location inside area we just need to have the
              // first
              // one
              if (!parameterIsArithmetic(paramfunc.parameter))
              {
                auto dataIndependentValue = querydata_result->at(0);
                querydata_result->clear();
                querydata_result->push_back(dataIndependentValue);
              }

              queryLevelDataCache.itsTimeSeriesGroups.insert(make_pair(cacheKey, querydata_result));
            }
          }
        }  // area handling

        if (querydata_result->size() > 0)
          aggregatedData.push_back(TimeSeriesData(
              erase_redundant_timesteps(aggregate(querydata_result, paramfunc.functions), tlist)));
      }
    }  // levels

    // store level-data
    store_data(aggregatedData, query, outputData);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION
std::vector<ObsParameter> Plugin::getObsParameters(const Query& query) const
{
  try
  {
    std::vector<ObsParameter> ret;
    if (itsObsEngine == nullptr)
      return ret;

    // if observation query exists, sort out obengine parameters
    std::set<std::string> stationTypes = itsObsEngine->getValidStationTypes();

    bool done = false;
    BOOST_FOREACH (const auto& areaproducers, query.timeproducers)
    {
      BOOST_FOREACH (const auto& producer, areaproducers)
      {
        if (stationTypes.find(producer) != stationTypes.end())
        {
          std::map<std::string, unsigned int> parameter_columns;
          unsigned int column_index = 0;
          BOOST_FOREACH (const Spine::ParameterAndFunctions& paramfuncs,
                         query.poptions.parameterFunctions())
          {
            Spine::Parameter parameter(paramfuncs.parameter);

            if (parameter_columns.find(parameter.name()) != parameter_columns.end())
            {
              ret.push_back(ObsParameter(
                  parameter, paramfuncs.functions, parameter_columns.at(parameter.name()), true));
            }
            else
            {
              ret.push_back(ObsParameter(parameter, paramfuncs.functions, column_index, false));

              parameter_columns.insert(make_pair(parameter.name(), column_index));
              column_index++;
            }
          }
          done = true;
          break;  // exit inner loop
        }
      }
      if (done)
        break;  // exit outer loop
    }

    return ret;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
#endif

// ----------------------------------------------------------------------
/*!
 * \brief Set obsegnine settings common for all queries
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION
void Plugin::setCommonObsSettings(Engine::Observation::Settings& settings,
                                  const std::string& producer,
                                  const ProducerDataPeriod& producerDataPeriod,
                                  const boost::posix_time::ptime& now,
                                  const ObsParameters& obsParameters,
                                  Query& query,
                                  Spine::PostGISDataSource& postGISDataSource) const
{
  try
  {
    // At least one of location specifiers must be set
    settings.geoids = query.geoids;
    settings.fmisids = query.fmisids;
    settings.lpnns = query.lpnns;
    settings.wmos = query.wmos;
    settings.boundingBox = query.boundingBox;
    settings.taggedLocations = query.loptions->locations();

    // Below are listed optional settings, defaults are set while constructing an ObsEngine::Oracle
    // instance.

    // TODO Because timezone="localtime" functions differently in observation,
    // force default timezone to be Europe/Helsinki. This must be fixed when obsplugin is obsoleted
    if (query.timezone == "localtime")
      query.timezone = "Europe/Helsinki";
    settings.timezone = (query.timezone == LOCALTIME_PARAM ? UTC_PARAM : query.timezone);

    settings.format = query.format;
    settings.stationtype = producer;
    settings.maxdistance = query.maxdistance;
    if (!query.maxdistanceOptionGiven)
      settings.maxdistance = 60000;
    settings.allplaces = query.allplaces;

    settings.timeformat = query.timeformat;
    settings.weekdays = query.weekdays;
    settings.language = query.language;
    settings.missingtext = query.valueformatter.missing();
    // Does this one actually do anything once the Setting object has been initialized??
    settings.localename = query.localename;
    settings.numberofstations = query.numberofstations;
    settings.latest = query.latestObservation;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
#endif

// ----------------------------------------------------------------------
/*!
 * \brief Update settings for a specific location
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION
void Plugin::setLocationObsSettings(Engine::Observation::Settings& settings,
                                    const std::string& producer,
                                    const ProducerDataPeriod& producerDataPeriod,
                                    const boost::posix_time::ptime& now,
                                    const Spine::TaggedLocation& tloc,
                                    const ObsParameters& obsParameters,
                                    Query& query,
                                    Spine::PostGISDataSource& postGISDataSource) const
{
  try
  {
    settings.parameters.clear();
    settings.area_geoids.clear();

    unsigned int aggregationIntervalBehind = 0;
    unsigned int aggregationIntervalAhead = 0;

    int fmisid_index = -1;
    for (unsigned int i = 0; i < obsParameters.size(); i++)
    {
      const Spine::Parameter& param = obsParameters[i].param;

      if (query.maxAggregationIntervals[param.name()].behind > aggregationIntervalBehind)
        aggregationIntervalBehind = query.maxAggregationIntervals[param.name()].behind;
      if (query.maxAggregationIntervals[param.name()].ahead > aggregationIntervalAhead)
        aggregationIntervalAhead = query.maxAggregationIntervals[param.name()].ahead;

      // prevent passing duplicate parameters to observation (for example temperature,
      // max_t(temperature))
      // location parameters are handled in timeseries plugin
      if (obsParameters[i].duplicate ||
          (is_location_parameter(obsParameters[i].param.name()) && producer != FLASH_PRODUCER) ||
          is_time_parameter(obsParameters[i].param.name()))
        continue;

      // fmisid must be always included (except for flash) in queries in order to get location info
      // from geonames
      if (param.name() == FMISID_PARAM && producer != FLASH_PRODUCER)
        fmisid_index = settings.parameters.size();

      // all parameters are fetched at once
      settings.parameters.push_back(param);
    }

    std::string loc_name_original = (tloc.loc ? tloc.loc->name : "");
    std::string loc_name = (tloc.loc ? get_name_base(tloc.loc->name) : "");

    // Area handling:
    // 1) Get OGRGeometry object, expand it with radius
    // 2) Fetch stations geoids from ObsEngine located inside the area
    // 3) Fetch location info from GeoEngine. The info will be used in responses to location and
    // time
    // related queries
    // 4) Fetch the observations from ObsEngine using the geoids
    if (tloc.loc)
    {
      if (tloc.loc->type == Spine::Location::Path)
      {
#ifdef MYDEBUG
        std::cout << tloc.loc->name << " is a Path" << std::endl;
#endif
        std::string pathName = loc_name;
        const OGRGeometry* pGeo = 0;
        if (pathName.find(',') != std::string::npos)
        {
          NFmiSvgPath svgPath;
          get_svg_path(tloc, postGISDataSource, svgPath);
          Fmi::OGR::CoordinatePoints geoCoordinates;
          for (NFmiSvgPath::const_iterator iter = svgPath.begin(); iter != svgPath.end(); iter++)
            geoCoordinates.push_back(std::pair<double, double>(iter->itsX, iter->itsY));
          pGeo = Fmi::OGR::constructGeometry(geoCoordinates, wkbLineString, 4326);

          if (!pGeo)
            throw Spine::Exception(
                BCP,
                "Invalid path parameter " + tloc.loc->name +
                    ", should be in format 'lon0,lat0,lon1,lat1,...,lonn,latn[:radius]'!");
        }
        else
        {
          // path is fetched from database
          pGeo = postGISDataSource.getOGRGeometry(loc_name, wkbMultiLineString);

          if (!pGeo)
            throw Spine::Exception(BCP, "Path " + loc_name + " not found in PostGIS database!");
        }

        std::unique_ptr<OGRGeometry> poly;
        // if no radius has been given use 200 meters
        double radius = (tloc.loc->radius == 0 ? 200 : tloc.loc->radius * 1000);
        poly.reset(Fmi::OGR::expandGeometry(pGeo, radius));

        std::string wktString = Fmi::OGR::exportToWkt(*poly);
        settings.area_geoids = getGeoids(itsObsEngine, producer, wktString);

#ifdef MYDEBUG
        std::cout << "WKT of buffered area: " << std::endl << wktString << std::endl;
        std::cout << "#" << settings.area_geoids.size() << " stations found" << std::endl;
#endif

        query.maxdistance = 0;
      }
      else if (tloc.loc->type == Spine::Location::Area)
      {
#ifdef MYDEBUG
        std::cout << tloc.loc->name << " is an Area" << std::endl;
#endif

        const OGRGeometry* pGeo = postGISDataSource.getOGRGeometry(loc_name, wkbMultiPolygon);

        if (!pGeo)
          throw Spine::Exception(BCP, "Area " + tloc.loc->name + " not found in PostGIS database!");

        std::unique_ptr<OGRGeometry> poly;
        poly.reset(Fmi::OGR::expandGeometry(pGeo, tloc.loc->radius * 1000));

        std::string wktString = Fmi::OGR::exportToWkt(*poly);
        settings.area_geoids = getGeoids(itsObsEngine, producer, wktString);

#ifdef MYDEBUG
        std::cout << "WKT of buffered area: " << std::endl << wktString << std::endl;
        std::cout << "#" << settings.area_geoids.size() << " stations found" << std::endl;
#endif

        query.maxdistance = 0;
      }
      else if (tloc.loc->type == Spine::Location::BoundingBox && producer != FLASH_PRODUCER)
      {
#ifdef MYDEBUG
        std::cout << tloc.loc->name << " is a BoundingBox" << std::endl;
#endif

        Spine::BoundingBox bbox(loc_name);

        NFmiSvgPath svgPath;
        std::pair<double, double> firstCorner(bbox.xMin, bbox.yMin);
        std::pair<double, double> secondCorner(bbox.xMax, bbox.yMax);
        make_rectangle_path(svgPath, firstCorner, secondCorner);
        Fmi::OGR::CoordinatePoints geoCoordinates;
        for (NFmiSvgPath::const_iterator iter = svgPath.begin(); iter != svgPath.end(); iter++)
          geoCoordinates.push_back(std::pair<double, double>(iter->itsX, iter->itsY));
        const OGRGeometry* pGeo = Fmi::OGR::constructGeometry(geoCoordinates, wkbPolygon, 4326);

        std::unique_ptr<OGRGeometry> poly;
        poly.reset(Fmi::OGR::expandGeometry(pGeo, tloc.loc->radius * 1000));

        std::string wktString(Fmi::OGR::exportToWkt(*poly));
        settings.area_geoids = getGeoids(itsObsEngine, producer, wktString);

#ifdef MYDEBUG
        std::cout << "WKT of buffered area: " << std::endl << wktString << std::endl;
        std::cout << "#" << settings.area_geoids.size() << " stations found" << std::endl;
#endif

        query.maxdistance = 0;
      }
      else if (producer != FLASH_PRODUCER && (tloc.loc->type == Spine::Location::Place ||
                                              tloc.loc->type == Spine::Location::CoordinatePoint) &&
               tloc.loc->radius > 0)
      {
#ifdef MYDEBUG
        std::cout << tloc.loc->name << " is an Area (Place + radius)" << std::endl;
#endif

        const OGRGeometry* pGeo = postGISDataSource.getOGRGeometry(loc_name, wkbPoint);

        if (!pGeo)
          throw Spine::Exception(BCP, "Area " + loc_name + " not found in PostGIS database!");

        std::unique_ptr<OGRGeometry> poly;
        poly.reset(Fmi::OGR::expandGeometry(pGeo, tloc.loc->radius * 1000));

        std::string wktString = Fmi::OGR::exportToWkt(*poly);
        settings.area_geoids = getGeoids(itsObsEngine, producer, wktString);
        query.maxdistance = 0;

#ifdef MYDEBUG
        std::cout << "WKT of buffered area: " << std::endl << wktString << std::endl;
        std::cout << "#" << settings.area_geoids.size() << " stations found" << std::endl;
#endif
      }
      else if (producer != FLASH_PRODUCER && tloc.loc->type == Spine::Location::CoordinatePoint &&
               tloc.loc->radius > 0)
      {
#ifdef MYDEBUG
        std::cout << tloc.loc->name << " is an Area (coordinate point + radius)" << std::endl;
#endif
        Fmi::OGR::CoordinatePoints geoCoordinates;
        geoCoordinates.push_back(
            std::pair<double, double>(tloc.loc->longitude, tloc.loc->latitude));
        const OGRGeometry* pGeo = Fmi::OGR::constructGeometry(geoCoordinates, wkbPoint, 4326);

        std::unique_ptr<OGRGeometry> poly;
        poly.reset(Fmi::OGR::expandGeometry(pGeo, tloc.loc->radius * 1000));

        std::string wktString = Fmi::OGR::exportToWkt(*poly);
        settings.area_geoids = getGeoids(itsObsEngine, producer, wktString);
        query.maxdistance = 0;

#ifdef MYDEBUG
        std::cout << "WKT of buffered area: " << std::endl << wktString << std::endl;
        std::cout << "#" << settings.area_geoids.size() << " stations found" << std::endl;
#endif
      }

    }  // if(tloc.loc)

    // fmisid must be always included in order to get thelocation info from geonames
    if (fmisid_index == -1 && producer != FLASH_PRODUCER)
    {
      Spine::Parameter fmisidParam =
          Spine::Parameter(FMISID_PARAM, Spine::Parameter::Type::DataIndependent);
      settings.parameters.push_back(fmisidParam);
    }

    // Below are listed optional settings, defaults are set while constructing an ObsEngine::Oracle
    // instance.

    boost::local_time::time_zone_ptr tz = getTimeZones().time_zone_from_string(query.timezone);
    local_date_time ldt_now(now, tz);

    if (query.toptions.startTimeData)
    {
      query.toptions.startTime = ldt_now.local_time() - hours(24);
      query.toptions.startTimeData = false;
    }
    if (query.toptions.endTimeData)
    {
      query.toptions.endTime = ldt_now.local_time();
      query.toptions.endTimeData = false;
    }

    if (query.toptions.startTime > ldt_now.local_time())
      query.toptions.startTime = ldt_now.local_time();
    if (query.toptions.endTime > ldt_now.local_time())
      query.toptions.endTime = ldt_now.local_time();

    if (!query.starttimeOptionGiven && !query.endtimeOptionGiven)
    {
      query.toptions.startTime =
          producerDataPeriod.getLocalStartTime(producer, query.timezone, getTimeZones())
              .local_time();
      query.toptions.endTime =
          producerDataPeriod.getLocalEndTime(producer, query.timezone, getTimeZones()).local_time();
    }

    if (query.starttimeOptionGiven && !query.endtimeOptionGiven)
    {
      query.toptions.endTime = ldt_now.local_time();
    }

    if (!query.starttimeOptionGiven && query.endtimeOptionGiven)
    {
      query.toptions.startTime = query.toptions.endTime - hours(24);
    }

    // observation requires the times to be in UTC. The correct way to do it
    // is to use the make_time function IF the times are assumed to be in local time

    boost::local_time::local_date_time local_starttime(query.toptions.startTime, tz);
    boost::local_time::local_date_time local_endtime(query.toptions.endTime, tz);

    if (!query.toptions.startTimeUTC)
      local_starttime = Fmi::TimeParser::make_time(
          query.toptions.startTime.date(), query.toptions.startTime.time_of_day(), tz);

    if (!query.toptions.endTimeUTC)
      local_endtime = Fmi::TimeParser::make_time(
          query.toptions.endTime.date(), query.toptions.endTime.time_of_day(), tz);

    settings.starttime = local_starttime.utc_time();
    settings.endtime = local_endtime.utc_time();

    // Adjust to accommodate aggregation

    settings.starttime = settings.starttime - minutes(aggregationIntervalBehind);
    settings.endtime = settings.endtime + minutes(aggregationIntervalAhead);

    // observations up till now
    if (settings.endtime > now)
      settings.endtime = now;

    if (!query.toptions.timeStep || *query.toptions.timeStep == 0)
      settings.timestep = 1;
    else
      settings.timestep = *query.toptions.timeStep;

#ifdef MYDEBUG
    std::cout << "query.toptions.startTimeUTC: " << (query.toptions.startTimeUTC ? "true" : "false")
              << endl;
    std::cout << "query.toptions.endTimeUTC: " << (query.toptions.endTimeUTC ? "true" : "false")
              << endl;
    std::cout << "query.toptions.startTime: " << query.toptions.startTime << std::endl;
    std::cout << "query.toptions.endTime: " << query.toptions.endTime << std::endl;

    std::cout << "settings.allplaces: " << settings.allplaces << std::endl;
    std::cout << "settings.boundingBox.size(): " << settings.boundingBox.size() << std::endl;
    std::cout << "settings.boundingBoxIsGiven: " << settings.boundingBoxIsGiven << std::endl;
    std::cout << "settings.starttime: " << settings.starttime << std::endl;
    std::cout << "settings.endtime: " << settings.endtime << std::endl;
    std::cout << "settings.starttimeGiven: " << settings.starttimeGiven << std::endl;
    std::cout << "settings.fmisids.size(): " << settings.fmisids.size() << std::endl;
    std::cout << "settings.format: " << settings.format << std::endl;
    std::cout << "settings.geoids.size(): " << settings.geoids.size() << std::endl;
    std::cout << "settings.hours.size(): " << settings.hours.size() << std::endl;
    std::cout << "settings.language: " << settings.language << std::endl;
    std::cout << "settings.latest: " << settings.latest << std::endl;
    std::cout << "settings.localename: " << settings.localename << std::endl;
    std::cout << "settings.locale.name(): " << settings.locale.name() << std::endl;
    std::cout << "settings.lpnns.size(): " << settings.lpnns.size() << std::endl;
    std::cout << "settings.maxdistance: " << settings.maxdistance << std::endl;
    std::cout << "settings.missingtext: " << settings.missingtext << std::endl;
    std::cout << "settings.locations.size(): " << settings.locations.size() << std::endl;
    std::cout << "settings.coordinates.size(): " << settings.coordinates.size() << std::endl;
    std::cout << "settings.numberofstations: " << settings.numberofstations << std::endl;
    std::cout << "settings.parameters.size(): " << settings.parameters.size() << std::endl;
    std::cout << "settings.stationtype: " << settings.stationtype << std::endl;
    std::cout << "settings.timeformat: " << settings.timeformat << std::endl;
    std::cout << "settings.timestep: " << settings.timestep << std::endl;
    std::cout << "settings.timestring: " << settings.timestring << std::endl;
    std::cout << "settings.timezone: " << settings.timezone << std::endl;
    std::cout << "settings.weekdays.size(): " << settings.weekdays.size() << std::endl;
    std::cout << "settings.wmos.size(): " << settings.wmos.size() << std::endl;
#endif
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
#endif

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION
void Plugin::fetchObsEngineValuesForPlaces(const State& state,
                                           const std::string& producer,
                                           const Spine::TaggedLocation& tloc,
                                           const ObsParameters& obsParameters,
                                           Engine::Observation::Settings& settings,
                                           Query& query,
                                           OutputData& outputData)
{
  try
  {
    ts::TimeSeriesVectorPtr observation_result;

    // Quick query if there is no aggregation
    if (!query.timeAggregationRequested)
    {
      observation_result = itsObsEngine->values(settings, query.toptions);
    }
    else
    {
      // Request all observations in order to do aggregation
      Spine::TimeSeriesGeneratorOptions tmpoptions;
      tmpoptions.startTime = settings.starttime;
      tmpoptions.endTime = settings.endtime;
      tmpoptions.startTimeUTC = query.toptions.startTimeUTC;
      tmpoptions.endTimeUTC = query.toptions.endTimeUTC;
      settings.timestep = 1;

      // fetches results for all location and all parameters
      observation_result = itsObsEngine->values(settings, tmpoptions);
    }

    if (observation_result->size() == 0)
      return;

    int fmisid_index = get_fmisid_index(settings);

    TimeSeriesByLocation observation_result_by_location =
        get_timeseries_by_fmisid(producer, observation_result, settings, query, fmisid_index);

    Spine::TimeSeriesGeneratorCache::TimeList tlist;
    auto tz = getTimeZones().time_zone_from_string(query.timezone);
    if (!query.toptions.all())
      tlist = itsTimeSeriesCache->generate(query.toptions, tz);

    // iterate locations
    for (unsigned int i = 0; i < observation_result_by_location.size(); i++)
    {
      observation_result = observation_result_by_location[i].second;
      int fmisid = observation_result_by_location[i].first;

      // get location
      Spine::LocationPtr loc;
      if (producer != FLASH_PRODUCER)
      {
        loc = getLocation(itsGeoEngine, fmisid, FMISID_PARAM, query.language);
        if (!loc)
          continue;
      }
      else
      {
        Spine::Location location(0, 0, "", query.timezone);
        loc = boost::make_shared<Spine::Location>(location);
      }

      // lets find out actual timesteps
      std::vector<boost::local_time::local_date_time> timestep_vector;
      const ts::TimeSeries& ts = observation_result->at(0);
      for (unsigned int i = 0; i < ts.size(); i++)
        timestep_vector.push_back(ts[i].time);

      unsigned int obs_result_field_index = 0;
      ts::TimeSeriesVectorPtr observationResult2(new ts::TimeSeriesVector());
      // iterate parameters
      for (unsigned int i = 0; i < obsParameters.size(); i++)
      {
        const ObsParameter& obsParam(obsParameters[i]);

        std::string paramname(obsParam.param.name());

        if (is_location_parameter(paramname) && producer != FLASH_PRODUCER)
        {
          // add data for location field
          ts::TimeSeries timeseries;
          for (unsigned int j = 0; j < timestep_vector.size(); j++)
          {
            ts::Value value = location_parameter(loc,
                                                 obsParam.param.name(),
                                                 query.valueformatter,
                                                 query.timezone,
                                                 query.precisions[i]);

            timeseries.push_back(ts::TimedValue(timestep_vector[j], value));
          }

          observationResult2->push_back(timeseries);
        }
        else if (is_time_parameter(paramname))
        {
          // add data for time fields
          Spine::Location location(0, 0, "", query.timezone);
          ts::TimeSeries timeseries;
          for (unsigned int j = 0; j < timestep_vector.size(); j++)
          {
            std::string value = time_parameter(paramname,
                                               timestep_vector[j],
                                               state.getTime(),
                                               *loc,
                                               query.timezone,
                                               getTimeZones(),
                                               query.outlocale,
                                               *query.timeformatter,
                                               query.timestring);

            timeseries.push_back(ts::TimedValue(timestep_vector[j], value));
          }
          observationResult2->push_back(timeseries);
        }
        else if (!obsParameters[i].duplicate)
        {
          // add data fields fetched from observation
          auto result = *observation_result;
          if (result[obs_result_field_index].empty())
          {
            continue;
          }
          auto result_at_index = result[obs_result_field_index];
          // observationResult2->push_back((*observation_result)[obs_result_field_index]);
          observationResult2->push_back(result_at_index);
          obs_result_field_index++;
        }
      }

      // TODO: Can we do a fast exit here if query.timeAggregationRequested == false???

      observation_result = observationResult2;

      ts::Value missing_value = Spine::TimeSeries::None();
      ts::TimeSeriesVectorPtr aggregated_observation_result(new ts::TimeSeriesVector());
      std::vector<TimeSeriesData> aggregatedData;
      // iterate parameters and do aggregation
      for (unsigned int i = 0; i < obsParameters.size(); i++)
      {
        unsigned int data_column = obsParameters[i].data_column;
        Spine::ParameterFunctions pfunc = obsParameters[i].functions;

        if (data_column >= observation_result->size())
          continue;

        ts::TimeSeries ts = (*observation_result)[data_column];
        ts::TimeSeriesPtr tsptr = ts::aggregate(ts, pfunc);
        if (tsptr->size() == 0)
          continue;

        aggregated_observation_result->push_back(*ts::aggregate(ts, pfunc));
      }

      if (aggregated_observation_result->size() == 0)
      {
#ifdef MYDEBUG
        std::cout << "aggregated_observation_result (" << producer << ") is empty" << std::endl;
#endif
        continue;
      }
#ifdef MYDEBUG
      std::cout << "aggregated_observation_result (" << producer << ")" << std::endl;
      std::cout << *aggregated_observation_result << std::endl;
#endif

      // if producer is syke or flash accept all timesteps
      if (query.toptions.all() || producer == FLASH_PRODUCER || producer == SYKE_PRODUCER)
      {
        Spine::TimeSeriesGenerator::LocalTimeList aggtimes;
        for (const ts::TimedValue& tv : aggregated_observation_result->at(0))
          aggtimes.push_back(tv.time);
        // store observation data
        store_data(
            erase_redundant_timesteps(aggregated_observation_result, aggtimes), query, outputData);
      }
      else
      {
        // Else accept only the original generated timesteps
        store_data(
            erase_redundant_timesteps(aggregated_observation_result, *tlist), query, outputData);
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
#endif

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION
void Plugin::fetchObsEngineValuesForArea(const State& state,
                                         const std::string& producer,
                                         const Spine::TaggedLocation& tloc,
                                         const ObsParameters& obsParameters,
                                         Engine::Observation::Settings& settings,
                                         Query& query,
                                         OutputData& outputData)
{
  try
  {
    settings.geoids = settings.area_geoids;
    settings.locations.clear();

    // fetches results for all locations in the area and all parameters
    ts::TimeSeriesVectorPtr observation_result = itsObsEngine->values(settings, query.toptions);

#ifdef MYDEBYG
    std::cout << "observation_result for area: " << *observation_result << std::endl;
#endif
    if (observation_result->size() == 0)
      return;

    Spine::LocationPtr loc = tloc.loc;
    std::string place = (loc ? loc->name : "");

    // lets find out actual timesteps: different locations may have different timesteps
    std::vector<boost::local_time::local_date_time> ts_vector;
    std::set<boost::local_time::local_date_time> ts_set;
    const ts::TimeSeries& ts = observation_result->at(0);
    BOOST_FOREACH (const ts::TimedValue& tval, ts)
      ts_set.insert(tval.time);
    ts_vector.insert(ts_vector.end(), ts_set.begin(), ts_set.end());
    std::sort(ts_vector.begin(), ts_vector.end());

    int fmisid_index = get_fmisid_index(settings);

    // separate timeseries of different locations to their own data structures
    TimeSeriesByLocation tsv_area =
        get_timeseries_by_fmisid(producer, observation_result, settings, query, fmisid_index);
    // make sure that all timeseries have the same timesteps
    BOOST_FOREACH (FmisidTSVectorPair& val, tsv_area)
    {
      ts::TimeSeriesVector* tsv = val.second.get();
      BOOST_FOREACH (ts::TimeSeries& ts, *tsv)
      {
        for (unsigned int k = 0; k < ts_vector.size(); k++)
        {
          boost::local_time::local_date_time lt(ts_vector[k]);
          ts::Value val("");
          if (ts.size() == k)
            ts.push_back(ts::TimedValue(lt, val));
          else if (ts[k].time != lt)
          {
            ts.insert(ts.begin() + k, ts::TimedValue(lt, val));
          }
        }
      }
    }

    std::vector<FmisidTSVectorPair> tsv_area_with_added_fields;
    // add data for location- and time-related fields; these fields are added by timeseries plugin
    BOOST_FOREACH (FmisidTSVectorPair& val, tsv_area)
    {
      ts::TimeSeriesVector* tsv_observation_result = val.second.get();

      ts::TimeSeriesVectorPtr observation_result_with_added_fields(new ts::TimeSeriesVector());
      //		  ts::TimeSeriesVector* tsv_observation_result = observation_result.get();
      const ts::TimeSeries& fmisid_ts = tsv_observation_result->at(fmisid_index);
      unsigned int obs_result_field_index = 0;
      for (unsigned int i = 0; i < obsParameters.size(); i++)
      {
        std::string paramname = obsParameters[i].param.name();

        // add data for location fields
        if (is_location_parameter(paramname) && producer != FLASH_PRODUCER)
        {
          ts::TimeSeries location_ts;

          for (unsigned int j = 0; j < ts_vector.size(); j++)
          {
            int fmisid = get_fmisid_value(fmisid_ts[j].value);
            Spine::LocationPtr loc =
                getLocation(itsGeoEngine, fmisid, FMISID_PARAM, query.language);
            ts::Value value = location_parameter(loc,
                                                 obsParameters[i].param.name(),
                                                 query.valueformatter,
                                                 query.timezone,
                                                 query.precisions[i]);

            location_ts.push_back(ts::TimedValue(ts_vector[j], value));
          }
          observation_result_with_added_fields->push_back(location_ts);
        }
        else if (is_time_parameter(paramname))
        {
          // add data for time fields
          Spine::Location location(0, 0, "", query.timezone);

          ts::TimeSeriesGroupPtr grp(new ts::TimeSeriesGroup);
          ts::TimeSeries time_ts;
          for (unsigned int j = 0; j < ts_vector.size(); j++)
          {
            int fmisid = get_fmisid_value(fmisid_ts[j].value);
            Spine::LocationPtr loc =
                getLocation(itsGeoEngine, fmisid, FMISID_PARAM, query.language);
            std::string paramvalue = time_parameter(paramname,
                                                    ts_vector[j],
                                                    state.getTime(),
                                                    (loc ? *loc : location),
                                                    query.timezone,
                                                    getTimeZones(),
                                                    query.outlocale,
                                                    *query.timeformatter,
                                                    query.timestring);

            time_ts.push_back(ts::TimedValue(ts_vector[j], paramvalue));
          }
          observation_result_with_added_fields->push_back(time_ts);
        }
        else if (!obsParameters[i].duplicate)
        {
          observation_result_with_added_fields->push_back(
              tsv_observation_result->at(obs_result_field_index));
          obs_result_field_index++;
        }
      }
      tsv_area_with_added_fields.push_back(
          FmisidTSVectorPair(val.first, observation_result_with_added_fields));
    }

#ifdef MYDEBUG
    std::cout << "observation_result after locally added fields: " << std::endl;

    BOOST_FOREACH (FmisidTSVectorPair& val, tsv_area_with_added_fields)
    {
      ts::TimeSeriesVector* tsv = val.second.get();
      std::cout << "timeseries for fmisid " << val.first << ": " << std::endl << *tsv << std::endl;
    }
#endif

    // build time series vector groups for parameters
    // each group is dedicated for one parameter and it contains timeseries
    // of single parameter from different locations
    //
    // each TimeSeriesGroup contains a vector of timeseries of a single parameter as follows
    //
    // TimeSeriesGroup1
    //     loc1 name        loc2 name
    // -------------------------------
    // t1  rautatientori    kumpula
    // t2  rautatientori    kumpula
    // ..  ..               ..
    //
    // TimeSeriesGroup2
    //     loc1 t2m        loc2 t2m
    // -------------------------------
    // t1  9.1             9.2
    // t2  9.0             9.1
    // ..  ..              ..
    //

    std::vector<ts::TimeSeriesGroupPtr> tsg_vector;
    for (unsigned int i = 0; i < obsParameters.size(); i++)
      tsg_vector.push_back(ts::TimeSeriesGroupPtr(new ts::TimeSeriesGroup));

    // iterate locations
    for (unsigned int i = 0; i < tsv_area_with_added_fields.size(); i++)
    {
      const ts::TimeSeriesVector* tsv = tsv_area_with_added_fields[i].second.get();

      // iterate fields
      for (unsigned int k = 0; k < tsv->size(); k++)
      {
        // add k:th time series to the group
        ts::LonLat lonlat(0, 0);  // location is not relevant here
        ts::LonLatTimeSeries lonlat_ts(lonlat, tsv->at(k));
        tsg_vector[k]->push_back(lonlat_ts);
      }
    }

#ifdef MYDEBUG
    std::cout << "timeseries groups: " << std::endl;
    for (unsigned int i = 0; i < tsg_vector.size(); i++)
    {
      ts::TimeSeriesGroupPtr tsg = tsg_vector.at(i);
      std::cout << "group#" << i << ": " << std::endl << *tsg << std::endl;
    }
#endif

    // first generate timesteps
    Spine::TimeSeriesGeneratorCache::TimeList tlist;
    auto tz = getTimeZones().time_zone_from_string(query.timezone);
    if (!query.toptions.all())
      tlist = itsTimeSeriesCache->generate(query.toptions, tz);

    ts::Value missing_value = Spine::TimeSeries::None();
    // iterate parameters, aggregate, and store aggregated result
    for (unsigned int i = 0; i < obsParameters.size(); i++)
    {
      unsigned int data_column = obsParameters[i].data_column;
      ts::TimeSeriesGroupPtr tsg = tsg_vector.at(data_column);

      if (tsg->size() == 0)
        continue;

      if (special(obsParameters[i].param))
      {
        // value of these special fields is different in different locations,
        if (obsParameters[i].param.name() != STATIONNAME_PARAM &&
            obsParameters[i].param.name() != STATION_NAME_PARAM &&
            obsParameters[i].param.name() != LON_PARAM &&
            obsParameters[i].param.name() != LAT_PARAM &&
            obsParameters[i].param.name() != LATLON_PARAM &&
            obsParameters[i].param.name() != LONLAT_PARAM &&
            obsParameters[i].param.name() != PLACE_PARAM &&
            obsParameters[i].param.name() != STATIONLON_PARAM &&
            obsParameters[i].param.name() != STATIONLAT_PARAM &&
            obsParameters[i].param.name() != STATIONLONGITUDE_PARAM &&
            obsParameters[i].param.name() != STATIONLATITUDE_PARAM &&
            obsParameters[i].param.name() != STATION_ELEVATION_PARAM &&
            obsParameters[i].param.name() != STATIONARY_PARAM &&
            // obsParameters[i].param.name() != DISTANCE_PARAM &&
            // obsParameters[i].param.name() != DIRECTION_PARAM &&
            obsParameters[i].param.name() != FMISID_PARAM &&
            obsParameters[i].param.name() != WMO_PARAM &&
            obsParameters[i].param.name() != GEOID_PARAM &&
            obsParameters[i].param.name() != LPNN_PARAM &&
            obsParameters[i].param.name() != RWSID_PARAM &&
            obsParameters[i].param.name() != SENSOR_NO_PARAM &&
            obsParameters[i].param.name() != LONGITUDE_PARAM &&
            obsParameters[i].param.name() != LATITUDE_PARAM)
        {
          // handle possible empty fields to avoid spaces in
          // the beginning and end of output vector
          for (unsigned int k = 1; k < tsg->size(); k++)
          {
            ts::LonLatTimeSeries& llts = tsg->at(k);
            ts::TimeSeries& ts = llts.timeseries;
            ts::LonLatTimeSeries& lltsAt0 = tsg->at(0);
            ts::TimeSeries& tsAt0 = lltsAt0.timeseries;

            for (unsigned int j = 0; j < ts.size(); j++)
            {
              std::stringstream ss_val0;
              ss_val0 << tsAt0[j].value;
              std::stringstream ss_val;
              ss_val << ts[j].value;

              if (ss_val0.str().empty() && !ss_val.str().empty())
                tsAt0[j].value = ts[j].value;
            }
          }

          // delete all but first timeseries for parameters that are not location dependent, like
          // time
          tsg->erase(tsg->begin() + 1, tsg->end());

          // area name is replaced with the name given in URL
          if (loc && obsParameters[i].param.name() == NAME_PARAM)
          {
            ts::LonLatTimeSeries& llts = tsg->at(0);
            ts::TimeSeries& ts = llts.timeseries;

            BOOST_FOREACH (ts::TimedValue& tv, ts)
              tv.value = place;
          }
        }
      }

      Spine::ParameterFunctions pfunc = obsParameters[i].functions;
      // do the aggregation
      ts::TimeSeriesGroupPtr aggregated_tsg = ts::aggregate(*tsg, pfunc);

#ifdef MYDEBUG
      std::cout << "aggregated group#" << i << ": " << std::endl << *aggregated_tsg << std::endl;
#endif

      std::vector<TimeSeriesData> aggregatedData;

      // If all timesteps are requested or producer is syke or flash accept all timesteps
      if (query.toptions.all() || producer == FLASH_PRODUCER || producer == SYKE_PRODUCER)
      {
        Spine::TimeSeriesGenerator::LocalTimeList aggtimes;
        ts::TimeSeries ts = aggregated_tsg->at(0).timeseries;
        for (const ts::TimedValue& tv : ts)
          aggtimes.push_back(tv.time);
        // store observation data
        aggregatedData.push_back(
            TimeSeriesData(erase_redundant_timesteps(aggregated_tsg, aggtimes)));
        store_data(aggregatedData, query, outputData);
      }
      else
      {
        // Else accept only the original generated timesteps
        aggregatedData.push_back(TimeSeriesData(erase_redundant_timesteps(aggregated_tsg, *tlist)));
        // store observation data
        store_data(aggregatedData, query, outputData);
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
#endif

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION
void Plugin::fetchObsEngineValues(const State& state,
                                  const std::string& producer,
                                  const Spine::TaggedLocation& tloc,
                                  const ObsParameters& obsParameters,
                                  Engine::Observation::Settings& settings,
                                  Query& query,
                                  OutputData& outputData)
{
  try
  {
    if (settings.area_geoids.size() == 0 || producer == FLASH_PRODUCER)
    {
      // fetch data for places
      fetchObsEngineValuesForPlaces(
          state, producer, tloc, obsParameters, settings, query, outputData);
    }
    else
    {
      // fetch data for area
      fetchObsEngineValuesForArea(
          state, producer, tloc, obsParameters, settings, query, outputData);
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
#endif

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION
void Plugin::processObsEngineQuery(const State& state,
                                   Query& query,
                                   OutputData& outputData,
                                   const AreaProducers& areaproducers,
                                   const ProducerDataPeriod& producerDataPeriod,
                                   ObsParameters& obsParameters,
                                   Spine::PostGISDataSource& postGISDataSource)
{
  try
  {
    if (areaproducers.empty())
      throw Spine::Exception(BCP, "BUG: processObsEngineQuery producer list empty");

    std::string producer = *areaproducers.begin();

    // in contrast to area query
    bool locationQueryExecuted = false;

    // Settings which are the same for all locations

    Engine::Observation::Settings settings;
    setCommonObsSettings(settings,
                         producer,
                         producerDataPeriod,
                         state.getTime(),
                         obsParameters,
                         query,
                         postGISDataSource);

    if (query.loptions->locations().empty())
    {
      // This is not beautiful
      Spine::LocationPtr loc;
      Spine::TaggedLocation tloc("", loc);

      // Update settings for this particular location
      setLocationObsSettings(settings,
                             producer,
                             producerDataPeriod,
                             state.getTime(),
                             tloc,
                             obsParameters,
                             query,
                             postGISDataSource);

      // single locations are fetched all at once
      // area locations are fetched area by area
      if (locationQueryExecuted && settings.area_geoids.size() == 0)
      {
      }
      else
      {
        std::vector<TimeSeriesData> tsdatavector;
        outputData.push_back(make_pair("_obs_", tsdatavector));
        fetchObsEngineValues(state, producer, tloc, obsParameters, settings, query, outputData);
        // if(settings.area_geoids.size() == 0)
        // locationQueryExecuted = true;
      }
    }
    else
    {
      BOOST_FOREACH (const auto& tloc, query.loptions->locations())
      {
        // Update settings for this particular location
        setLocationObsSettings(settings,
                               producer,
                               producerDataPeriod,
                               state.getTime(),
                               tloc,
                               obsParameters,
                               query,
                               postGISDataSource);

        // single locations are fetched all at once
        // area locations are fetched area by area
        if (locationQueryExecuted && settings.area_geoids.size() == 0)
          continue;

        std::vector<TimeSeriesData> tsdatavector;
        outputData.push_back(make_pair("_obs_", tsdatavector));
        fetchObsEngineValues(state, producer, tloc, obsParameters, settings, query, outputData);

        if (settings.area_geoids.size() == 0)
          locationQueryExecuted = true;
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
#endif

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void Plugin::processQEngineQuery(const State& state,
                                 Query& masterquery,
                                 OutputData& outputData,
                                 const AreaProducers& areaproducers,
                                 const ProducerDataPeriod& producerDataPeriod,
                                 Spine::PostGISDataSource& postGISDataSource)
{
  try
  {
    // first timestep is here in utc
    boost::posix_time::ptime first_timestep = masterquery.latestTimestep;

    bool firstProducer = outputData.empty();

    BOOST_FOREACH (const auto& tloc, masterquery.loptions->locations())
    {
      Query query = masterquery;
      QueryLevelDataCache queryLevelDataCache;

      std::vector<TimeSeriesData> tsdatavector;
      outputData.push_back(make_pair(get_location_id(tloc.loc), tsdatavector));

      if (masterquery.timezone == LOCALTIME_PARAM)
        query.timezone = tloc.loc->timezone;

      query.toptions.startTime = first_timestep;
      if (!firstProducer)
        query.toptions.startTime += minutes(1);

      // producer can be alias, get actual producer
      std::string producer(select_producer(*itsQEngine, *(tloc.loc), query, areaproducers));
      bool isClimatologyProducer =
          (producer.empty() ? false : itsQEngine->getProducerConfig(producer).isclimatology);

      boost::local_time::local_date_time data_period_endtime(
          producerDataPeriod.getLocalEndTime(producer, query.timezone, getTimeZones()));

      // Reset for each new location, since fetchQEngineValues modifies it
      auto old_start_time = query.toptions.startTime;

      BOOST_FOREACH (const Spine::ParameterAndFunctions& paramfunc,
                     query.poptions.parameterFunctions())
      {
        // reset to original start time for each new location
        query.toptions.startTime = old_start_time;

        // every parameter starts from the same row
        if (query.toptions.endTime > data_period_endtime.local_time() &&
            !data_period_endtime.is_not_a_date_time() && !isClimatologyProducer)
        {
          query.toptions.endTime = data_period_endtime.local_time();
        }

        fetchQEngineValues(state,
                           paramfunc,
                           tloc,
                           query,
                           areaproducers,
                           producerDataPeriod,
                           postGISDataSource,
                           queryLevelDataCache,
                           outputData);
      }
      // get the latest_timestep from previous query
      masterquery.latestTimestep = query.latestTimestep;
      masterquery.lastpoint = query.lastpoint;
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void Plugin::processQuery(const State& state,
                          Spine::Table& table,
                          Query& masterquery,
                          Spine::PostGISDataSource& postGISDataSource)
{
  try
  {
    // if only location related parameters queried, use shortcut

    if (is_plain_location_query(masterquery.poptions.parameters()))
    {
      fetchLocationValues(masterquery, postGISDataSource, table, 0, 0);
      return;
    }

    ProducerDataPeriod producerDataPeriod;

// producerDataPeriod contains information of data periods of different producers
#ifndef WITHOUT_OBSERVATION
    producerDataPeriod.init(state, *itsQEngine, itsObsEngine, masterquery.timeproducers);
#else
    producerDataPeriod.init(state, *itsQEngine, masterquery.timeproducers);
#endif

    // the result data is stored here during the query
    OutputData outputData;

    bool producerMissing = (masterquery.timeproducers.empty());

    if (producerMissing)
    {
      masterquery.timeproducers.push_back(AreaProducers());
    }

#ifndef WITHOUT_OBSERVATION
    ObsParameters obsParameters = getObsParameters(masterquery);
#endif

    boost::posix_time::ptime latestTimestep = masterquery.latestTimestep;
    bool startTimeUTC = masterquery.toptions.startTimeUTC;

    // This loop will iterate through the producers, collecting as much
    // data in order as is possible. The later producers patch the data
    // *after* the first ones if possible.

    std::size_t producer_group = 0;
    BOOST_FOREACH (const AreaProducers& areaproducers, masterquery.timeproducers)
    {
      Query query = masterquery;

      query.timeproducers.clear();  // not used
      // set latestTimestep for the query
      query.latestTimestep = latestTimestep;

      if (producer_group != 0)
        query.toptions.startTimeUTC = startTimeUTC;
      query.toptions.endTimeUTC = masterquery.toptions.endTimeUTC;

#ifndef WITHOUT_OBSERVATION
      if (!areaproducers.empty() && !itsConfig.obsEngineDisabled() &&
          isObsProducer(areaproducers.front()))
      {
        processObsEngineQuery(state,
                              query,
                              outputData,
                              areaproducers,
                              producerDataPeriod,
                              obsParameters,
                              postGISDataSource);
      }
      else
#endif
      {
        processQEngineQuery(
            state, query, outputData, areaproducers, producerDataPeriod, postGISDataSource);
      }

      // get the latestTimestep from previous query
      latestTimestep = query.latestTimestep;
      startTimeUTC = query.toptions.startTimeUTC;
      ++producer_group;
    }

#ifndef WITHOUT_OBSERVATION
    // precision for observation special parameters', e.g. fmisid must be zero
    for (unsigned int i = 0; i < obsParameters.size(); i++)
    {
      std::string paramname(obsParameters[i].param.name());
      if (paramname == WMO_PARAM || paramname == LPNN_PARAM || paramname == FMISID_PARAM ||
          paramname == RWSID_PARAM || paramname == SENSOR_NO_PARAM || paramname == LEVEL_PARAM ||
          paramname == GEOID_PARAM)
        masterquery.precisions[i] = 0;
    }
#endif

    // insert data into the table
    fill_table(masterquery, outputData, table);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Perform a TimeSeries query
 */
// ----------------------------------------------------------------------

void Plugin::query(const State& state,
                   const Spine::HTTP::Request& request,
                   Spine::HTTP::Response& response)
{
  try
  {
    using namespace std;
    using namespace std::chrono;

    Spine::Table data;

    std::string timeheader;

    // Options
    high_resolution_clock::time_point t1 = high_resolution_clock::now();
    Query query(state, request, itsConfig);
    high_resolution_clock::time_point t2 = high_resolution_clock::now();

    string producer_option =
        Spine::optional_string(request.getParameter(PRODUCER_PARAM),
                               Spine::optional_string(request.getParameter(STATIONTYPE_PARAM), ""));

// At least one of location specifiers must be set

#ifndef WITHOUT_OBSERVATION
    if (query.geoids.size() == 0 && query.fmisids.size() == 0 && query.lpnns.size() == 0 &&
        query.wmos.size() == 0 && query.boundingBox.size() == 0 &&
        producer_option != FLASH_PRODUCER && query.loptions->locations().size() == 0)
#else
    if (query.loptions->locations().size() == 0)
#endif
      throw Spine::Exception(BCP, "No location option given!");

#ifdef MYDEBUG
    std::cout << query.loptions->locations().size() << " locations:" << std::endl;
    BOOST_FOREACH (const auto& tloc, query.loptions->locations())
      cout << formatLocation(*(tloc.loc)) << endl;
    std::cout << query.wmos.size() << " wmos:" << std::endl;
    for (const auto& wmo : query.wmos)
      std::cout << "\t" << wmo << std::endl;
#endif

    // The formatter knows which mimetype to send
    boost::shared_ptr<Spine::TableFormatter> formatter(
        Spine::TableFormatterFactory::create(query.format));
    string mime = formatter->mimetype() + "; charset=UTF-8";
    response.setHeader("Content-Type", mime.c_str());

    // Calculate the hash value for the product. Zero value implies
    // the product is not cacheable.

    auto product_hash = hash_value(state, query, request);

    high_resolution_clock::time_point t3 = high_resolution_clock::now();

    timeheader
        .append(
            Fmi::to_string(std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count()))
        .append("+")
        .append(
            Fmi::to_string(std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count()));

    if (product_hash != 0)
    {
      std::ostringstream os;
      os << std::hex << '"' << product_hash << "-timeseries\"";
      response.setHeader("ETag", os.str());

      // If the product is cacheable and etag was requested, respond with etag only

      if (request.getHeader("X-Request-ETag"))
      {
        response.setStatus(Spine::HTTP::Status::no_content);
        return;
      }

      // Else see if we have cached the result

      auto obj = itsCache->find(product_hash);
      if (obj)
      {
        response.setHeader("X-Duration", timeheader);
        response.setHeader("X-TimeSeries-Cache", "yes");
        response.setContent(obj);
        return;
      }
    }

    // Must generate the result from scratch

    response.setHeader("X-TimeSeries-Cache", "no");

    // No cached result available - generate the result

    processQuery(state, data, query, itsPostGISDataSource);

    high_resolution_clock::time_point t4 = high_resolution_clock::now();
    timeheader.append("+").append(
        Fmi::to_string(std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3).count()));

    data.setMissingText(query.valueformatter.missing());

    // The names of the columns
    Spine::TableFormatter::Names headers;
    BOOST_FOREACH (const Spine::Parameter& p, query.poptions.parameters())
    {
      headers.push_back(p.alias());
    }

    // Format product
    // Deduce WXML-tag from used producer. (What happens when we combine forecasts and
    // observations??).

    // If query is fast, we do not not have observation producers
    // This means we put 'forecast' into wxml-tag
    std::string wxml_type = "forecast";

    for (const auto& obsProducer : itsObsEngineStationTypes)
    {
      if (boost::algorithm::contains(producer_option, obsProducer))
      {
        // Observation mentioned, use 'observation' wxml type
        wxml_type = "observation";
        break;
      }
    }

    auto formatter_options = itsConfig.formatterOptions();
    formatter_options.setFormatType(wxml_type);

    ostringstream output;
    formatter->format(output, data, headers, request, formatter_options);
    high_resolution_clock::time_point t5 = high_resolution_clock::now();
    timeheader.append("+").append(
        Fmi::to_string(std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4).count()));

    // TODO: Should use std::move when it has become available
    boost::shared_ptr<std::string> result(new std::string());
    std::string tmp = output.str();
    std::swap(tmp, *result);

    if (result->size() == 0)
    {
      std::cerr << "Warning: Empty output for request " << request.getQueryString() << " from "
                << request.getClientIP() << std::endl;
    }

#ifdef MYDEBUG
    std::cout << "Output:" << std::endl << *result << std::endl;
#endif

    if (product_hash != 0)
      itsCache->insert(product_hash, result);

    response.setHeader("X-Duration", timeheader);
    response.setContent(*result);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Main content handler
 */
// ----------------------------------------------------------------------

void Plugin::requestHandler(Spine::Reactor& theReactor,
                            const Spine::HTTP::Request& theRequest,
                            Spine::HTTP::Response& theResponse)
{
  try
  {
    bool isdebug = ("debug" == Spine::optional_string(theRequest.getParameter("format"), ""));

    try
    {
      theResponse.setHeader("Access-Control-Allow-Origin", "*");

      auto expires_seconds = itsConfig.expirationTime();
      State state(*this);

      theResponse.setStatus(Spine::HTTP::Status::ok);

      query(state, theRequest, theResponse);  // may modify the status

      // Adding response headers

      boost::shared_ptr<Fmi::TimeFormatter> tformat(Fmi::TimeFormatter::create("http"));

      if (expires_seconds == 0)
      {
        theResponse.setHeader("Cache-Control", "no-cache, must-revalidate");  // HTTP/1.1
        theResponse.setHeader("Pragma", "no-cache");                          // HTTP/1.0
      }
      else
      {
        std::string cachecontrol = "public, max-age=" + Fmi::to_string(expires_seconds);
        theResponse.setHeader("Cache-Control", cachecontrol.c_str());

        ptime t_expires = state.getTime() + seconds(expires_seconds);
        std::string expiration = tformat->format(t_expires);
        theResponse.setHeader("Expires", expiration.c_str());
      }

      std::string modification = tformat->format(state.getTime());
      theResponse.setHeader("Last-Modified", modification.c_str());
    }
    catch (...)
    {
      // Catching all exceptions

      Spine::Exception exception(BCP, "Request processing exception!", NULL);
      exception.addParameter("URI", theRequest.getURI());
      exception.printError();

      if (isdebug)
      {
        // Delivering the exception information as HTTP content
        std::string fullMessage = exception.getHtmlStackTrace();
        theResponse.setContent(fullMessage);
        theResponse.setStatus(Spine::HTTP::Status::ok);
      }
      else
      {
        theResponse.setStatus(Spine::HTTP::Status::bad_request);
      }

      // Adding the first exception information into the response header

      std::string firstMessage = exception.what();
      boost::algorithm::replace_all(firstMessage, "\n", " ");
      firstMessage = firstMessage.substr(0, 300);
      theResponse.setHeader("X-TimeSeriesPlugin-Error", firstMessage.c_str());
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Plugin constructor
 */
// ----------------------------------------------------------------------

Plugin::Plugin(Spine::Reactor* theReactor, const char* theConfig)
    : SmartMetPlugin(),
      itsModuleName("TimeSeries"),
      itsConfig(theConfig),
      itsPostGISDataSource(),
      itsReady(false),
      itsReactor(theReactor),
#ifndef WITHOUT_OBSERVATION
      itsObsEngine(0),
#endif
      itsCache(),
      itsTimeSeriesCache()
{
  try
  {
    if (theReactor->getRequiredAPIVersion() != SMARTMET_API_VERSION)
    {
      std::cerr << "*** TimeSeriesPlugin and Server SmartMet API version mismatch ***" << std::endl;
      return;
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize in a separate thread due to PostGIS slowness
 */
// ----------------------------------------------------------------------

void Plugin::init()
{
  try
  {
    // Result cache
    itsCache.reset(new Spine::SmartMetCache(itsConfig.maxMemoryCacheSize(),
                                            itsConfig.maxFilesystemCacheSize(),
                                            itsConfig.filesystemCacheDirectory()));

    // Time series cache
    itsTimeSeriesCache.reset(new Spine::TimeSeriesGeneratorCache);
    itsTimeSeriesCache->resize(itsConfig.maxTimeSeriesCacheSize());

    // PostGIS
    std::vector<std::string> postgis_identifier_keys(itsConfig.getPostGISIdentifierKeys());

    for (unsigned int i = 0; i < postgis_identifier_keys.size(); i++)
    {
      if (itsShutdownRequested)
        return;

      Spine::postgis_identifier pgis_identifier(
          itsConfig.getPostGISIdentifier(postgis_identifier_keys[i]));
      std::string log_message;

      itsPostGISDataSource.readData(pgis_identifier, log_message);
    }

    /* GeoEngine */
    auto engine = itsReactor->getSingleton("Geonames", NULL);
    if (!engine)
      throw Spine::Exception(BCP, "Geonames engine unavailable");
    itsGeoEngine = reinterpret_cast<Engine::Geonames::Engine*>(engine);

    /* GisEngine */
    engine = itsReactor->getSingleton("Gis", NULL);
    if (!engine)
      throw Spine::Exception(BCP, "Gis engine unavailable");
    itsGisEngine = reinterpret_cast<Engine::Gis::Engine*>(engine);

    /* QEngine */
    engine = itsReactor->getSingleton("Querydata", NULL);
    if (!engine)
      throw Spine::Exception(BCP, "Querydata engine unavailable");
    itsQEngine = reinterpret_cast<Engine::Querydata::Engine*>(engine);

#ifndef WITHOUT_OBSERVATION
    if (!itsConfig.obsEngineDisabled())
    {
      /* ObsEngine */
      engine = itsReactor->getSingleton("Observation", NULL);
      if (!engine)
        throw Spine::Exception(BCP, "Observation engine unavailable");
      itsObsEngine = reinterpret_cast<Engine::Observation::Engine*>(engine);

      itsObsEngine->setGeonames(itsGeoEngine);
      // fetch obsebgine station types (producers)
      itsObsEngineStationTypes = itsObsEngine->getValidStationTypes();
    }
#endif

    // Initialization done, register services. We are aware that throwing
    // from a separate thread will cause a crash, but these should never
    // fail.
    if (!itsReactor->addContentHandler(this,
                                       itsConfig.defaultUrl(),
                                       boost::bind(&Plugin::callRequestHandler, this, _1, _2, _3)))
      throw Spine::Exception(BCP, "Failed to register timeseries content handler");

    // DEPRECATED:

    if (!itsReactor->addContentHandler(
            this, "/pointforecast", boost::bind(&Plugin::callRequestHandler, this, _1, _2, _3)))
      throw Spine::Exception(BCP, "Failed to register pointforecast content handler");

    // DONE
    itsReady = true;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Shutdown the plugin
 */
// ----------------------------------------------------------------------

void Plugin::shutdown()
{
  try
  {
    std::cout << "  -- Shutdown requested (timeseries)\n";
    if (itsCache != NULL)
      itsCache->shutdown();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Fast query deduction
 */
// ----------------------------------------------------------------------

bool Plugin::queryIsFast(const Spine::HTTP::Request& theRequest) const
{
  try
  {
    auto producers = theRequest.getParameterList("producer");
    if (producers.empty())
    {
      // No producers mentioned, default producer is forecast which is fast
      return true;
    }
    else
    {
      for (const auto& producer : producers)
      {
        for (const auto& obsProducer : itsObsEngineStationTypes)
        {
          if (boost::algorithm::contains(producer, obsProducer))
          {
            // Observation mentioned, query is slow
            return false;
          }
        }
      }

      // No observation producers mentioned, query is fast
      return true;
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Destructor
 */
// ----------------------------------------------------------------------

Plugin::~Plugin()
{
  try
  {
    if (itsCache != NULL)
      itsCache->shutdown();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the plugin name
 */
// ----------------------------------------------------------------------

const std::string& Plugin::getPluginName() const
{
  return itsModuleName;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the required version
 */
// ----------------------------------------------------------------------

int Plugin::getRequiredAPIVersion() const
{
  return SMARTMET_API_VERSION;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return true if the plugin has been initialized
 */
// ----------------------------------------------------------------------

bool Plugin::ready() const
{
  return itsReady;
}

#ifndef WITHOUT_OBSERVATION
bool Plugin::isObsProducer(const std::string& producer) const
{
  return (itsObsEngineStationTypes.find(producer) != itsObsEngineStationTypes.end());
}
#endif

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

/*
 * Server knows us through the 'SmartMetPlugin' virtual interface, which
 * the 'Plugin' class implements.
 */

extern "C" SmartMetPlugin* create(SmartMet::Spine::Reactor* them, const char* config)
{
  return new SmartMet::Plugin::TimeSeries::Plugin(them, config);
}

extern "C" void destroy(SmartMetPlugin* us)
{
  // This will call 'Plugin::~Plugin()' since the destructor is virtual
  delete us;
}

// ======================================================================
