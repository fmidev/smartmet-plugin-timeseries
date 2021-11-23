// ======================================================================
/*!
 * \brief SmartMet TimeSeries plugin implementation
 */
// ======================================================================

#include "Plugin.h"
#include "GridInterface.h"
#include "Hash.h"
#include "ImageFormatter.h"
#include "LocationTools.h"
#include "State.h"
#include <engines/gis/Engine.h>
#include <engines/observation/Keywords.h>
#include <fmt/format.h>
#include <gis/OGR.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/GraphFunctions.h>
#include <grid-files/common/ShowFunction.h>
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>
#include <newbase/NFmiIndexMaskTools.h>
#include <newbase/NFmiSvgTools.h>
#include <spine/Convenience.h>
#include <spine/ParameterTools.h>
#include <spine/SmartMet.h>
#include <spine/TableFormatterFactory.h>
#include <gis/CoordinateTransformation.h>

#define FUNCTION_TRACE FUNCTION_TRACE_OFF

using boost::local_time::local_date_time;
using boost::posix_time::hours;
using boost::posix_time::minutes;
using boost::posix_time::ptime;
using boost::posix_time::seconds;

//#define MYDEBUG ON

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
namespace
{
#ifdef MYDEBUG
void print_settings(const Engine::Observation::Settings& settings)
{
  std::cout << "settings.taggedFMISIDs: " << settings.taggedFMISIDs.size() << " kpl" << std::endl;
  std::cout << "settings.allplaces: " << settings.allplaces << std::endl;
  std::cout << "settings.boundingBox.size(): " << settings.boundingBox.size() << std::endl;
  std::cout << "settings.starttime: " << settings.starttime << std::endl;
  std::cout << "settings.endtime: " << settings.endtime << std::endl;
  std::cout << "settings.starttimeGiven: " << settings.starttimeGiven << std::endl;
  std::cout << "settings.format: " << settings.format << std::endl;
  std::cout << "settings.hours.size(): " << settings.hours.size() << std::endl;
  std::cout << "settings.language: " << settings.language << std::endl;
  std::cout << "settings.latest: " << settings.latest << std::endl;
  std::cout << "settings.localename: " << settings.localename << std::endl;
  std::cout << "settings.locale.name(): " << settings.locale.name() << std::endl;
  std::cout << "settings.maxdistance: " << settings.maxdistance << std::endl;
  std::cout << "settings.missingtext: " << settings.missingtext << std::endl;
  std::cout << "settings.taggedLocations.size(): " << settings.taggedLocations.size() << std::endl;
  std::cout << "settings.numberofstations: " << settings.numberofstations << std::endl;
  std::cout << "settings.parameters.size(): " << settings.parameters.size() << std::endl;
  std::cout << "settings.stationtype: " << settings.stationtype << std::endl;
  std::cout << "settings.timeformat: " << settings.timeformat << std::endl;
  std::cout << "settings.timestep: " << settings.timestep << std::endl;
  std::cout << "settings.timestring: " << settings.timestring << std::endl;
  std::cout << "settings.timezone: " << settings.timezone << std::endl;
  std::cout << "settings.weekdays.size(): " << settings.weekdays.size() << std::endl;
  std::string wktString = settings.wktArea;
  if (wktString.size() > 50)
  {
    wktString.resize(50);
    wktString += " ... ";
  }
  std::cout << "settings.wktArea: " << wktString << std::endl;
}
#endif


Spine::Parameter get_query_param(const Spine::Parameter& parameter)
{
  std::string paramname = parameter.name();
  std::string alias = parameter.alias();
  Spine::Parameter::Type type = parameter.type();
  FmiParameterName number = parameter.number();
  if(paramname == "x")
	{
	  paramname = "lon";
	  alias = "lon";
	  type = Spine::Parameter::Type::DataDerived;
	  number = kFmiLongitude;
	}
  else if(paramname == "y")
	{
	  paramname = "lat";
	  alias = "lat";
	  type = Spine::Parameter::Type::DataDerived;
	  number = kFmiLatitude;
	}

  return Spine::Parameter(paramname, alias, type, number);  
}

void transform_wgs84_coordinates(const std::string& name, const std::string& target_crs, const Spine::Location& loc, Spine::TimeSeries::TimeSeries& tseries)
{
  if(target_crs.empty() || target_crs == "EPSG:4326")
	return;

  double longitude = loc.longitude;
  double latitude = loc.latitude;
  Fmi::CoordinateTransformation transformation("WGS84", target_crs);
  transformation.transform(longitude, latitude);

  if(name == "x")
	{
	  for(auto& item : tseries)
		item.value = longitude;
	}
  else if(name == "y")
	{
	  for(auto& item : tseries)
		item.value = latitude;
	}
}

void transform_wgs84_coordinates(const std::string& name, const std::string& crs, Spine::TimeSeries::TimeSeriesGroup& tsg)
{
  for(auto& item : tsg)
	{
	  Spine::Location loc(item.lonlat.lon, item.lonlat.lat);
	  transform_wgs84_coordinates(name, crs, loc, item.timeseries);
	}
}

std::string get_parameter_id(const Spine::Parameter& parameter)
{
  try
  {
    std::string ret = parameter.name();
    if (parameter.getSensorNumber())
      ret += Fmi::to_string(*(parameter.getSensorNumber()));
    const auto& sensorParameter = parameter.getSensorParameter();
    if (sensorParameter == "qc")  // later maybe longitude, latitude
      ret += sensorParameter;
    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

bool is_mobile_producer(const std::string& producer)
{
  try
  {
    return (producer == SmartMet::Engine::Observation::ROADCLOUD_PRODUCER ||
            producer == SmartMet::Engine::Observation::TECONER_PRODUCER ||
            producer == SmartMet::Engine::Observation::FMI_IOT_PRODUCER ||
            producer == SmartMet::Engine::Observation::NETATMO_PRODUCER ||
            producer == SmartMet::Engine::Observation::BK_HYDROMETA_PRODUCER);
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

bool is_flash_producer(const std::string& producer)
{
  try
  {
    return (producer == FLASH_PRODUCER);
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

bool is_flash_or_mobile_producer(const std::string& producer)
{
  try
  {
    return (is_flash_producer(producer) || is_mobile_producer(producer));
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

std::unique_ptr<Spine::Location> get_bbox_location(const std::string& bbox_string,
                                                   const std::string& language,
                                                   const Engine::Geonames::Engine& geoengine)
{
  std::vector<std::string> parts;
  boost::algorithm::split(parts, bbox_string, boost::algorithm::is_any_of(","));

  double lon1 = Fmi::stod(parts[0]);
  double lat1 = Fmi::stod(parts[1]);
  double lon2 = Fmi::stod(parts[2]);
  double lat2 = Fmi::stod(parts[3]);

  // get location info for center coordinates
  double lon = (lon1 + lon2) / 2.0;
  double lat = (lat1 + lat2) / 2.0;

  return get_coordinate_location(lon, lat, language, geoengine);
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
    {
      return querydata.find(location.longitude,
                            location.latitude,
                            query.maxdistance,
                            use_data_max_distance,
                            query.leveltype);
    }

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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
      const auto& locationName = outputData[i].first;
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

        const auto& paramName = paramlist[j % numberOfParameters].name();
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
            tf.setCurrentColumn(k);
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    if (outputData.empty())
      return;

    ts::TableFeeder tf(table, query.valueformatter, query.precisions);
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

void add_missing_timesteps(ts::TimeSeries& ts,
                           const Spine::TimeSeriesGeneratorCache::TimeList& tlist)
{
  if (!tlist || tlist->empty())
    return;

  ts::TimeSeries ts2(ts.getLocalTimePool());

  SmartMet::Spine::TimeSeriesGenerator::LocalTimeList::const_iterator it = tlist->begin();

  for (const auto& value : ts)
  {
    // Add missing timesteps
    while (it != tlist->end() && *it < value.time)
    {
      ts2.emplace_back(ts::TimedValue(*it, ts::None()));
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
    ts2.emplace_back(ts::TimedValue(*it, ts::None()));
    ++it;
  }
  ts = ts2;
}

TimeSeriesByLocation get_timeseries_by_fmisid(
    const std::string& producer,
    const ts::TimeSeriesVectorPtr& observation_result,
    const Spine::TimeSeriesGeneratorCache::TimeList& tlist,
    int fmisid_index)

{
  try
  {
    TimeSeriesByLocation ret;

    if (is_flash_or_mobile_producer(producer))
    {
      ret.emplace_back(make_pair(0, observation_result));
      return ret;
    }

    // find fmisid time series
    const ts::TimeSeries& fmisid_ts = observation_result->at(fmisid_index);

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
      ts::TimeSeriesVectorPtr tsv(new ts::TimeSeriesVector());
      start_index = location_index.first;
      end_index = location_index.second;
      for (unsigned int k = 0; k < observation_result->size(); k++)
      {
        const ts::TimeSeries& ts_k = observation_result->at(k);
        if (ts_k.empty())
          tsv->push_back(ts_k);
        else
        {
          ts::TimeSeries ts_ik(fmisid_ts.getLocalTimePool());
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

#endif

// ----------------------------------------------------------------------
/*!
 * \brief Set precision of special parameters such as fmisid to zero
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION
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

Spine::TaggedLocationList get_locations_inside_geometry(const Spine::LocationList& locations,
                                                        const OGRGeometry& geom)
{
  try
  {
    Spine::TaggedLocationList ret;

    for (auto loc : locations)
    {
      std::string wkt =
          ("POINT(" + Fmi::to_string(loc->longitude) + " " + Fmi::to_string(loc->latitude) + ")");
      std::unique_ptr<OGRGeometry> location_geom = get_ogr_geometry(wkt);
      if (geom.Contains(location_geom.get()))
        ret.emplace_back(Spine::TaggedLocation(loc->name, loc));
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace

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

    for (const auto& name_value : request.getParameterMap())
    {
      const auto& name = name_value.first;
      if (name != "hour" && name != "time" && name != "timestep" && name != "timesteps" &&
          name != "starttime" && name != "startstep" && name != "endtime")
      {
        Fmi::hash_combine(hash, Fmi::hash_value(name_value.first));
        Fmi::hash_combine(hash, Fmi::hash_value(name_value.second));
      }
    }

    // If the query depends on locations only, that's it!

    if (is_plain_location_query(masterquery.poptions.parameters()))
      return hash;

#ifndef WITHOUT_OBSERVATION

    // Check here first if any of the producers is an observation.
    // If so, return zero

    for (const AreaProducers& areaproducers : masterquery.timeproducers)
    {
      for (const auto& areaproducer : areaproducers)
      {
        if (isObsProducer(areaproducer))
          return Fmi::bad_hash;
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
      masterquery.timeproducers.emplace_back(AreaProducers());
    }

#ifndef WITHOUT_OBSERVATION
    ObsParameters obsParameters = getObsParameters(masterquery);
#endif

    boost::posix_time::ptime latestTimestep = masterquery.latestTimestep;

    bool startTimeUTC = masterquery.toptions.startTimeUTC;

    // This loop will iterate through the producers.

    std::size_t producer_group = 0;
    bool firstProducer = true;

    for (const AreaProducers& areaproducers : masterquery.timeproducers)
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

        for (const auto& tloc : query.loptions->locations())
        {
          Query subquery = query;
          QueryLevelDataCache queryLevelDataCache;

          if (subquery.timezone == LOCALTIME_PARAM)
            subquery.timezone = tloc.loc->timezone;

          subquery.toptions.startTime = first_timestep;

          if (!firstProducer)
            subquery.toptions.startTime += boost::posix_time::minutes(1);  // WHY???????
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
            if (loc->type == Spine::Location::Wkt)
            {
              loc = masterquery.wktGeometries.getLocation(tloc.loc->name);
            }
            else if (loc->type == Spine::Location::Path || loc->type == Spine::Location::Area)
            {
              NFmiSvgPath svgPath;
              loc = getLocationForArea(tloc, query, &svgPath);
            }
            else if (loc->type == Spine::Location::BoundingBox)
            {
              // get location info for center coordinates
              std::unique_ptr<Spine::Location> tmp =
                  get_bbox_location(place, query.language, *itsGeoEngine);

              tmp->name = tloc.tag;
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
              Fmi::Exception ex(BCP, "No data available for '" + tloc.tag + "'!");
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
            Fmi::hash_combine(timeseries_hash, Fmi::hash_value(subquery.timezone));
            Fmi::hash_combine(timeseries_hash, subquery.toptions.hash_value());

            if (handled_timeseries.find(timeseries_hash) == handled_timeseries.end())
            {
              handled_timeseries.insert(timeseries_hash);

              const auto validtimes = qi->validTimes();
              if (validtimes->empty())
                throw Fmi::Exception(BCP, "Producer '" + producer + "' has no valid timesteps!");
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

              Fmi::hash_combine(hash, querydata_hash);
              Fmi::hash_combine(hash, Fmi::hash_value(*tlist));
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

Spine::LocationPtr Plugin::getLocationForArea(const Spine::TaggedLocation& tloc,
                                              const Query& query,
                                              NFmiSvgPath* svgPath /* = nullptr*/) const
{
  try
  {
    double bottom = 0.0, top = 0.0, left = 0.0, right = 0.0;

    const OGRGeometry* geom = get_ogr_geometry(tloc, itsGeometryStorage);

    if (geom)
    {
      OGREnvelope envelope;
      geom->getEnvelope(&envelope);
      top = envelope.MaxY;
      bottom = envelope.MinY;
      left = envelope.MinX;
      right = envelope.MaxX;
    }

    if (svgPath != nullptr)
    {
      get_svg_path(tloc, itsGeometryStorage, *svgPath);

      if (!geom)
      {
        // get location info for center coordinate
        bottom = svgPath->begin()->itsY;
        top = svgPath->begin()->itsY;
        left = svgPath->begin()->itsX;
        right = svgPath->begin()->itsX;

        for (const auto& element : *svgPath)
        {
          if (element.itsX < left)
            left = element.itsX;
          if (element.itsX > right)
            right = element.itsX;
          if (element.itsY < bottom)
            bottom = element.itsY;
          if (element.itsY > top)
            top = element.itsY;
        }
      }
    }

    double lon = (right + left) / 2.0;
    double lat = (top + bottom) / 2.0;
    std::unique_ptr<Spine::Location> tmp =
        get_coordinate_location(lon, lat, query.language, *itsGeoEngine);

    tmp->name = tloc.tag;
    tmp->type = tloc.loc->type;
    tmp->radius = tloc.loc->radius;

    return Spine::LocationPtr(tmp.release());
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

Spine::LocationPtr Plugin::getLocationForArea(const Spine::TaggedLocation& tloc,
                                              int radius,
                                              const Query& query,
                                              NFmiSvgPath* svgPath /* = nullptr*/) const
{
  FUNCTION_TRACE
  try
  {
    double bottom = 0.0, top = 0.0, left = 0.0, right = 0.0;

    const OGRGeometry* geom = get_ogr_geometry(tloc, itsGeometryStorage);
    std::string wktString;

    std::unique_ptr<OGRGeometry> expandedGeomUptr;
    if (geom && radius > 0)
    {
      auto* expandedGeom = Fmi::OGR::expandGeometry(geom, radius);
      expandedGeomUptr.reset(expandedGeom);
      wktString = Fmi::OGR::exportToWkt(*expandedGeom);
      geom = expandedGeom;
    }

    if (geom)
    {
      OGREnvelope envelope;
      geom->getEnvelope(&envelope);
      top = envelope.MaxY;
      bottom = envelope.MinY;
      left = envelope.MinX;
      right = envelope.MaxX;
    }

    if (svgPath != nullptr)
    {
      if (wktString.length() > 0)
      {
        convertWktMultipolygonToSvgPath(wktString, *svgPath);
      }
      else
      {
        get_svg_path(tloc, itsGeometryStorage, *svgPath);
      }

      if (!geom)
      {
        // get location info for center coordinate
        bottom = svgPath->begin()->itsY;
        top = svgPath->begin()->itsY;
        left = svgPath->begin()->itsX;
        right = svgPath->begin()->itsX;

        for (const auto& element : *svgPath)
        {
          if (element.itsX < left)
            left = element.itsX;
          if (element.itsX > right)
            right = element.itsX;
          if (element.itsY < bottom)
            bottom = element.itsY;
          if (element.itsY > top)
            top = element.itsY;
        }
      }
    }

    std::pair<double, double> lonlatCenter((right + left) / 2.0, (top + bottom) / 2.0);

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
    throw Fmi::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

Spine::TimeSeriesGenerator::LocalTimeList Plugin::generateQEngineQueryTimes(
    const Query& query, const std::string& paramname) const
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
    std::set<boost::local_time::local_date_time> qdtimesteps(tlist->begin(), tlist->end());

    // time frame is extended by aggregation interval
    boost::local_time::local_date_time timeseriesStartTime = *(tlist->begin());
    boost::local_time::local_date_time timeseriesEndTime = *(--tlist->end());
    timeseriesStartTime -= boost::posix_time::minutes(aggregationIntervalBehind);
    timeseriesEndTime += boost::posix_time::minutes(aggregationIntervalAhead);

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

      // generate timelist for aggregation
      tlist = itsTimeSeriesCache->generate(topt, tz);
      qdtimesteps.insert(tlist->begin(), tlist->end());
    }

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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void Plugin::fetchStaticLocationValues(Query& query,
                                       Spine::Table& data,
                                       unsigned int column_index,
                                       unsigned int row_index)
{
  try
  {
    unsigned int column = column_index;
    unsigned int row = row_index;

    for (const Spine::Parameter& param : query.poptions.parameters())
    {
      row = row_index;
      const auto& pname = param.name();
      for (const auto& tloc : query.loptions->locations())
      {
        Spine::LocationPtr loc = tloc.loc;
        if (loc->type == Spine::Location::Path || loc->type == Spine::Location::Area)
          loc = getLocationForArea(tloc, query);
        else if (loc->type == Spine::Location::Wkt)
          loc = query.wktGeometries.getLocation(tloc.loc->name);

        std::string val = location_parameter(
											 loc, pname, query.valueformatter, query.timezone, query.precisions[column], query.crs);
        data.set(column, row++, val);
      }
      column++;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Spine::LocationList get_indexmask_locations(const NFmiIndexMask& indexmask,
                                            const Spine::LocationPtr& loc,
                                            const SmartMet::Engine::Querydata::Q& qi,
                                            const Engine::Geonames::Engine& geoengine)
{
  Spine::LocationList loclist;

  for (const auto& mask : indexmask)
  {
    NFmiPoint coord = qi->latLon(mask);
    Spine::Location location(*loc);
    location.longitude = coord.X();
    location.latitude = coord.Y();
    location.dem = geoengine.demHeight(location.longitude, location.latitude);
    location.covertype = geoengine.coverType(location.longitude, location.latitude);
    location.type = Spine::Location::CoordinatePoint;
    Spine::LocationPtr locPtr = boost::make_shared<Spine::Location>(location);
    loclist.emplace_back(locPtr);
  }

  return loclist;
}

// If query.groupareas is false, find out locations inside area and process them individaually
void Plugin::resolveAreaLocations(Query& query,
                                  const State& state,
                                  const AreaProducers& areaproducers)
{
  if (query.groupareas == true)
    return;

  Spine::TaggedLocationList tloclist;
  for (const auto& tloc : query.loptions->locations())
  {
    Spine::LocationPtr loc = tloc.loc;
    Spine::LocationPtr area_loc;
    if (tloc.loc->type == Spine::Location::BoundingBox)
      area_loc = get_bbox_location(get_name_base(tloc.loc->name), query.language, *itsGeoEngine);
    else
      area_loc = getLocationForArea(tloc, query);
    auto producer = select_producer(*itsQEngine, *area_loc, query, areaproducers);
    if (producer.empty())
      return;
    auto qi = (query.origintime ? state.get(producer, *query.origintime) : state.get(producer));

    if (qi->isGrid() &&
        (loc->type == Spine::Location::BoundingBox || loc->type == Spine::Location::Area ||
         ((loc->type == Spine::Location::Place || loc->type == Spine::Location::CoordinatePoint) &&
          loc->radius > 0)))
    {
      NFmiSvgPath svgPath;
      if (loc->type == Spine::Location::Wkt)
        svgPath = query.wktGeometries.getSvgPath(tloc.loc->name);
      else if (loc->type == Spine::Location::Path || loc->type == Spine::Location::Area)
        getLocationForArea(tloc, query, &svgPath);

      NFmiIndexMask indexmask;
      if (loc->type == Spine::Location::BoundingBox)
      {
        std::vector<std::string> coordinates;
        std::string place = get_name_base(loc->name);
        boost::algorithm::split(coordinates, place, boost::algorithm::is_any_of(","));
        if (coordinates.size() != 4)
          throw Fmi::Exception(BCP,
                               "Invalid bbox parameter " + place +
                                   ", should be in format 'lon,lat,lon,lat[:radius]'!");

        std::string lonstr1 = coordinates[0];
        std::string latstr1 = coordinates[1];
        std::string lonstr2 = coordinates[2];
        std::string latstr2 = coordinates[3];

        if (latstr2.find(':') != std::string::npos)
          latstr2.erase(latstr2.begin() + latstr2.find(':'), latstr2.end());

        double lon1 = Fmi::stod(lonstr1);
        double lat1 = Fmi::stod(latstr1);
        double lon2 = Fmi::stod(lonstr2);
        double lat2 = Fmi::stod(latstr2);

        NFmiSvgPath boundingBoxPath;
        NFmiSvgTools::BBoxToSvgPath(boundingBoxPath, lon1, lat1, lon2, lat2);
        indexmask = NFmiIndexMaskTools::MaskExpand(qi->grid(), boundingBoxPath, loc->radius);
      }
      else if (loc->type == Spine::Location::Area || loc->type == Spine::Location::Place ||
               loc->type == Spine::Location::CoordinatePoint)
      {
        if (loc->type != Spine::Location::Wkt)  // SVG for WKT has been extracted earlier
          get_svg_path(tloc, itsGeometryStorage, svgPath);
        indexmask = NFmiIndexMaskTools::MaskExpand(qi->grid(), svgPath, loc->radius);
      }

      for (const auto& mask : indexmask)
      {
        NFmiPoint coord = qi->latLon(mask);
        Spine::Location location(*area_loc);
        location.longitude = coord.X();
        location.latitude = coord.Y();
        location.dem = itsGeoEngine->demHeight(location.longitude, location.latitude);
        location.covertype = itsGeoEngine->coverType(location.longitude, location.latitude);
        location.type = Spine::Location::CoordinatePoint;
        Spine::LocationPtr locPtr = boost::make_shared<Spine::Location>(location);
        Spine::TaggedLocation new_tloc(tloc.tag, locPtr);
        tloclist.emplace_back(new_tloc);
      }
    }
    else
    {
      tloclist.emplace_back(tloc);
    }
  }

  query.loptions->setLocations(tloclist);
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
                                QueryLevelDataCache& queryLevelDataCache,
                                OutputData& outputData)

{
  try
  {
    Spine::LocationPtr loc = tloc.loc;
    std::string place = get_name_base(loc->name);
    std::string paramname = paramfunc.parameter.name();

    NFmiSvgPath svgPath;
    bool isWkt = false;
    if (loc->type == Spine::Location::Wkt)
    {
      loc = query.wktGeometries.getLocation(tloc.loc->name);
      svgPath = query.wktGeometries.getSvgPath(tloc.loc->name);
      isWkt = true;
    }
    else if (loc->type == Spine::Location::Path || loc->type == Spine::Location::Area)
    {
      loc = getLocationForArea(tloc, query, &svgPath);
    }
    else if (loc->type == Spine::Location::BoundingBox)
    {
      // get location info for center coordinate
      std::unique_ptr<Spine::Location> tmp =
          get_bbox_location(place, query.language, *itsGeoEngine);

      tmp->name = tloc.tag;
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
      Fmi::Exception ex(BCP, "No data available for " + place);
      ex.disableLogging();
      throw ex;
    }

    auto qi = (query.origintime ? state.get(producer, *query.origintime) : state.get(producer));

    const auto validtimes = qi->validTimes();
    if (validtimes->empty())
      throw Fmi::Exception(BCP, "Producer " + producer + " has no valid time steps");
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

    std::string country = state.getGeoEngine().countryName(loc->iso2, query.language);

    std::vector<TimeSeriesData> aggregatedData;  // store here data of all levels

    // If no pressures/heights are chosen, loading all or just chosen data levels.
    //
    // Otherwise loading chosen data levels if any, then the chosen pressure
    // and/or height levels (interpolated values at chosen pressures/heights)

    bool loadDataLevels =
        (!query.levels.empty() || (query.pressures.empty() && query.heights.empty()));
    std::string levelType(loadDataLevels ? "data:" : "");
    auto itPressure = query.pressures.begin();
    auto itHeight = query.heights.begin();

    // Loop over the levels
    for (qi->resetLevel();;)
    {
      boost::optional<float> pressure, height;
      float levelValue = 0;

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

      if (tlist.empty())
        return;

#ifdef BRAINSTORM_1195
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
#endif

      auto querydata_tlist = generateQEngineQueryTimes(query, paramname);

#ifdef BRAINSTORM_1195
      // restore original timestep
      query.toptions.timeSteps = timeStepOriginal;
#endif

      ts::Value missing_value = Spine::TimeSeries::None();

#ifdef MYDEBUG
      std::cout << std::endl << "producer: " << producer << std::endl;
      std::cout << "data period start time: "
                << producerDataPeriod.getLocalStartTime(producer, query.timezone, getTimeZones())
                << std::endl;
      std::cout << "data period end time: "
                << producerDataPeriod.getLocalEndTime(producer, query.timezone, getTimeZones())
                << std::endl;
      std::cout << "paramname: " << paramname << std::endl;
      std::cout << "query.timezone: " << query.timezone << std::endl;
      std::cout << query.toptions;

      std::cout << "generated timesteps: " << std::endl;
      for (const boost::local_time::local_date_time& ldt : tlist)
      {
        std::cout << ldt << std::endl;
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
        else if (paramname == "fmisid" || paramname == "lpnn" || paramname == "wmo")
        {
          querydata_result = boost::make_shared<ts::TimeSeries>(state.getLocalTimePool());
          for (const auto& t : querydata_tlist)
          {
            if (loc->fmisid && paramname == "fmisid")
            {
              querydata_result->emplace_back(ts::TimedValue(t, *(loc->fmisid)));
            }
            else
            {
              querydata_result->emplace_back(ts::TimedValue(t, ts::None()));
            }
          }
        }
        else
        {
		  Spine::Parameter param = get_query_param(paramfunc.parameter);

          Engine::Querydata::ParameterOptions querydata_param(param,
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
                                                              query.lastpoint,
                                                              state.getLocalTimePool());

          // one location, list of local times (no radius -> pointforecast)
          querydata_result = loadDataLevels ? qi->values(querydata_param, querydata_tlist)
                             : pressure
                                 ? qi->valuesAtPressure(querydata_param, querydata_tlist, *pressure)
                                 : qi->valuesAtHeight(querydata_param, querydata_tlist, *height);

          if (querydata_result->size() > 0)
          {
			if(paramfunc.parameter.name() == "x" || paramfunc.parameter.name() == "y")
			  transform_wgs84_coordinates(paramfunc.parameter.name(), query.crs, *loc, *querydata_result);

            queryLevelDataCache.itsTimeSeries.insert(make_pair(cacheKey, querydata_result));
          }
        }

        aggregatedData.emplace_back(TimeSeriesData(DataFunctions::erase_redundant_timesteps(
            DataFunctions::aggregate(querydata_result, paramfunc.functions), tlist)));
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
            Spine::LocationList llist;

            if (isWkt)
            {
              OGRwkbGeometryType geomType =
                  query.wktGeometries.getGeometry(tloc.loc->name)->getGeometryType();
              if (geomType == wkbMultiLineString)
              {
                // OGRMultiLineString -> handle each LineString separately
                std::list<NFmiSvgPath> svgList = query.wktGeometries.getSvgPaths(tloc.loc->name);
                for (const auto& svg : svgList)
                {
                  Spine::LocationList ll =
                      get_location_list(svg, tloc.tag, query.step, state.getGeoEngine());
                  if (ll.size() > 0)
                    llist.insert(llist.end(), ll.begin(), ll.end());
                }
              }
              else if (geomType == wkbMultiPoint)
              {
                llist = query.wktGeometries.getLocations(tloc.loc->name);
              }
              else if (geomType == wkbLineString)
              {
                // For LineString svgPath has been extracted earlier
                llist = get_location_list(svgPath, tloc.tag, query.step, state.getGeoEngine());
              }
            }
            else
            {
              llist = get_location_list(svgPath, tloc.tag, query.step, state.getGeoEngine());
            }

			Spine::Parameter param = get_query_param(paramfunc.parameter);

            Engine::Querydata::ParameterOptions querydata_param(param,
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
                                                                query.lastpoint,
                                                                state.getLocalTimePool());

            // list of locations, list of local times
            querydata_result =
                loadDataLevels
                    ? qi->values(querydata_param, llist, querydata_tlist, query.maxdistance)
                : pressure
                    ? qi->valuesAtPressure(
                          querydata_param, llist, querydata_tlist, query.maxdistance, *pressure)
                    : qi->valuesAtHeight(
                          querydata_param, llist, querydata_tlist, query.maxdistance, *height);
            if (querydata_result->size() > 0)
            {
              // if the value is not dependent on location inside area
              // we just need to have the first one
              if (!parameter_is_arithmetic(paramfunc.parameter))
              {
                auto dataIndependentValue = querydata_result->at(0);
                querydata_result->clear();
                querydata_result->push_back(dataIndependentValue);
              }

			  if(paramfunc.parameter.name() == "x" || paramfunc.parameter.name() == "y")
				transform_wgs84_coordinates(paramfunc.parameter.name(), query.crs, *querydata_result);

              queryLevelDataCache.itsTimeSeriesGroups.insert(make_pair(cacheKey, querydata_result));
            }
          }
          else if (qi->isGrid() &&
                   (loc->type == Spine::Location::BoundingBox ||
                    loc->type == Spine::Location::Area || loc->type == Spine::Location::Place ||
                    loc->type == Spine::Location::CoordinatePoint))
          {
            NFmiIndexMask mask;

            if (loc->type == Spine::Location::BoundingBox)
            {
              std::vector<std::string> coordinates;
              boost::algorithm::split(coordinates, place, boost::algorithm::is_any_of(","));
              if (coordinates.size() != 4)
                throw Fmi::Exception(BCP,
                                     "Invalid bbox parameter " + place +
                                         ", should be in format 'lon,lat,lon,lat[:radius]'!");

              std::string lonstr1 = coordinates[0];
              std::string latstr1 = coordinates[1];
              std::string lonstr2 = coordinates[2];
              std::string latstr2 = coordinates[3];

              if (latstr2.find(':') != std::string::npos)
                latstr2.erase(latstr2.begin() + latstr2.find(':'), latstr2.end());

              double lon1 = Fmi::stod(lonstr1);
              double lat1 = Fmi::stod(latstr1);
              double lon2 = Fmi::stod(lonstr2);
              double lat2 = Fmi::stod(latstr2);

              NFmiSvgPath boundingBoxPath;
              NFmiSvgTools::BBoxToSvgPath(boundingBoxPath, lon1, lat1, lon2, lat2);
              mask = NFmiIndexMaskTools::MaskExpand(qi->grid(), boundingBoxPath, loc->radius);
            }
            else if (loc->type == Spine::Location::Area || loc->type == Spine::Location::Place ||
                     loc->type == Spine::Location::CoordinatePoint)
            {
              if (!isWkt)  // SVG for WKT has been extracted earlier
                get_svg_path(tloc, itsGeometryStorage, svgPath);
              mask = NFmiIndexMaskTools::MaskExpand(qi->grid(), svgPath, loc->radius);
            }

			Spine::Parameter param = get_query_param(paramfunc.parameter);

            Engine::Querydata::ParameterOptions querydata_param(param,
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
                                                                query.lastpoint,
                                                                state.getLocalTimePool());

            // Indexmask (indexed locations on the area)
            Spine::LocationList llist = get_indexmask_locations(mask, loc, qi, *itsGeoEngine);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
            querydata_result =
                loadDataLevels
                    ? qi->values(querydata_param, llist, querydata_tlist, query.maxdistance)
                : pressure
                    ? qi->valuesAtPressure(
                          querydata_param, llist, querydata_tlist, query.maxdistance, *pressure)
                    : qi->valuesAtHeight(
                          querydata_param, llist, querydata_tlist, query.maxdistance, *height);
#pragma GCC diagnostic pop

            if (querydata_result->size() > 0)
            {
              // if the value is not dependent on location inside
              // area we just need to have the first one
              if (!parameter_is_arithmetic(paramfunc.parameter))
              {
                auto dataIndependentValue = querydata_result->at(0);
                querydata_result->clear();
                querydata_result->push_back(dataIndependentValue);
              }

			  if(paramfunc.parameter.name() == "x" || paramfunc.parameter.name() == "y")
				transform_wgs84_coordinates(paramfunc.parameter.name(), query.crs, *querydata_result);

              queryLevelDataCache.itsTimeSeriesGroups.insert(make_pair(cacheKey, querydata_result));
            }
          }
        }  // area handling

        if (querydata_result->size() > 0)
          aggregatedData.emplace_back(TimeSeriesData(DataFunctions::erase_redundant_timesteps(
              DataFunctions::aggregate(querydata_result, paramfunc.functions), tlist)));
      }
    }  // levels

    // store level-data
    DataFunctions::store_data(aggregatedData, query, outputData);
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
    for (const auto& areaproducers : query.timeproducers)
    {
      for (const auto& producer : areaproducers)
      {
        if (stationTypes.find(producer) != stationTypes.end())
        {
          std::map<std::string, unsigned int> parameter_columns;
          unsigned int column_index = 0;
          for (const Spine::ParameterAndFunctions& paramfuncs : query.poptions.parameterFunctions())
          {
            Spine::Parameter parameter(paramfuncs.parameter);

            std::string parameter_id = get_parameter_id(parameter);

            if (parameter_columns.find(parameter_id) != parameter_columns.end())
            {
              ret.emplace_back(ObsParameter(
                  parameter, paramfuncs.functions, parameter_columns.at(parameter_id), true));
            }
            else
            {
              ret.emplace_back(ObsParameter(parameter, paramfuncs.functions, column_index, false));
              parameter_columns.insert(make_pair(parameter_id, column_index));
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
#endif

// ----------------------------------------------------------------------
/*!
 * \brief Set obsegnine settings common for all queries
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION
void Plugin::getCommonObsSettings(Engine::Observation::Settings& settings,
                                  const std::string& producer,
                                  Query& query) const
{
  try
  {
    // At least one of location specifiers must be set
    if (is_flash_producer(producer))
    {
      settings.boundingBox = query.boundingBox;
      settings.taggedLocations = query.loptions->locations();
    }

    // Below are listed optional settings, defaults are set while constructing an ObsEngine::Oracle
    // instance.

    // TODO Because timezone="localtime" functions differently in observation,
    // force default timezone to be Europe/Helsinki. This must be fixed when obsplugin is obsoleted
    if (query.timezone == "localtime")
      query.timezone = "Europe/Helsinki";
    settings.timezone = (query.timezone == LOCALTIME_PARAM ? UTC_PARAM : query.timezone);

    settings.format = query.format;
    settings.stationtype = producer;
    if (producer == SmartMet::Engine::Observation::FMI_IOT_PRODUCER)
      settings.stationtype_specifier = query.iot_producer_specifier;
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
    settings.useDataCache = query.useDataCache;
    // Data filtering settings
    settings.dataFilter = query.dataFilter;
    // Option to prevent queries to database
    settings.preventDatabaseQuery = itsConfig.obsEngineDatabaseQueryPrevented();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
#endif

// ----------------------------------------------------------------------
/*!
 * \brief Update settings for a specific location
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION

bool Plugin::resolveAreaStations(const Spine::LocationPtr& location,
                                 const std::string& producer,
                                 Query& query,
                                 Engine::Observation::Settings& settings,
                                 std::string& name) const
{
  try
  {
    if (!location)
      return false;

    std::string loc_name_original = location->name;
    std::string loc_name = (get_name_base(location->name) + query.areasource);

    Spine::LocationPtr loc = location;
    bool isWkt = (loc->type == Spine::Location::Wkt);

    if (isWkt)
      loc = query.wktGeometries.getLocation(loc_name_original);

    // Area handling:
    // 1) Get OGRGeometry object, expand it with radius
    // 2) Fetch stations fmisids from ObsEngine located inside the area
    // 3) Fetch location info from GeoEngine. The info will be used in
    // responses to location and time related queries
    // 4) Fetch the observations from ObsEngine using the geoids
    if (loc)
    {
      Engine::Observation::StationSettings stationSettings;

#ifdef MYDEBUG
      std::cout << loc_name << " is a Path" << std::endl;
#endif

      std::string wktString;
      if (loc->type == Spine::Location::Path)
      {
        const OGRGeometry* pGeo = nullptr;
        // if no radius has been given use 200 meters
        double radius = (loc->radius == 0 ? 200 : loc->radius * 1000);

#ifdef MYDEBUG
        std::cout << loc_name << " is a Path" << std::endl;
#endif

        if (isWkt)
        {
          pGeo = query.wktGeometries.getGeometry(loc_name_original);
          wktString = Fmi::OGR::exportToWkt(*pGeo);
        }
        else
        {
          if (loc_name.find(',') != std::string::npos)
          {
            std::vector<std::string> parts;
            boost::algorithm::split(parts, loc_name, boost::algorithm::is_any_of(","));
            if (parts.size() % 2)
              throw Fmi::Exception(
                  BCP,
                  "Path " + loc_name + "is invalid, because it has odd number of coordinates!");

            std::string wkt = "LINESTRING(";
            for (unsigned int i = 0; i < parts.size(); i += 2)
            {
              if (i > 0)
                wkt += ", ";
              wkt += parts[i];
              wkt += " ";
              wkt += parts[i + 1];
            }
            wkt += ")";

            std::unique_ptr<OGRGeometry> geom = get_ogr_geometry(wkt, loc->radius);
            wktString = Fmi::OGR::exportToWkt(*geom);
          }
          else
          {
            // path is fetched from database
            pGeo = itsGeometryStorage.getOGRGeometry(loc_name, wkbMultiLineString);

            if (!pGeo)
              throw Fmi::Exception(BCP, "Path " + loc_name + " not found in PostGIS database!");

            std::unique_ptr<OGRGeometry> poly;
            poly.reset(Fmi::OGR::expandGeometry(pGeo, radius));
            wktString = Fmi::OGR::exportToWkt(*poly);
          }
        }
        if (!is_flash_or_mobile_producer(producer))
          stationSettings.fmisids = get_fmisids_for_wkt(
              itsObsEngine, producer, settings.starttime, settings.endtime, wktString);

        query.maxdistance = 0;
      }
      else if (loc->type == Spine::Location::Area)
      {
#ifdef MYDEBUG
        std::cout << loc_name << " is an Area" << std::endl;
#endif

        const OGRGeometry* pGeo = nullptr;

        if (isWkt)
        {
          pGeo = query.wktGeometries.getGeometry(loc_name_original);

          if (!pGeo)
            throw Fmi::Exception(BCP, "Area " + loc_name + " is not a WKT geometry!");

          wktString = Fmi::OGR::exportToWkt(*pGeo);
        }
        else
        {
          pGeo = itsGeometryStorage.getOGRGeometry(loc_name, wkbPolygon);
          if (pGeo == nullptr)
            pGeo = itsGeometryStorage.getOGRGeometry(loc_name, wkbMultiPolygon);

          if (!pGeo)
            throw Fmi::Exception(BCP, "Area " + loc_name + " not found in PostGIS database!");

          std::unique_ptr<OGRGeometry> poly;
          poly.reset(Fmi::OGR::expandGeometry(pGeo, loc->radius * 1000));

          wktString = Fmi::OGR::exportToWkt(*poly);
        }

        if (!is_flash_or_mobile_producer(producer))
          stationSettings.fmisids = get_fmisids_for_wkt(
              itsObsEngine, producer, settings.starttime, settings.endtime, wktString);
        query.maxdistance = 0;
      }
      else if (loc->type == Spine::Location::BoundingBox && !is_flash_producer(producer))
      {
#ifdef MYDEBUG
        std::cout << loc_name << " is a BoundingBox" << std::endl;
#endif

        Spine::BoundingBox bbox(loc_name);

        NFmiSvgPath svgPath;
        NFmiSvgTools::BBoxToSvgPath(svgPath, bbox.xMin, bbox.yMin, bbox.xMax, bbox.yMax);

        std::string wkt;
        for (const auto& element : svgPath)
        {
          if (wkt.empty())
            wkt += "POLYGON((";
          else
            wkt += ", ";
          wkt += Fmi::to_string(element.itsX);
          wkt += " ";
          wkt += Fmi::to_string(element.itsY);
        }
        if (!wkt.empty())
          wkt += "))";

        std::unique_ptr<OGRGeometry> geom = get_ogr_geometry(wkt, loc->radius);
        wktString = Fmi::OGR::exportToWkt(*geom);
        if (!is_flash_or_mobile_producer(producer))
          stationSettings.fmisids = get_fmisids_for_wkt(
              itsObsEngine, producer, settings.starttime, settings.endtime, wktString);

        query.maxdistance = 0;
      }
      else if (!is_flash_producer(producer) && loc->type == Spine::Location::Place &&
               loc->radius > 0)
      {
#ifdef MYDEBUG
        std::cout << loc_name << " is an Area (Place + radius)" << std::endl;
#endif

        std::string wkt = "POINT(";
        wkt += Fmi::to_string(loc->longitude);
        wkt += " ";
        wkt += Fmi::to_string(loc->latitude);
        wkt += ")";

        std::unique_ptr<OGRGeometry> geom = get_ogr_geometry(wkt, loc->radius);
        wktString = Fmi::OGR::exportToWkt(*geom);
        if (!is_flash_or_mobile_producer(producer))
          stationSettings.fmisids = get_fmisids_for_wkt(
              itsObsEngine, producer, settings.starttime, settings.endtime, wktString);
        query.maxdistance = 0;
      }
      else if (!is_flash_producer(producer) && loc->type == Spine::Location::CoordinatePoint &&
               loc->radius > 0)
      {
#ifdef MYDEBUG
        std::cout << loc_name << " is an Area (coordinate point + radius)" << std::endl;
#endif

        if (isWkt)
        {
          const OGRGeometry* pGeo = query.wktGeometries.getGeometry(loc_name_original);

          if (!pGeo)
            throw Fmi::Exception(BCP, "Area " + loc_name + " is not a WKT geometry!");

          wktString = Fmi::OGR::exportToWkt(*pGeo);
        }
        else
        {
          std::string wkt = "POINT(";
          wkt += Fmi::to_string(loc->longitude);
          wkt += " ";
          wkt += Fmi::to_string(loc->latitude);
          wkt += ")";

          std::unique_ptr<OGRGeometry> geom = get_ogr_geometry(wkt, loc->radius);
          wktString = Fmi::OGR::exportToWkt(*geom);
        }

        if (!is_flash_or_mobile_producer(producer))
          stationSettings.fmisids = get_fmisids_for_wkt(
              itsObsEngine, producer, settings.starttime, settings.endtime, wktString);
        query.maxdistance = 0;
      }

#ifdef MYDEBUG
      std::cout << "WKT of buffered area: " << std::endl << wktString << std::endl;
      std::cout << "#" << stationSettings.fmisids.size() << " stations found" << std::endl;
      for (auto fmisid : stationSettings.fmisids)
        std::cout << "fmisid: " << fmisid << std::endl;
#endif

      if (is_flash_or_mobile_producer(producer) && !wktString.empty())
      {
        settings.wktArea = wktString;
      }
      else if (stationSettings.fmisids.size() > 0)
      {
        settings.taggedFMISIDs = itsObsEngine->translateToFMISID(
            settings.starttime, settings.endtime, producer, stationSettings);

        name = loc->name;
        if (loc->type == Spine::Location::Wkt)
          name = query.wktGeometries.getName(name);
        else if (name.find(" as ") != std::string::npos)
          name = name.substr(name.find(" as ") + 4);

        return (!settings.taggedFMISIDs.empty());
      }
    }  // if(loc)

    return false;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void Plugin::resolveParameterSettings(const ObsParameters& obsParameters,
                                      const Query& query,
                                      const std::string& producer,
                                      Engine::Observation::Settings& settings,
                                      unsigned int& aggregationIntervalBehind,
                                      unsigned int& aggregationIntervalAhead) const
{
  try
  {
    int fmisid_index = -1;

    for (const auto& obsparam : obsParameters)
    {
      const Spine::Parameter& param = obsparam.param;
      const auto& pname = param.name();

      if (query.maxAggregationIntervals.find(pname) != query.maxAggregationIntervals.end())
      {
        if (query.maxAggregationIntervals.at(pname).behind > aggregationIntervalBehind)
          aggregationIntervalBehind = query.maxAggregationIntervals.at(pname).behind;
        if (query.maxAggregationIntervals.at(pname).ahead > aggregationIntervalAhead)
          aggregationIntervalAhead = query.maxAggregationIntervals.at(pname).ahead;
      }

      // prevent passing duplicate parameters to observation (for example temperature,
      // max_t(temperature))
      // location parameters are handled in timeseries plugin
      if (obsparam.duplicate ||
          (SmartMet::Spine::is_location_parameter(pname) &&
           !is_flash_or_mobile_producer(producer)) ||
          SmartMet::Spine::is_time_parameter(pname))
        continue;

      // fmisid must be always included (except for flash) in queries in order to get location
      // info from geonames
      if (pname == FMISID_PARAM && !is_flash_or_mobile_producer(producer))
        fmisid_index = settings.parameters.size();

      // all parameters are fetched at once
      settings.parameters.push_back(param);
    }

    // fmisid must be always included in order to get thelocation info from geonames
    if (fmisid_index == -1 && !is_flash_or_mobile_producer(producer))
    {
      Spine::Parameter fmisidParam =
          Spine::Parameter(FMISID_PARAM, Spine::Parameter::Type::DataIndependent);
      settings.parameters.push_back(fmisidParam);
    }
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void Plugin::resolveTimeSettings(const std::string& producer,
                                 const ProducerDataPeriod& producerDataPeriod,
                                 const boost::posix_time::ptime& now,
                                 Query& query,
                                 unsigned int aggregationIntervalBehind,
                                 unsigned int aggregationIntervalAhead,
                                 Engine::Observation::Settings& settings) const
{
  try
  {
    // Below are listed optional settings, defaults are set while constructing an
    // ObsEngine::Oracle instance.

    boost::local_time::time_zone_ptr tz = getTimeZones().time_zone_from_string(query.timezone);
    boost::local_time::local_date_time ldt_now(now, tz);
    boost::posix_time::ptime ptime_now =
        (query.toptions.startTimeUTC ? ldt_now.utc_time() : ldt_now.local_time());

    if (query.toptions.startTimeData)
    {
      query.toptions.startTime = ptime_now - boost::posix_time::hours(24);
      query.toptions.startTimeData = false;
    }
    if (query.toptions.endTimeData)
    {
      query.toptions.endTime = ptime_now;
      query.toptions.endTimeData = false;
    }

    if (query.toptions.startTime > ptime_now)
      query.toptions.startTime = ptime_now;
    if (query.toptions.endTime > ptime_now)
      query.toptions.endTime = ptime_now;

    if (!query.starttimeOptionGiven && !query.endtimeOptionGiven)
    {
      if (query.toptions.startTimeUTC)
        query.toptions.startTime =
            producerDataPeriod.getLocalStartTime(producer, query.timezone, getTimeZones())
                .utc_time();
      else
        query.toptions.startTime =
            producerDataPeriod.getLocalStartTime(producer, query.timezone, getTimeZones())
                .local_time();
      if (query.toptions.startTimeUTC)
        query.toptions.endTime =
            producerDataPeriod.getLocalEndTime(producer, query.timezone, getTimeZones()).utc_time();
      else
        query.toptions.endTime =
            producerDataPeriod.getLocalEndTime(producer, query.timezone, getTimeZones())
                .local_time();
    }

    if (query.starttimeOptionGiven && !query.endtimeOptionGiven)
    {
      query.toptions.endTime = ptime_now;
    }

    if (!query.starttimeOptionGiven && query.endtimeOptionGiven)
    {
      query.toptions.startTime = query.toptions.endTime - boost::posix_time::hours(24);
      query.toptions.startTimeUTC = query.toptions.endTimeUTC;
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

    settings.starttime = settings.starttime - boost::posix_time::minutes(aggregationIntervalBehind);
    settings.endtime = settings.endtime + boost::posix_time::minutes(aggregationIntervalAhead);

    // observations up till now
    if (settings.endtime > now)
      settings.endtime = now;

    if (!query.toptions.timeStep)
      settings.timestep = 0;
    else
      settings.timestep = *query.toptions.timeStep;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void Plugin::getObsSettings(std::vector<SettingsInfo>& settingsVector,
                            const std::string& producer,
                            const ProducerDataPeriod& producerDataPeriod,
                            const boost::posix_time::ptime& now,
                            const ObsParameters& obsParameters,
                            Query& query) const
{
  try
  {
    Engine::Observation::Settings settings;

    // Common settings for all locations
    getCommonObsSettings(settings, producer, query);

    unsigned int aggregationIntervalBehind = 0;
    unsigned int aggregationIntervalAhead = 0;

    // Parameter related settings
    resolveParameterSettings(obsParameters,
                             query,
                             producer,
                             settings,
                             aggregationIntervalBehind,
                             aggregationIntervalAhead);

    // Time related settings
    resolveTimeSettings(producer,
                        producerDataPeriod,
                        now,
                        query,
                        aggregationIntervalBehind,
                        aggregationIntervalAhead,
                        settings);

    Engine::Observation::StationSettings stationSettings;

    for (const auto& tloc : query.loptions->locations())
    {
      Spine::LocationPtr loc = tloc.loc;
      if (!loc)
        continue;
      if ((loc->type == Spine::Location::Place || loc->type == Spine::Location::CoordinatePoint) &&
          loc->radius == 0)
      {
        if (!is_flash_or_mobile_producer(producer))
        {
          // Note: We do not detect if there is an fmisid for the location since converting
          // the search to be for a fmisid would lose the geoid tag for the location.

          stationSettings.nearest_station_settings.emplace_back(loc->longitude,
                                                                loc->latitude,
                                                                settings.maxdistance,
                                                                settings.numberofstations,
                                                                tloc.tag,
                                                                loc->fmisid);
        }
      }
      else
      {
        Engine::Observation::Settings areaSettings = settings;

        std::string name;
        if (resolveAreaStations(loc, producer, query, areaSettings, name))
          settingsVector.emplace_back(areaSettings, query.groupareas, name);
        else if (!areaSettings.wktArea.empty())
          settings.wktArea = areaSettings.wktArea;
      }
    }  // for-loop

    // Note: GEOIDs are processed by the nearest station search settings above

    // LPNNs
    for (auto lpnn : query.lpnns)
      stationSettings.lpnns.push_back(lpnn);
    // WMOs
    for (auto wmo : query.wmos)
      stationSettings.wmos.push_back(wmo);
    // FMISIDs
    for (auto fmisid : query.fmisids)
      stationSettings.fmisids.push_back(fmisid);

    // Bounding box
    if (!query.boundingBox.empty() && is_flash_producer(producer))
    {
      stationSettings.bounding_box_settings["minx"] = query.boundingBox.at("minx");
      stationSettings.bounding_box_settings["miny"] = query.boundingBox.at("miny");
      stationSettings.bounding_box_settings["maxx"] = query.boundingBox.at("maxx");
      stationSettings.bounding_box_settings["maxy"] = query.boundingBox.at("maxy");
    }

    if (!is_flash_or_mobile_producer(producer))
    {
      settings.taggedFMISIDs = itsObsEngine->translateToFMISID(
          settings.starttime, settings.endtime, producer, stationSettings);
    }

    if (settings.taggedFMISIDs.size() > 0 || settings.boundingBox.size() > 0 ||
        settings.taggedLocations.size() > 0)
      settingsVector.emplace_back(settings, false, "");

    if (settingsVector.empty() && is_flash_or_mobile_producer(producer))
      settingsVector.emplace_back(settings, false, "");

#ifdef MYDEBUG
    std::cout << "query.toptions.startTimeUTC: " << (query.toptions.startTimeUTC ? "true" : "false")
              << std::endl;
    std::cout << "query.toptions.endTimeUTC: " << (query.toptions.endTimeUTC ? "true" : "false")
              << std::endl;
    std::cout << "query.toptions.startTime: " << query.toptions.startTime << std::endl;
    std::cout << "query.toptions.endTime: " << query.toptions.endTime << std::endl;
    std::cout << "query.toptions.all(): " << query.toptions.all() << std::endl;
    if (query.toptions.timeSteps)
      std::cout << "query.toptions.timeSteps: " << *query.toptions.timeSteps << std::endl;
    if (query.toptions.timeStep)
      std::cout << "query.toptions.timeStep: " << *query.toptions.timeStep << std::endl;
    if (query.toptions.timeList.size() > 0)
    {
      std::cout << "query.toptions.timeList: " << std::endl;
      for (const auto& t : query.toptions.timeList)
        std::cout << t << std::endl;
    }
#endif
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
                                           const ObsParameters& obsParameterss,
                                           Engine::Observation::Settings& settings,
                                           Query& query,
                                           OutputData& outputData)
{
  try
  {
    ts::TimeSeriesVectorPtr observation_result;
    ObsParameters obsParameters = obsParameterss;

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
#ifdef MYDEBYG
    std::cout << "observation_result for places: " << *observation_result << std::endl;
#endif

    if (observation_result->empty())
      return;

    ts::Value missing_value = Spine::TimeSeries::None();
    int fmisid_index = get_fmisid_index(settings);

    Spine::TimeSeriesGeneratorCache::TimeList tlist;
    auto tz = getTimeZones().time_zone_from_string(query.timezone);
    // If query.toptions.startTime == query.toptions.endTime and timestep is missing
    // use minutes as timestep
    if (!query.toptions.timeStep && query.toptions.startTime == query.toptions.endTime)
    {
      query.toptions.timeStep = query.toptions.startTime.time_of_day().minutes();
      if (*query.toptions.timeStep == 0)
        query.toptions.timeStep = 60;
    }

    if (!query.toptions.all())
      tlist = itsTimeSeriesCache->generate(query.toptions, tz);

    TimeSeriesByLocation observation_result_by_location =
        get_timeseries_by_fmisid(producer, observation_result, tlist, fmisid_index);

    // iterate locations
    for (const auto& observation_result_location : observation_result_by_location)
    {
      observation_result = observation_result_location.second;
      int fmisid = observation_result_location.first;

      // get location

      Spine::LocationPtr loc;
      if (!is_flash_or_mobile_producer(producer))
      {
        loc = get_location(state.getGeoEngine(), fmisid, FMISID_PARAM, query.language);
        if (!loc)
        {
          // Most likely an old station not known to geoengine. The result will not
          // contain name, lon or lat. Use stationname, stationlon, stationlat instead.
          loc = boost::make_shared<Spine::Location>(0, 0, "", query.timezone);
        }
      }
      else
      {
        Spine::Location location(0, 0, "", query.timezone);
        loc = boost::make_shared<Spine::Location>(location);
      }

      // lets find out actual timesteps
      std::vector<boost::local_time::local_date_time> timestep_vector;
      const ts::TimeSeries& ts = observation_result->at(0);
      for (const auto& value : ts)
        timestep_vector.push_back(value.time);

      unsigned int obs_result_field_index = 0;
      ts::TimeSeriesVectorPtr observationResult2(new ts::TimeSeriesVector());
      std::map<std::string, unsigned int> parameterResultIndexes;

      // Iterate parameters and store values for all parameters
      // into observationResult2 data structure
      for (unsigned int i = 0; i < obsParameters.size(); i++)
      {
        ObsParameter& obsParam = obsParameters.at(i);

        std::string paramname(obsParam.param.name());

        if (SmartMet::Spine::is_location_parameter(paramname) &&
            !is_flash_or_mobile_producer(producer))
        {
          // add data for location field
          ts::TimeSeries timeseries(state.getLocalTimePool());

          for (const auto& timestep : timestep_vector)
          {
            ts::Value value = location_parameter(loc,
                                                 obsParam.param.name(),
                                                 query.valueformatter,
                                                 query.timezone,
                                                 query.precisions[i],
												 query.crs);

            timeseries.emplace_back(ts::TimedValue(timestep, value));
          }

          observationResult2->emplace_back(timeseries);
          parameterResultIndexes.insert(std::make_pair(paramname, observationResult2->size() - 1));
        }
        else if (SmartMet::Spine::is_time_parameter(paramname))
        {
          // add data for time fields
          Spine::Location location(0, 0, "", query.timezone);
          ts::TimeSeries timeseries(state.getLocalTimePool());
          for (const auto& timestep : timestep_vector)
          {
            ts::Value value = time_parameter(paramname,
                                             timestep,
                                             state.getTime(),
                                             *loc,
                                             query.timezone,
                                             getTimeZones(),
                                             query.outlocale,
                                             *query.timeformatter,
                                             query.timestring);
            timeseries.emplace_back(ts::TimedValue(timestep, value));
          }
          observationResult2->emplace_back(timeseries);
          parameterResultIndexes.insert(std::make_pair(paramname, observationResult2->size() - 1));
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
          // If special parameter contains missing values in some timesteps, replace them with
          // exsisting values
          if (SmartMet::Spine::is_special_parameter(paramname))
          {
            ts::Value actual_value = missing_value;
            bool missing_values_exists = false;
            for (const auto& item : result_at_index)
            {
              if (item.value == missing_value)
                missing_values_exists = true;
              else
                actual_value = item.value;
              if (actual_value != missing_value && missing_values_exists)
                break;
            }
            if (actual_value != missing_value && missing_values_exists)
            {
              for (auto& item : result_at_index)
                if (item.value == missing_value)
                  item.value = actual_value;
            }
          }

          observationResult2->push_back(result_at_index);
          std::string pname_plus_snumber = get_parameter_id(obsParameters[i].param);
          parameterResultIndexes.insert(
              std::make_pair(pname_plus_snumber, observationResult2->size() - 1));
          obs_result_field_index++;
        }
      }

      // Finally do aggregation if requested and remove reduntant timesteps
      observation_result = observationResult2;

      ts::TimeSeriesVectorPtr aggregated_observation_result(new ts::TimeSeriesVector());
      std::vector<TimeSeriesData> aggregatedData;
      // iterate parameters and do aggregation
      for (unsigned int i = 0; i < obsParameters.size(); i++)
      {
        const ObsParameter& obsParam = obsParameters[i];
        std::string paramname = obsParam.param.name();
        std::string pname_plus_snumber = get_parameter_id(obsParameters[i].param);
        if (parameterResultIndexes.find(pname_plus_snumber) != parameterResultIndexes.end())
          paramname = pname_plus_snumber;
        else if (parameterResultIndexes.find(paramname) == parameterResultIndexes.end())
          continue;

        unsigned int resultIndex = parameterResultIndexes.at(paramname);
        ts::TimeSeries ts = (*observation_result)[resultIndex];
        Spine::ParameterFunctions pfunc = obsParam.functions;
        ts::TimeSeriesPtr tsptr;
        // If inner function exists aggregation happens
        if (pfunc.innerFunction.exists())
        {
          tsptr = ts::aggregate(ts, pfunc);
          if (tsptr->empty())
            continue;
        }
        else
        {
          tsptr = boost::make_shared<ts::TimeSeries>(state.getLocalTimePool());
          *tsptr = ts;
        }
        aggregated_observation_result->push_back(*tsptr);
      }

      if (aggregated_observation_result->empty())
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
      if (query.toptions.all() || is_flash_or_mobile_producer(producer) ||
          producer == SYKE_PRODUCER)
      {
        boost::posix_time::ptime startTimeAsUTC = query.toptions.startTime;
        boost::posix_time::ptime endTimeAsUTC = query.toptions.endTime;
        if (!query.toptions.startTimeUTC)
        {
          boost::local_time::local_date_time ldt = Fmi::TimeParser::make_time(
              query.toptions.startTime.date(), query.toptions.startTime.time_of_day(), tz);
          startTimeAsUTC = ldt.utc_time();
        }
        if (!query.toptions.endTimeUTC)
        {
          boost::local_time::local_date_time ldt = Fmi::TimeParser::make_time(
              query.toptions.endTime.date(), query.toptions.endTime.time_of_day(), tz);
          endTimeAsUTC = ldt.utc_time();
        }

        Spine::TimeSeriesGenerator::LocalTimeList aggtimes;
        for (const ts::TimedValue& tv : aggregated_observation_result->at(0))
        {
          // Do not show timesteps beyond starttime/endtime
          if (tv.time.utc_time() >= startTimeAsUTC && tv.time.utc_time() <= endTimeAsUTC)
            aggtimes.push_back(tv.time);
        }
        // store observation data
        DataFunctions::store_data(
            DataFunctions::erase_redundant_timesteps(aggregated_observation_result, aggtimes),
            query,
            outputData);
      }
      else
      {
        // Else accept only the originally generated timesteps
        DataFunctions::store_data(
            DataFunctions::erase_redundant_timesteps(aggregated_observation_result, *tlist),
            query,
            outputData);
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
                                         const ObsParameters& obsParameters,
                                         const std::string& areaName,
                                         Engine::Observation::Settings& settings,
                                         Query& query,
                                         OutputData& outputData)
{
  try
  {
    // fetches results for all locations in the area and all parameters
    ts::TimeSeriesVectorPtr observation_result = itsObsEngine->values(settings, query.toptions);

#ifdef MYDEBYG
    std::cout << "observation_result for area: " << *observation_result << std::endl;
#endif
    if (observation_result->empty())
      return;

    // lets find out actual timesteps: different locations may have different timesteps
    std::vector<boost::local_time::local_date_time> ts_vector;
    std::set<boost::local_time::local_date_time> ts_set;
    const ts::TimeSeries& ts = observation_result->at(0);
    for (const ts::TimedValue& tval : ts)
      ts_set.insert(tval.time);
    ts_vector.insert(ts_vector.end(), ts_set.begin(), ts_set.end());
    std::sort(ts_vector.begin(), ts_vector.end());

    int fmisid_index = get_fmisid_index(settings);

    // All timesteps from result set
    Spine::TimeSeriesGeneratorCache::TimeList tlist_all =
        std::make_shared<Spine::TimeSeriesGenerator::LocalTimeList>(
            Spine::TimeSeriesGenerator::LocalTimeList());
    for (const auto t : ts_vector)
      tlist_all->push_back(t);

    // Separate timeseries of different locations to their own data structures and add missing
    // timesteps
    TimeSeriesByLocation tsv_area =
        get_timeseries_by_fmisid(producer, observation_result, tlist_all, fmisid_index);

    std::vector<FmisidTSVectorPair> tsv_area_with_added_fields;
    // add data for location- and time-related fields; these fields are added by timeseries
    // plugin
    for (FmisidTSVectorPair& val : tsv_area)
    {
      ts::TimeSeriesVector* tsv_observation_result = val.second.get();

      ts::TimeSeriesVectorPtr observation_result_with_added_fields(new ts::TimeSeriesVector());
      //		  ts::TimeSeriesVector* tsv_observation_result = observation_result.get();
      const ts::TimeSeries& fmisid_ts = tsv_observation_result->at(fmisid_index);

      // fmisid may be missing for rows for which there is no data. Hence we extract
      // it from the full time timeseries once.
      int fmisid = get_fmisid_value(fmisid_ts);

      Spine::LocationPtr loc =
          get_location(state.getGeoEngine(), fmisid, FMISID_PARAM, query.language);

      unsigned int obs_result_field_index = 0;
      for (unsigned int i = 0; i < obsParameters.size(); i++)
      {
        std::string paramname = obsParameters[i].param.name();

        // add data for location fields
        if (SmartMet::Spine::is_location_parameter(paramname) &&
            !is_flash_or_mobile_producer(producer))
        {
          ts::TimeSeries location_ts(state.getLocalTimePool());

          for (const auto& ts : ts_vector)
          {
            ts::Value value = location_parameter(loc,
                                                 obsParameters[i].param.name(),
                                                 query.valueformatter,
                                                 query.timezone,
                                                 query.precisions[i],
												 query.crs);

            location_ts.emplace_back(ts::TimedValue(ts, value));
          }
          observation_result_with_added_fields->push_back(location_ts);
        }
        else if (SmartMet::Spine::is_time_parameter(paramname))
        {
          // add data for time fields
          Spine::Location dummyloc(0, 0, "", query.timezone);

          ts::TimeSeriesGroupPtr grp(new ts::TimeSeriesGroup);
          ts::TimeSeries time_ts(state.getLocalTimePool());

          for (const auto& ts : ts_vector)
          {
            ts::Value value = time_parameter(paramname,
                                             ts,
                                             state.getTime(),
                                             (loc ? *loc : dummyloc),
                                             query.timezone,
                                             getTimeZones(),
                                             query.outlocale,
                                             *query.timeformatter,
                                             query.timestring);

            time_ts.emplace_back(ts::TimedValue(ts, value));
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
      tsv_area_with_added_fields.emplace_back(
          FmisidTSVectorPair(val.first, observation_result_with_added_fields));
    }

#ifdef MYDEBUG
    std::cout << "observation_result after locally added fields: " << std::endl;

    for (FmisidTSVectorPair& val : tsv_area_with_added_fields)
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
      tsg_vector.emplace_back(ts::TimeSeriesGroupPtr(new ts::TimeSeriesGroup));

    // iterate locations

    for (const auto& tsv_area : tsv_area_with_added_fields)
    {
      const ts::TimeSeriesVector* tsv = tsv_area.second.get();

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

    ts::Value missing_value = Spine::TimeSeries::None();
    // iterate parameters, aggregate, and store aggregated result

    for (const auto& obsparam : obsParameters)
    {
      unsigned int data_column = obsparam.data_column;
      ts::TimeSeriesGroupPtr tsg = tsg_vector.at(data_column);

      if (tsg->empty())
        continue;

      if (special(obsparam.param))
      {
        // value of these special fields is different in different locations,
        if (obsparam.param.name() != STATIONNAME_PARAM &&
            obsparam.param.name() != STATION_NAME_PARAM && obsparam.param.name() != LON_PARAM &&
            obsparam.param.name() != LAT_PARAM && obsparam.param.name() != LATLON_PARAM &&
            obsparam.param.name() != LONLAT_PARAM && obsparam.param.name() != PLACE_PARAM &&
            obsparam.param.name() != STATIONLON_PARAM &&
            obsparam.param.name() != STATIONLAT_PARAM &&
            obsparam.param.name() != STATIONLONGITUDE_PARAM &&
            obsparam.param.name() != STATIONLATITUDE_PARAM &&
            obsparam.param.name() != STATION_ELEVATION_PARAM &&
            obsparam.param.name() != STATIONARY_PARAM &&
            // obsparam.param.name() != DISTANCE_PARAM &&
            // obsparam.param.name() != DIRECTION_PARAM &&
            obsparam.param.name() != FMISID_PARAM && obsparam.param.name() != WMO_PARAM &&
            obsparam.param.name() != GEOID_PARAM && obsparam.param.name() != LPNN_PARAM &&
            obsparam.param.name() != RWSID_PARAM && obsparam.param.name() != SENSOR_NO_PARAM &&
            obsparam.param.name() != LONGITUDE_PARAM && obsparam.param.name() != LATITUDE_PARAM)
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

          // delete all but first timeseries for parameters that are not location dependent,
          // like time
          tsg->erase(tsg->begin() + 1, tsg->end());

          // area name is replaced with the name given in URL
          if (obsparam.param.name() == NAME_PARAM)
          { /*
             std::string place = (loc ? loc->name : "");
             ts::LonLatTimeSeries& llts = tsg->at(0);
             ts::TimeSeries& ts = llts.timeseries;
             std::string name = place;
             if (tloc.loc->type == Spine::Location::Wkt)
                   name = query.wktGeometries.getName(place);
             else if (name.find(" as ") != std::string::npos)
                   name = name.substr(name.find(" as ") + 4);
             for (ts::TimedValue& tv : ts)
                   tv.value = name;
            */
            ts::LonLatTimeSeries& llts = tsg->at(0);
            ts::TimeSeries& ts = llts.timeseries;
            for (ts::TimedValue& tv : ts)
              tv.value = areaName;
          }
        }
      }

      Spine::ParameterFunctions pfunc = obsparam.functions;
      // Do the aggregation if requasted
      ts::TimeSeriesGroupPtr aggregated_tsg(new ts::TimeSeriesGroup);
      if (pfunc.innerFunction.exists())
      {
        *aggregated_tsg = *(ts::aggregate(*tsg, pfunc));
      }
      else
      {
        *aggregated_tsg = *tsg;
      }

#ifdef MYDEBUG
      std::cout << boost::posix_time::second_clock::universal_time() << " - aggregated group#" << i
                << ": " << std::endl
                << *aggregated_tsg << std::endl;
#endif

      std::vector<TimeSeriesData> aggregatedData;

      // If all timesteps are requested or producer is syke or flash accept all timesteps
      if (query.toptions.all() || is_flash_or_mobile_producer(producer) ||
          producer == SYKE_PRODUCER)
      {
        Spine::TimeSeriesGenerator::LocalTimeList aggtimes;
        ts::TimeSeries ts = aggregated_tsg->at(0).timeseries;
        for (const ts::TimedValue& tv : ts)
          aggtimes.push_back(tv.time);
        // store observation data
        aggregatedData.emplace_back(
            TimeSeriesData(DataFunctions::erase_redundant_timesteps(aggregated_tsg, aggtimes)));
        DataFunctions::store_data(aggregatedData, query, outputData);
      }
      else
      {
        // Else accept only the original generated timesteps
        // Generate requested timesteps
        Spine::TimeSeriesGeneratorCache::TimeList tlist;
        auto tz = getTimeZones().time_zone_from_string(query.timezone);
        if (!query.toptions.all())
          tlist = itsTimeSeriesCache->generate(query.toptions, tz);

        aggregatedData.emplace_back(
            TimeSeriesData(DataFunctions::erase_redundant_timesteps(aggregated_tsg, *tlist)));
        // store observation data
        DataFunctions::store_data(aggregatedData, query, outputData);
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
                                   const ObsParameters& obsParameters)
{
  try
  {
    if (areaproducers.empty())
      throw Fmi::Exception(BCP, "BUG: processObsEngineQuery producer list empty");

    for (const auto& producer : areaproducers)
    {
      if (!isObsProducer(producer))
        continue;

      std::vector<SettingsInfo> settingsVector;

      getObsSettings(
          settingsVector, producer, producerDataPeriod, state.getTime(), obsParameters, query);

      for (auto& item : settingsVector)
      {
        Engine::Observation::Settings& settings = item.settings;
        settings.localTimePool = state.getLocalTimePool();

        if (query.debug)
          settings.debug_options = Engine::Observation::Settings::DUMP_SETTINGS;

#ifdef MYDEBUG
        print_settings(settings);
#endif

        std::vector<TimeSeriesData> tsdatavector;
        outputData.emplace_back(make_pair("_obs_", tsdatavector));

        if (!item.is_area || is_flash_or_mobile_producer(producer))
          fetchObsEngineValuesForPlaces(
              state, producer, obsParameters, settings, query, outputData);
        else
          fetchObsEngineValuesForArea(
              state, producer, obsParameters, item.area_name, settings, query, outputData);
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
                                 const ProducerDataPeriod& producerDataPeriod)
{
  try
  {
    // Resolve locations for FMISDs,WMOs,LPNNs (https://jira.fmi.fi/browse/BRAINSTORM-1848)
    Engine::Geonames::LocationOptions lopt = itsGeoEngine->parseLocations(
        masterquery.fmisids, masterquery.lpnns, masterquery.wmos, masterquery.language);
    const Spine::TaggedLocationList& locations = lopt.locations();
    Spine::TaggedLocationList tagged_ll = masterquery.loptions->locations();
    tagged_ll.insert(tagged_ll.end(), locations.begin(), locations.end());
    masterquery.loptions->setLocations(tagged_ll);

    // If user wants to get grid points of area to separate lines, resolve coordinates inside area
    if (masterquery.groupareas == false)
      resolveAreaLocations(masterquery, state, areaproducers);

    // first timestep is here in utc
    boost::posix_time::ptime first_timestep = masterquery.latestTimestep;

    bool firstProducer = outputData.empty();

    std::set<std::string> processed_locations;
    for (const auto& tloc : masterquery.loptions->locations())
    {
      std::string location_id = get_location_id(tloc.loc);
      // Data for each location is fetched only once
      if (processed_locations.find(location_id) != processed_locations.end())
        continue;
      processed_locations.insert(location_id);

      Query query = masterquery;
      QueryLevelDataCache queryLevelDataCache;

      std::vector<TimeSeriesData> tsdatavector;
      outputData.emplace_back(make_pair(location_id, tsdatavector));

      if (masterquery.timezone == LOCALTIME_PARAM)
        query.timezone = tloc.loc->timezone;

      query.toptions.startTime = first_timestep;
      if (!firstProducer)
        query.toptions.startTime += boost::posix_time::minutes(1);

      // producer can be alias, get actual producer
      std::string producer(select_producer(*itsQEngine, *(tloc.loc), query, areaproducers));
      bool isClimatologyProducer =
          (producer.empty() ? false : itsQEngine->getProducerConfig(producer).isclimatology);

      boost::local_time::local_date_time data_period_endtime(
          producerDataPeriod.getLocalEndTime(producer, query.timezone, getTimeZones()));

      // Reset for each new location, since fetchQEngineValues modifies it
      auto old_start_time = query.toptions.startTime;

      for (const Spine::ParameterAndFunctions& paramfunc : query.poptions.parameterFunctions())
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool Plugin::processGridEngineQuery(const State& state,
                                    Query& query,
                                    OutputData& outputData,
                                    QueryServer::QueryStreamer_sptr queryStreamer,
                                    const AreaProducers& areaproducers,
                                    const ProducerDataPeriod& producerDataPeriod)
{
  FUNCTION_TRACE
  try
  {
    if (itsConfig.gridEngineDisabled())
      return false;

    boost::posix_time::ptime latestTimestep = query.latestTimestep;

    BOOST_FOREACH (auto& tloc, query.loptions->locations())
    {
      query.latestTimestep = latestTimestep;

      Spine::LocationPtr loc = tloc.loc;
      std::string place = get_name_base(loc->name);

      std::vector<std::vector<T::Coordinate>> polygonPath;

      switch (loc->type)
      {
        case Spine::Location::Wkt:
        {
          NFmiSvgPath svgPath;
          loc = query.wktGeometries.getLocation(tloc.loc->name);
          svgPath = query.wktGeometries.getSvgPath(tloc.loc->name);
          convertSvgPathToPolygonPath(svgPath, polygonPath);

          if (polygonPath.size() > 1 && getPolygonPathLength(polygonPath) == polygonPath.size())
          {
            T::Coordinate_vec polygonPoints;
            convertToPointVector(polygonPath, polygonPoints);
            polygonPath.clear();
            polygonPath.push_back(polygonPoints);
          }
        }
        break;
        case Spine::Location::Area:
        {
          NFmiSvgPath svgPath;
          loc = getLocationForArea(tloc, tloc.loc->radius * 1000, query, &svgPath);
          convertSvgPathToPolygonPath(svgPath, polygonPath);
        }
        break;

        case Spine::Location::Path:
        {
          NFmiSvgPath svgPath;
          loc = getLocationForArea(tloc, tloc.loc->radius * 1000, query, &svgPath);
          // convertSvgPathToPolygonPath(svgPath,polygonPath);
          Spine::LocationList locationList =
              get_location_list(svgPath, tloc.tag, query.step, *itsGeoEngine);
          std::vector<T::Coordinate> coordinates;
          for (const auto& ll : locationList)
          {
            coordinates.emplace_back(T::Coordinate(ll->longitude, ll->latitude));
          }
          polygonPath.emplace_back(coordinates);
        }
        break;

        case Spine::Location::BoundingBox:
        {
          // Split bounding box coordinates from the query string
          std::vector<std::string> parts;
          boost::algorithm::split(parts, place, boost::algorithm::is_any_of(","));

          double lon1 = Fmi::stod(parts[0]);
          double lat1 = Fmi::stod(parts[1]);
          double lon2 = Fmi::stod(parts[2]);
          double lat2 = Fmi::stod(parts[3]);

          // Get location info by the center coordinates
          Spine::LocationPtr locCenter = get_bbox_location(place, query.language, *itsGeoEngine);

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

          if (tloc.loc->radius == 0)
          {
            // Create bounding box polygon

            std::vector<T::Coordinate> coordinates;
            coordinates.emplace_back(T::Coordinate(lon1, lat1));
            coordinates.emplace_back(T::Coordinate(lon1, lat2));
            coordinates.emplace_back(T::Coordinate(lon2, lat2));
            coordinates.emplace_back(T::Coordinate(lon2, lat1));
            coordinates.emplace_back(T::Coordinate(lon1, lat1));
            polygonPath.emplace_back(coordinates);
          }
          else
          {
            // Create expanded bounding box polygon

            const OGRGeometry* geom =
                itsGeometryStorage.getOGRGeometry(locCenter->area, wkbMultiPolygon);
            std::unique_ptr<const OGRGeometry> newGeomUptr;
            std::unique_ptr<OGRGeometry> expandedGeomUptr;
            if (geom)
            {
              OGRGeometry* newGeom = geom->clone();
              newGeomUptr.reset(newGeom);

              auto wkt = fmt::format("MULTIPOLYGON ((({} {},{} {},{} {},{} {},{} {})))",
                                     lon1,
                                     lat1,
                                     lon1,
                                     lat2,
                                     lon2,
                                     lat2,
                                     lon2,
                                     lat1,
                                     lon1,
                                     lat1);

              const char* p = wkt.c_str();
              newGeom->importFromWkt(&p);

              auto expandedGeom = Fmi::OGR::expandGeometry(newGeom, tloc.loc->radius);
              expandedGeomUptr.reset(expandedGeom);

              std::string wktString = Fmi::OGR::exportToWkt(*expandedGeom);
              convertWktMultipolygonToPolygonPath(wktString, polygonPath);
            }
            else
            {
              std::cout << "### GEOMETRY NOT FOUND\n";
            }
          }
        }
        break;

        default:
        {
          NFmiSvgPath svgPath;
          get_svg_path(tloc, itsGeometryStorage, svgPath);
          convertSvgPathToPolygonPath(svgPath, polygonPath);
        }
        break;
      }

      T::GeometryId_set geometryIdList;
      if (areaproducers.empty() && !itsGridInterface->containsParameterWithGridProducer(query) &&
          !itsGridInterface->isValidDefaultRequest(itsConfig.defaultGridGeometries(),
                                                   itsConfig.ignoreGridGeometriesWhenPreloadReady(),
                                                   polygonPath,
                                                   geometryIdList))
        return false;

      std::string country = itsGeoEngine->countryName(loc->iso2, query.language);
      // std::cout << formatLocation(*loc) << endl;
      // std::cout << formatLocation(*(tloc.loc)) << endl;

      AreaProducers producers = areaproducers;
      auto defaultProducer = itsConfig.defaultProducerMappingName();

      if (producers.empty() && !defaultProducer.empty())
        producers.push_back(defaultProducer);

      itsGridInterface->processGridQuery(state,
                                         query,
                                         outputData,
                                         queryStreamer,
                                         producers,
                                         producerDataPeriod,
                                         tloc,
                                         loc,
                                         country,
                                         geometryIdList,
                                         polygonPath);
    }
    return true;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", NULL);
  }
}

void Plugin::checkInKeywordLocations(Query& masterquery)
{
  // If inkeyword given resolve locations
  if (!masterquery.inKeywordLocations.empty())
  {
    Spine::TaggedLocationList tloc_list;
    for (const auto& tloc : masterquery.loptions->locations())
    {
      if (tloc.loc->type == Spine::Location::Wkt)
      {
        // Find locations inside WKT-area
        const OGRGeometry* geom = masterquery.wktGeometries.getGeometry(tloc.loc->name);
        if (geom)
          tloc_list = get_locations_inside_geometry(masterquery.inKeywordLocations, *geom);
      }
      else if (tloc.loc->type == Spine::Location::Area)
      {
        // Find locations inside Area
        const OGRGeometry* geom = get_ogr_geometry(tloc, itsGeometryStorage);
        if (geom)
          tloc_list = get_locations_inside_geometry(masterquery.inKeywordLocations, *geom);
      }
      else if (tloc.loc->type == Spine::Location::BoundingBox)
      {
        // Find locations inside Bounding Box
        Spine::BoundingBox bbox(get_name_base(tloc.loc->name));

        std::string wkt =
            ("POLYGON((" + Fmi::to_string(bbox.xMin) + " " + Fmi::to_string(bbox.yMin) + "," +
             Fmi::to_string(bbox.xMin) + " " + Fmi::to_string(bbox.yMax) + "," +
             Fmi::to_string(bbox.xMax) + " " + Fmi::to_string(bbox.yMax) + "," +
             Fmi::to_string(bbox.xMax) + " " + Fmi::to_string(bbox.yMin) + "," +
             Fmi::to_string(bbox.xMin) + " " + Fmi::to_string(bbox.yMin) + "))");
        std::unique_ptr<OGRGeometry> geom = get_ogr_geometry(wkt);
        if (geom)
          tloc_list = get_locations_inside_geometry(masterquery.inKeywordLocations, *geom);
      }
      else if (tloc.loc->type == Spine::Location::CoordinatePoint ||
               tloc.loc->type == Spine::Location::Place)
      {
        if (tloc.loc->radius == 0)
        {
          // Find nearest location
          std::pair<double, double> from_location(tloc.loc->longitude, tloc.loc->latitude);
          double distance = -1;
          Spine::LocationPtr nearest_loc = nullptr;
          for (const auto& loc : masterquery.inKeywordLocations)
          {
            std::pair<double, double> to_location(loc->longitude, loc->latitude);

            double dist = distance_in_kilometers(from_location, to_location);
            if (distance == -1 || dist < distance)
            {
              distance = dist;
              nearest_loc = loc;
            }
          }
          if (nearest_loc)
          {
            tloc_list.emplace_back(Spine::TaggedLocation(nearest_loc->name, nearest_loc));
          }
        }
        else
        {
          // Find locations inside area
          std::string wkt = "POINT(";
          wkt += Fmi::to_string(tloc.loc->longitude);
          wkt += " ";
          wkt += Fmi::to_string(tloc.loc->latitude);
          wkt += ")";
          std::unique_ptr<OGRGeometry> geom = get_ogr_geometry(wkt, tloc.loc->radius);
          if (geom)
            tloc_list = get_locations_inside_geometry(masterquery.inKeywordLocations, *geom);
        }
      }
    }
    masterquery.loptions->setLocations(tloc_list);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

boost::shared_ptr<std::string> Plugin::processQuery(
    const State& state,
    Spine::Table& table,
    Query& masterquery,
    const QueryServer::QueryStreamer_sptr& queryStreamer,
    size_t& product_hash)
{
  try
  {
    checkInKeywordLocations(masterquery);

    // if only location related parameters queried, use shortcut
    if (is_plain_location_query(masterquery.poptions.parameters()))
    {
      fetchStaticLocationValues(masterquery, table, 0, 0);
      return {};
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

    const bool producerMissing = masterquery.timeproducers.empty();
    if (producerMissing)
      masterquery.timeproducers.emplace_back(AreaProducers());

#ifndef WITHOUT_OBSERVATION
    const ObsParameters obsParameters = getObsParameters(masterquery);
#endif

    boost::posix_time::ptime latestTimestep = masterquery.latestTimestep;
    bool startTimeUTC = masterquery.toptions.startTimeUTC;

    // This loop will iterate through the producers, collecting as much
    // data in order as is possible. The later producers patch the data
    // *after* the first ones if possible.

    std::size_t producer_group = 0;
    for (const AreaProducers& areaproducers : masterquery.timeproducers)
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
        processObsEngineQuery(
            state, query, outputData, areaproducers, producerDataPeriod, obsParameters);
      }
      else
#endif
          // Grid-query is executed if the following conditions are fulfilled:
          //   1. The usage of Grid-Engine is enabled (=> timeseries configuration file)
          //   2. The actual Grid-Engine is enabled (=> grid-engine configuration file)
          //   3. If the one of the following conditions is true:
          //       a) Grid-query is requested by the query parameter (source=grid)
          //       b) Query source is not defined and at least one of the producers is a grid
          //       producer c) Query source is not defined and at least one of the query parameters
          //       contains a grid producer d) Query source is not defined and no producers are
          //       defined and the primary forecast
          //          source defined in the config file is "grid".

          if (!itsConfig.gridEngineDisabled() && itsGridEngine->isEnabled() &&
              (strcasecmp(masterquery.forecastSource.c_str(), "grid") == 0 ||
               (masterquery.forecastSource == "" &&
                (((!areaproducers.empty() &&
                   itsGridInterface->containsGridProducer(masterquery))) ||
                 (itsGridInterface->containsParameterWithGridProducer(masterquery)) ||
                 (areaproducers.empty() &&
                  strcasecmp(itsConfig.primaryForecastSource().c_str(), "grid") == 0)))))
      {
        bool processed = processGridEngineQuery(
            state, query, outputData, queryStreamer, areaproducers, producerDataPeriod);

        if (processed)
        {
          // We need different hash calculcations for the grid requests.
          product_hash = Fmi::bad_hash;
        }
        // If the query was not processed then we should call the QEngine instead.
        else
        {
          processQEngineQuery(state, query, outputData, areaproducers, producerDataPeriod);
        }
      }
      else
      {
        processQEngineQuery(state, query, outputData, areaproducers, producerDataPeriod);
      }

      // get the latestTimestep from previous query
      latestTimestep = query.latestTimestep;
      startTimeUTC = query.toptions.startTimeUTC;
      ++producer_group;
    }

#ifndef WITHOUT_OBSERVATION
    fix_precisions(masterquery, obsParameters);
#endif

    // insert data into the table
    fill_table(masterquery, outputData, table);

    return nullptr;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    using std::chrono::duration_cast;
    using std::chrono::high_resolution_clock;
    using std::chrono::microseconds;

    Spine::Table data;
    QueryServer::QueryStreamer_sptr queryStreamer;

    // Options
    high_resolution_clock::time_point t1 = high_resolution_clock::now();
    Query query(state, request, itsConfig);
    high_resolution_clock::time_point t2 = high_resolution_clock::now();

    data.setPaging(query.startrow, query.maxresults);

    std::string producer_option =
        Spine::optional_string(request.getParameter(PRODUCER_PARAM),
                               Spine::optional_string(request.getParameter(STATIONTYPE_PARAM), ""));
    boost::algorithm::to_lower(producer_option);
    // At least one of location specifiers must be set

#ifndef WITHOUT_OBSERVATION
    if (query.fmisids.empty() && query.lpnns.empty() && query.wmos.empty() &&
        query.boundingBox.empty() && !is_flash_or_mobile_producer(producer_option) &&
        query.loptions->locations().empty())
#else
    if (query.loptions->locations().empty())
#endif
      throw Fmi::Exception(BCP, "No location option given!");

    // The formatter knows which mimetype to send

    SmartMet::Spine::TableFormatter* fmt = nullptr;

    if (strcasecmp(query.format.c_str(), "IMAGE") == 0)
      fmt = new Spine::ImageFormatter();
    else if (strcasecmp(query.format.c_str(), "FILE") == 0)
    {
      fmt = new Spine::ImageFormatter();
      auto* qStreamer = new QueryServer::QueryStreamer();
      queryStreamer.reset(qStreamer);
    }
    else if (strcasecmp(query.format.c_str(), "INFO") == 0)
      fmt = Spine::TableFormatterFactory::create("debug");
    else
      fmt = Spine::TableFormatterFactory::create(query.format);

    bool gridEnabled = false;
    if (strcasecmp(query.forecastSource.c_str(), "grid") == 0 ||
        (query.forecastSource.length() == 0 &&
         strcasecmp(itsConfig.primaryForecastSource().c_str(), "grid") == 0))
      gridEnabled = true;

    boost::shared_ptr<Spine::TableFormatter> formatter(fmt);
    std::string mime = formatter->mimetype() + "; charset=UTF-8";
    response.setHeader("Content-Type", mime);

    // Calculate the hash value for the product.

    std::size_t product_hash = Fmi::bad_hash;

    try
    {
      product_hash = hash_value(state, query, request);
    }
    catch (...)
    {
      if (!gridEnabled)
        throw Fmi::Exception(BCP, "Operation failed!", NULL);
    }

    high_resolution_clock::time_point t3 = high_resolution_clock::now();

    std::string timeheader = Fmi::to_string(duration_cast<microseconds>(t2 - t1).count()) + '+' +
                             Fmi::to_string(duration_cast<microseconds>(t3 - t2).count());

    if (product_hash != Fmi::bad_hash)
    {
      response.setHeader("ETag", fmt::format("\"{:x}-timeseries\"", product_hash));

      // If the product is cacheable and etag was requested, respond with etag only

      if (request.getHeader("X-Request-ETag"))
      {
        response.setStatus(Spine::HTTP::Status::no_content);
        return;
      }
    }
    // If obj is not nullptr it is from cache
    auto obj = processQuery(state, data, query, queryStreamer, product_hash);

    if (obj)
    {
      response.setHeader("X-Duration", timeheader);
      response.setHeader("X-TimeSeries-Cache", "yes");
      response.setContent(obj);
      return;
    }

    // Must generate the result from scratch
    response.setHeader("X-TimeSeries-Cache", "no");

    high_resolution_clock::time_point t4 = high_resolution_clock::now();
    timeheader.append("+").append(
        Fmi::to_string(std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3).count()));

    data.setMissingText(query.valueformatter.missing());

    // The names of the columns
    Spine::TableFormatter::Names headers;
    for (const Spine::Parameter& p : query.poptions.parameters())
    {
      std::string header_name = p.alias();
      std::vector<std::string> partList;
      splitString(header_name, ':', partList);
      // There was a merge conflict in here at one time. GRIB branch processed
      // these two special cases, while master added the sensor part below. This
      // may be incorrect.
      if (partList.size() > 2 && (partList[0] == "ISOBANDS" || partList[0] == "ISOLINES"))
      {
        const char* p = header_name.c_str() + partList[0].size() + partList[1].size() + 2;
        headers.push_back(p);
      }
      else
      {
        const boost::optional<int>& sensor_no = p.getSensorNumber();
        if (sensor_no)
        {
          header_name += ("_#" + Fmi::to_string(*sensor_no));
        }
        if (!p.getSensorParameter().empty())
          header_name += ("_" + p.getSensorParameter());
        headers.push_back(header_name);
      }
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

    auto out = formatter->format(data, headers, request, formatter_options);
    high_resolution_clock::time_point t5 = high_resolution_clock::now();
    timeheader.append("+").append(
        Fmi::to_string(std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4).count()));

    // TODO: Should use std::move when it has become available
    boost::shared_ptr<std::string> result(new std::string());
    std::swap(out, *result);

    // Too many flash data requests with empty output filling the logs...
#if 0    
    if (result->empty())
    {
      std::cerr << "Warning: Empty output for request " << request.getQueryString() << " from "
                << request.getClientIP() << std::endl;
    }
#endif

#ifdef MYDEBUG
    std::cout << "Output:" << std::endl << *result << std::endl;
#endif

    response.setHeader("X-Duration", timeheader);

    if (strcasecmp(query.format.c_str(), "FILE") == 0)
    {
      std::string filename =
          "attachement; filename=timeseries_" + std::to_string(getTime()) + ".grib";
      response.setHeader("Content-type", "application/octet-stream");
      response.setHeader("Content-Disposition", filename);
      response.setContent(queryStreamer);
    }
    else
    {
      response.setContent(*result);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Plugin::grouplocations(Spine::HTTP::Request& theRequest)
{
  try
  {
    auto lonlats = theRequest.getParameter("lonlats");
    if (!lonlats)
      lonlats = theRequest.getParameter("lonlat");
    auto latlons = theRequest.getParameter("latlons");
    if (!latlons)
      latlons = theRequest.getParameter("latlon");
    auto places = theRequest.getParameter("places");
    auto fmisid = theRequest.getParameter("fmisid");
    auto lpnn = theRequest.getParameter("lpnn");
    auto wmo = theRequest.getParameter("wmo");

    std::string wkt_multipoint = "MULTIPOINT(";
    if (lonlats)
    {
      theRequest.removeParameter("lonlat");
      theRequest.removeParameter("lonlats");
      std::vector<std::string> parts;
      boost::algorithm::split(parts, *lonlats, boost::algorithm::is_any_of(","));
      if (parts.size() % 2 != 0)
        throw Fmi::Exception(BCP, "Invalid lonlats list: " + *lonlats);

      for (unsigned int j = 0; j < parts.size(); j += 2)
      {
        if (wkt_multipoint != "MULTIPOINT(")
          wkt_multipoint += ",";
        wkt_multipoint += "(" + parts[j] + " " + parts[j + 1] + ")";
      }
    }
    if (latlons)
    {
      theRequest.removeParameter("latlon");
      theRequest.removeParameter("latlons");
      std::vector<std::string> parts;
      boost::algorithm::split(parts, *latlons, boost::algorithm::is_any_of(","));
      if (parts.size() % 2 != 0)
        throw Fmi::Exception(BCP, "Invalid latlons list: " + *latlons);

      for (unsigned int j = 0; j < parts.size(); j += 2)
      {
        if (wkt_multipoint != "MULTIPOINT(")
          wkt_multipoint += ",";
        wkt_multipoint += "(" + parts[j + 1] + " " + parts[j] + ")";
      }
    }
    if (places)
    {
      theRequest.removeParameter("places");
      std::vector<std::string> parts;
      boost::algorithm::split(parts, *places, boost::algorithm::is_any_of(","));
      for (const auto& place : parts)
      {
        Spine::LocationPtr loc = itsGeoEngine->nameSearch(place, "fi");
        if (loc)
        {
          if (wkt_multipoint != "MULTIPOINT(")
            wkt_multipoint += ",";
          wkt_multipoint +=
              "(" + Fmi::to_string(loc->longitude) + " " + Fmi::to_string(loc->latitude) + ")";
        }
      }
    }
    std::vector<int> fmisids;
    std::vector<int> lpnns;
    std::vector<int> wmos;
    if (fmisid)
    {
      theRequest.removeParameter("fmisid");
      std::vector<std::string> parts;
      boost::algorithm::split(parts, *fmisid, boost::algorithm::is_any_of(","));
      for (const auto& id : parts)
        fmisids.push_back(Fmi::stoi(id));
    }

    if (lpnn)
    {
      theRequest.removeParameter("lpnn");
      std::vector<std::string> parts;
      boost::algorithm::split(parts, *lpnn, boost::algorithm::is_any_of(","));
      for (const auto& id : parts)
        lpnns.push_back(Fmi::stoi(id));
    }

    if (wmo)
    {
      theRequest.removeParameter("wmo");
      std::vector<std::string> parts;
      boost::algorithm::split(parts, *wmo, boost::algorithm::is_any_of(","));
      for (const auto& id : parts)
        wmos.push_back(Fmi::stoi(id));
    }

    Engine::Geonames::LocationOptions lopts =
        itsGeoEngine->parseLocations(fmisids, lpnns, wmos, "fi");
    for (const auto& lopt : lopts.locations())
    {
      if (lopt.loc)
      {
        if (wkt_multipoint != "MULTIPOINT(")
          wkt_multipoint += ",";
        wkt_multipoint += "(" + Fmi::to_string(lopt.loc->longitude) + " " +
                          Fmi::to_string(lopt.loc->latitude) + ")";
      }
    }

    wkt_multipoint += ")";
    theRequest.addParameter("wkt", wkt_multipoint);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Main content handler
 */
// ----------------------------------------------------------------------

void Plugin::requestHandler(Spine::Reactor& /* theReactor */,
                            const Spine::HTTP::Request& theRequest,
                            Spine::HTTP::Response& theResponse)
{
  // We need this in the catch block
  bool isdebug = false;

  try
  {
    if (Spine::optional_bool(theRequest.getParameter("grouplocations"), false))
      grouplocations(const_cast<Spine::HTTP::Request&>(theRequest));

    isdebug = ("debug" == Spine::optional_string(theRequest.getParameter("format"), ""));

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
      theResponse.setHeader("Cache-Control", cachecontrol);

      boost::posix_time::ptime t_expires =
          state.getTime() + boost::posix_time::seconds(expires_seconds);
      std::string expiration = tformat->format(t_expires);
      theResponse.setHeader("Expires", expiration);
    }

    std::string modification = tformat->format(state.getTime());
    theResponse.setHeader("Last-Modified", modification);
  }
  catch (...)
  {
    // Catching all exceptions

    Fmi::Exception ex(BCP, "Request processing exception!", nullptr);
    ex.addParameter("URI", theRequest.getURI());
    ex.addParameter("ClientIP", theRequest.getClientIP());
    ex.printError();

    std::string firstMessage = ex.what();
    if (isdebug)
    {
      // Delivering the exception information as HTTP content
      std::string fullMessage = ex.getHtmlStackTrace();
      theResponse.setContent(fullMessage);
      theResponse.setStatus(Spine::HTTP::Status::ok);
    }
    else
    {
      if (firstMessage.find("timeout") != std::string::npos)
        theResponse.setStatus(Spine::HTTP::Status::request_timeout);
      else
        theResponse.setStatus(Spine::HTTP::Status::bad_request);
    }

    // Adding the first exception information into the response header
    boost::algorithm::replace_all(firstMessage, "\n", " ");
    firstMessage = firstMessage.substr(0, 300);
    theResponse.setHeader("X-TimeSeriesPlugin-Error", firstMessage);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Plugin constructor
 */
// ----------------------------------------------------------------------

Plugin::Plugin(Spine::Reactor* theReactor, const char* theConfig)
    : SmartMetPlugin(), itsModuleName("TimeSeries"), itsConfig(theConfig), itsReactor(theReactor)
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    // Time series cache
    itsTimeSeriesCache.reset(new Spine::TimeSeriesGeneratorCache);
    itsTimeSeriesCache->resize(itsConfig.maxTimeSeriesCacheSize());

    /* GeoEngine */
    auto* engine = itsReactor->getSingleton("Geonames", nullptr);
    if (!engine)
      throw Fmi::Exception(BCP, "Geonames engine unavailable");
    itsGeoEngine = reinterpret_cast<Engine::Geonames::Engine*>(engine);

    /* GisEngine */
    engine = itsReactor->getSingleton("Gis", nullptr);
    if (!engine)
      throw Fmi::Exception(BCP, "Gis engine unavailable");
    itsGisEngine = reinterpret_cast<Engine::Gis::Engine*>(engine);

    // Read the geometries from PostGIS database
    itsGisEngine->populateGeometryStorage(itsConfig.getPostGISIdentifiers(), itsGeometryStorage);

    /* QEngine */
    engine = itsReactor->getSingleton("Querydata", nullptr);
    if (!engine)
      throw Fmi::Exception(BCP, "Querydata engine unavailable");
    itsQEngine = reinterpret_cast<Engine::Querydata::Engine*>(engine);

    /* GridEngine */
    if (!itsConfig.gridEngineDisabled())
    {
      engine = itsReactor->getSingleton("grid", NULL);
      if (!engine)
        throw Fmi::Exception(BCP, "The 'grid-engine' unavailable!");

      itsGridEngine = reinterpret_cast<Engine::Grid::Engine*>(engine);
      itsGridEngine->setDem(itsGeoEngine->dem());
      itsGridEngine->setLandCover(itsGeoEngine->landCover());

      itsGridInterface.reset(new GridInterface(itsGridEngine, itsGeoEngine->getTimeZones()));
    }

#ifndef WITHOUT_OBSERVATION
    if (!itsConfig.obsEngineDisabled())
    {
      /* ObsEngine */
      engine = itsReactor->getSingleton("Observation", nullptr);
      if (!engine)
        throw Fmi::Exception(BCP, "Observation engine unavailable");
      itsObsEngine = reinterpret_cast<Engine::Observation::Engine*>(engine);

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
      throw Fmi::Exception(BCP, "Failed to register timeseries content handler");

    // DEPRECATED:

    if (!itsReactor->addContentHandler(
            this, "/pointforecast", boost::bind(&Plugin::callRequestHandler, this, _1, _2, _3)))
      throw Fmi::Exception(BCP, "Failed to register pointforecast content handler");

    // DONE
    itsReady = true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Destructor
 */
// ----------------------------------------------------------------------

Plugin::~Plugin() {}

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

Fmi::Cache::CacheStatistics Plugin::getCacheStats() const
{
  Fmi::Cache::CacheStatistics ret;

  ret.insert(std::make_pair("Timeseries::timeseries_generator_cache",
                            itsTimeSeriesCache->getCacheStats()));

  return ret;
}

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
