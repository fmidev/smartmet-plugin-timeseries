// ======================================================================
/*!
 * \brief SmartMet TimeSeries plugin implementation
 */
// ======================================================================

#include "Plugin.h"
#include "Hash.h"
#include "LocationTools.h"
#include "ParameterTools.h"
#include "Query.h"
#include "QueryLevelDataCache.h"
#include "State.h"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/timer/timer.hpp>
#include <boost/tokenizer.hpp>
#include <engines/gis/Engine.h>
#include <engines/gis/MapOptions.h>
#include <engines/observation/Keywords.h>
#include <engines/observation/Utils.h>
#include <engines/querydata/OriginTime.h>
#include <fmt/format.h>
#include <gis/OGR.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeFormatter.h>
#include <newbase/NFmiIndexMask.h>
#include <newbase/NFmiIndexMaskTools.h>
#include <newbase/NFmiMultiQueryInfo.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiSvgTools.h>
#include <spine/Convenience.h>
#include <spine/Exception.h>
#include <spine/ParameterFactory.h>
#include <spine/SmartMet.h>
#include <spine/Table.h>
#include <spine/TableFeeder.h>
#include <spine/TableFormatterFactory.h>
#include <spine/TimeSeriesAggregator.h>
#include <spine/TimeSeriesOutput.h>
#include <spine/ValueFormatter.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>

using boost::numeric_cast;
using boost::local_time::local_date_time;
using boost::numeric::bad_numeric_cast;
using boost::numeric::negative_overflow;
using boost::numeric::positive_overflow;
using boost::posix_time::hours;
using boost::posix_time::minutes;
using boost::posix_time::ptime;
using boost::posix_time::seconds;

//#define MYDEBUG ON

namespace ts = SmartMet::Spine::TimeSeries;

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
namespace
{
bool is_mobile_producer(const std::string& producer)
{
  return (producer == SmartMet::Engine::Observation::ROADCLOUD_PRODUCER ||
          producer == SmartMet::Engine::Observation::TECONER_PRODUCER ||
          producer == SmartMet::Engine::Observation::NETATMO_PRODUCER);
}

bool is_flash_producer(const std::string& producer)
{
  return (producer == FLASH_PRODUCER);
}

bool is_flash_or_mobile_producer(const std::string& producer)
{
  return (is_flash_producer(producer) || is_mobile_producer(producer));
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    for (const auto& tloc : query.loptions->locations())
    {
      std::string locationId = get_location_id(tloc.loc);

      add_data_to_table(query.poptions.parameters(), tf, outputData, locationId, startRow);
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION

TimeSeriesByLocation get_timeseries_by_fmisid(const std::string& producer,
                                              const ts::TimeSeriesVectorPtr& observation_result,
                                              const Engine::Observation::Settings& settings,
                                              const Query& query,
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
        location_indexes.emplace_back(
            std::pair<unsigned int, unsigned int>(start_index, end_index));
        start_index = end_index = i;
      }
    }
    end_index = fmisid_ts.size();
    location_indexes.emplace_back(std::pair<unsigned int, unsigned int>(start_index, end_index));

    // iterate through locations and do aggregation
    for (unsigned int i = 0; i < location_indexes.size(); i++)
    {
      ts::TimeSeriesVectorPtr tsv(new ts::TimeSeriesVector());
      ts::TimeSeries ts;
      start_index = location_indexes[i].first;
      end_index = location_indexes[i].second;
      for (unsigned int k = 0; k < observation_result->size(); k++)
      {
        const ts::TimeSeries& ts_k = observation_result->at(k);
        if (ts_k.empty())
          tsv->push_back(ts_k);
        else
        {
          ts::TimeSeries ts_ik(ts_k.begin() + start_index, ts_k.begin() + end_index);
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
  for (unsigned int i = 0; i < obsParameters.size(); i++)
  {
    std::string paramname(obsParameters[i].param.name());
    if (paramname == WMO_PARAM || paramname == LPNN_PARAM || paramname == FMISID_PARAM ||
        paramname == RWSID_PARAM || paramname == SENSOR_NO_PARAM || paramname == LEVEL_PARAM ||
        paramname == GEOID_PARAM)
      masterquery.precisions[i] = 0;
  }
}
#endif

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

    for (const AreaProducers& areaproducers : masterquery.timeproducers)
    {
      for (const auto& areaproducer : areaproducers)
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
              // find geoinfo for the corner coordinate
              std::vector<std::string> parts;
              boost::algorithm::split(parts, place, boost::algorithm::is_any_of(","));

              double lon1 = Fmi::stod(parts[0]);
              double lat1 = Fmi::stod(parts[1]);
              double lon2 = Fmi::stod(parts[2]);
              double lat2 = Fmi::stod(parts[3]);

              // get location info for center coordinates
              double lon = (lon1 + lon2) / 2.0;
              double lat = (lat1 + lat2) / 2.0;
              std::unique_ptr<Spine::Location> tmp =
                  get_coordinate_location(lon, lat, query.language, *itsGeoEngine);

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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

        for (NFmiSvgPath::const_iterator it = svgPath->begin(); it != svgPath->end(); ++it)
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

Spine::TimeSeriesGenerator::LocalTimeList Plugin::generateQEngineQueryTimes(
    const Engine::Querydata::Q& q, const Query& query, const std::string& paramname) const
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
      std::string pname = param.name();
      for (const auto& tloc : query.loptions->locations())
      {
        Spine::LocationPtr loc = tloc.loc;
        if (loc->type == Spine::Location::Path || loc->type == Spine::Location::Area)
          loc = getLocationForArea(tloc, query);
        else if (loc->type == Spine::Location::Wkt)
          loc = query.wktGeometries.getLocation(tloc.loc->name);

        std::string val = location_parameter(
            tloc.loc, pname, query.valueformatter, query.timezone, query.precisions[column]);
        data.set(column, row++, val);
      }
      column++;
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
      // find geoinfo for the corner coordinate
      std::vector<std::string> parts;
      boost::algorithm::split(parts, place, boost::algorithm::is_any_of(","));

      double lon1 = Fmi::stod(parts[0]);
      double lat1 = Fmi::stod(parts[1]);
      double lon2 = Fmi::stod(parts[2]);
      double lat2 = Fmi::stod(parts[3]);

      // get location info for center coordinate
      double lon = (lon1 + lon2) / 2.0;
      double lat = (lat1 + lat2) / 2.0;
      std::unique_ptr<Spine::Location> tmp =
          get_coordinate_location(lon, lat, query.language, *itsGeoEngine);

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
      Spine::Exception ex(BCP, "No data available for " + place);
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
    std::string levelType(loadDataLevels ? "data:" : "");
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

      auto querydata_tlist = generateQEngineQueryTimes(qi, query, paramname);

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
      for (const local_date_time& ldt : tlist)
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
            Spine::LocationList llist;

            if (isWkt)
            {
              OGRwkbGeometryType geomType =
                  query.wktGeometries.getGeometry(tloc.loc->name)->getGeometryType();
              if (geomType == wkbMultiLineString)
              {
                // OGRMultiLineString -> handle each LineString separately
                std::list<NFmiSvgPath> svgList = query.wktGeometries.getSvgPaths(tloc.loc->name);
                for (auto svg : svgList)
                {
                  Spine::LocationList ll =
                      get_location_list(svg, tloc.tag, query.step, *itsGeoEngine);
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
                llist = get_location_list(svgPath, tloc.tag, query.step, *itsGeoEngine);
              }
            }
            else
            {
              llist = get_location_list(svgPath, tloc.tag, query.step, *itsGeoEngine);
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

            // list of locations, list of local times
            querydata_result =
                loadDataLevels
                    ? qi->values(querydata_param, llist, querydata_tlist, query.maxdistance)
                    : pressure ? qi->valuesAtPressure(querydata_param,
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
              // if the value is not dependent on location inside area
              // we just need to have the first one
              if (!parameter_is_arithmetic(paramfunc.parameter))
              {
                auto dataIndependentValue = querydata_result->at(0);
                querydata_result->clear();
                querydata_result->push_back(dataIndependentValue);
              }

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
                throw Spine::Exception(BCP,
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
              std::pair<double, double> firstCorner(lon1, lat1);
              std::pair<double, double> secondCorner(lon2, lat2);
              make_rectangle_path(boundingBoxPath, firstCorner, secondCorner);

              mask = NFmiIndexMaskTools::MaskExpand(qi->grid(), boundingBoxPath, loc->radius);
            }
            else if (loc->type == Spine::Location::Area || loc->type == Spine::Location::Place ||
                     loc->type == Spine::Location::CoordinatePoint)
            {
              if (!isWkt)  // SVG for WKT has been extracted earlier
                get_svg_path(tloc, itsGeometryStorage, svgPath);
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
              // if the value is not dependent on location inside
              // area we just need to have the first one
              if (!parameter_is_arithmetic(paramfunc.parameter))
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

            if (parameter_columns.find(parameter.name()) != parameter_columns.end())
            {
              ret.push_back(ObsParameter(
                  parameter, paramfuncs.functions, parameter_columns.at(parameter.name()), true));
              column_index++;
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
                                  Query& query) const
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
                                    Query& query) const
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
          (is_location_parameter(obsParameters[i].param.name()) &&
           !is_flash_or_mobile_producer(producer)) ||
          SmartMet::Engine::Observation::is_time_parameter(obsParameters[i].param.name()))
        continue;

      // fmisid must be always included (except for flash) in queries in order to get location info
      // from geonames
      if (param.name() == FMISID_PARAM && !is_flash_or_mobile_producer(producer))
        fmisid_index = settings.parameters.size();

      // all parameters are fetched at once
      settings.parameters.push_back(param);
    }

    std::string loc_name_original = (tloc.loc ? tloc.loc->name : "");
    std::string loc_name = (tloc.loc ? get_name_base(tloc.loc->name) : "");

    Spine::LocationPtr loc = tloc.loc;
    bool isWkt = (loc && loc->type == Spine::Location::Wkt);
    if (isWkt)
      loc = query.wktGeometries.getLocation(loc_name_original);

    // Area handling:
    // 1) Get OGRGeometry object, expand it with radius
    // 2) Fetch stations geoids from ObsEngine located inside the area
    // 3) Fetch location info from GeoEngine. The info will be used in
    // responses to location and time related queries
    // 4) Fetch the observations from ObsEngine using the geoids
    if (loc)
    {
      if (loc->type == Spine::Location::Path)
      {
        const OGRGeometry* pGeo = 0;
        // if no radius has been given use 200 meters
        double radius = (loc->radius == 0 ? 200 : loc->radius * 1000);
        std::string wktString;

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
              throw Spine::Exception(
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
              throw Spine::Exception(BCP, "Path " + loc_name + " not found in PostGIS database!");

            std::unique_ptr<OGRGeometry> poly;
            poly.reset(Fmi::OGR::expandGeometry(pGeo, radius));
            wktString = Fmi::OGR::exportToWkt(*poly);
          }
        }

        settings.wktArea = wktString;
        settings.area_geoids = get_geoids_for_wkt(itsObsEngine, producer, wktString);

#ifdef MYDEBUG
        std::cout << "WKT of buffered area: " << std::endl << wktString << std::endl;
        std::cout << "#" << settings.area_geoids.size() << " stations found" << std::endl;
        for (auto g : settings.area_geoids)
          std::cout << "geoid: " << g << std::endl;
#endif

        query.maxdistance = 0;
      }
      else if (loc->type == Spine::Location::Area)
      {
#ifdef MYDEBUG
        std::cout << loc_name << " is an Area" << std::endl;
#endif

        const OGRGeometry* pGeo = 0;
        std::string wktString;

        if (isWkt)
        {
          pGeo = query.wktGeometries.getGeometry(loc_name_original);

          if (!pGeo)
            throw Spine::Exception(BCP, "Area " + loc_name + " is not a WKT geometry!");

          wktString = Fmi::OGR::exportToWkt(*pGeo);
        }
        else
        {
          pGeo = itsGeometryStorage.getOGRGeometry(loc_name, wkbMultiPolygon);

          if (!pGeo)
            throw Spine::Exception(BCP, "Area " + loc_name + " not found in PostGIS database!");

          std::unique_ptr<OGRGeometry> poly;
          poly.reset(Fmi::OGR::expandGeometry(pGeo, loc->radius * 1000));

          wktString = Fmi::OGR::exportToWkt(*poly);
        }

        settings.wktArea = wktString;
        settings.area_geoids = get_geoids_for_wkt(itsObsEngine, producer, wktString);

#ifdef MYDEBUG
        std::cout << "WKT of buffered area: " << std::endl << wktString << std::endl;
        std::cout << "#" << settings.area_geoids.size() << " stations found" << std::endl;
#endif

        query.maxdistance = 0;
      }
      else if (loc->type == Spine::Location::BoundingBox && !is_flash_producer(producer))
      {
#ifdef MYDEBUG
        std::cout << loc_name << " is a BoundingBox" << std::endl;
#endif

        Spine::BoundingBox bbox(loc_name);

        NFmiSvgPath svgPath;
        std::pair<double, double> firstCorner(bbox.xMin, bbox.yMin);
        std::pair<double, double> secondCorner(bbox.xMax, bbox.yMax);
        make_rectangle_path(svgPath, firstCorner, secondCorner);
        std::string wkt;
        for (NFmiSvgPath::const_iterator iter = svgPath.begin(); iter != svgPath.end(); iter++)
        {
          if (wkt.empty())
            wkt += "POLYGON((";
          else
            wkt += ", ";
          wkt += Fmi::to_string(iter->itsX);
          wkt += " ";
          wkt += Fmi::to_string(iter->itsY);
        }
        if (!wkt.empty())
          wkt += "))";

        std::unique_ptr<OGRGeometry> geom = get_ogr_geometry(wkt, loc->radius);
        std::string wktString = Fmi::OGR::exportToWkt(*geom);
        settings.wktArea = wktString;
        settings.area_geoids = get_geoids_for_wkt(itsObsEngine, producer, wktString);

#ifdef MYDEBUG
        std::cout << "WKT of buffered area: " << std::endl << wktString << std::endl;
        std::cout << "#" << settings.area_geoids.size() << " stations found" << std::endl;
#endif

        query.maxdistance = 0;
      }
      else if (!is_flash_producer(producer) && loc->type == Spine::Location::Place &&
               loc->radius > 0)
      {
#ifdef MYDEBUG
        std::cout << loc_name << " is an Area (Place + radius)" << std::endl;
#endif
        const OGRGeometry* pGeo = itsGeometryStorage.getOGRGeometry(loc_name, wkbPoint);

        if (!pGeo)
          throw Spine::Exception(BCP, "Place " + loc_name + " not found in PostGIS database!");

        std::unique_ptr<OGRGeometry> poly;
        poly.reset(Fmi::OGR::expandGeometry(pGeo, loc->radius * 1000));

        std::string wktString = Fmi::OGR::exportToWkt(*poly);
        settings.wktArea = wktString;
        settings.area_geoids = get_geoids_for_wkt(itsObsEngine, producer, wktString);
        query.maxdistance = 0;

#ifdef MYDEBUG
        std::cout << "WKT of buffered area: " << std::endl << wktString << std::endl;
        std::cout << "#" << settings.area_geoids.size() << " stations found" << std::endl;
#endif
      }
      else if (!is_flash_producer(producer) && loc->type == Spine::Location::CoordinatePoint &&
               loc->radius > 0)
      {
#ifdef MYDEBUG
        std::cout << loc_name << " is an Area (coordinate point + radius)" << std::endl;
#endif

        std::string wktString;

        if (isWkt)
        {
          const OGRGeometry* pGeo = query.wktGeometries.getGeometry(loc_name_original);

          if (!pGeo)
            throw Spine::Exception(BCP, "Area " + loc_name + " is not a WKT geometry!");

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

        settings.wktArea = wktString;
        settings.area_geoids = get_geoids_for_wkt(itsObsEngine, producer, wktString);
        query.maxdistance = 0;

#ifdef MYDEBUG
        std::cout << "WKT of buffered area: " << std::endl << wktString << std::endl;
        std::cout << "#" << settings.area_geoids.size() << " stations found" << std::endl;
        for (auto g : settings.area_geoids)
          std::cout << "geoid: " << g << std::endl;
#endif
      }
    }  // if(loc)

    // fmisid must be always included in order to get thelocation info from geonames
    if (fmisid_index == -1 && !is_flash_or_mobile_producer(producer))
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
              << std::endl;
    std::cout << "query.toptions.endTimeUTC: " << (query.toptions.endTimeUTC ? "true" : "false")
              << std::endl;
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
    std::cout << "settings.area_geoids.size(): " << settings.area_geoids.size() << std::endl;
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
    std::cout << "settings.wktArea: " << settings.wktArea << std::endl;
#endif
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void Plugin::setMobileAndExternalDataSettings(Engine::Observation::Settings& settings,
                                              Query& query) const
{
  try
  {
    settings.mobileAndExternalDataFilter = query.mobileAndExternalDataFilter;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
      if (!is_flash_or_mobile_producer(producer))
      {
        loc = get_location(itsGeoEngine, fmisid, FMISID_PARAM, query.language);
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

        if (is_location_parameter(paramname) && !is_flash_or_mobile_producer(producer))
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
        else if (SmartMet::Engine::Observation::is_time_parameter(paramname))
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
      if (query.toptions.all() || is_flash_or_mobile_producer(producer) ||
          producer == SYKE_PRODUCER)
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

    // lets find out actual timesteps: different locations may have different timesteps
    std::vector<boost::local_time::local_date_time> ts_vector;
    std::set<boost::local_time::local_date_time> ts_set;
    const ts::TimeSeries& ts = observation_result->at(0);
    for (const ts::TimedValue& tval : ts)
      ts_set.insert(tval.time);
    ts_vector.insert(ts_vector.end(), ts_set.begin(), ts_set.end());
    std::sort(ts_vector.begin(), ts_vector.end());

    int fmisid_index = get_fmisid_index(settings);

    // separate timeseries of different locations to their own data structures
    TimeSeriesByLocation tsv_area =
        get_timeseries_by_fmisid(producer, observation_result, settings, query, fmisid_index);
    // make sure that all timeseries have the same timesteps
    for (FmisidTSVectorPair& val : tsv_area)
    {
      ts::TimeSeriesVector* tsv = val.second.get();
      for (ts::TimeSeries& ts : *tsv)
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
    for (FmisidTSVectorPair& val : tsv_area)
    {
      ts::TimeSeriesVector* tsv_observation_result = val.second.get();

      ts::TimeSeriesVectorPtr observation_result_with_added_fields(new ts::TimeSeriesVector());
      //		  ts::TimeSeriesVector* tsv_observation_result = observation_result.get();
      const ts::TimeSeries& fmisid_ts = tsv_observation_result->at(fmisid_index);

      // fmisid may be missing for rows for which there is no data. Hence we extract
      // it from the full time timeseries once.
      int fmisid = get_fmisid_value(fmisid_ts);

      Spine::LocationPtr loc = get_location(itsGeoEngine, fmisid, FMISID_PARAM, query.language);

      unsigned int obs_result_field_index = 0;
      for (unsigned int i = 0; i < obsParameters.size(); i++)
      {
        std::string paramname = obsParameters[i].param.name();

        // add data for location fields
        if (is_location_parameter(paramname) && !is_flash_or_mobile_producer(producer))
        {
          ts::TimeSeries location_ts;

          for (unsigned int j = 0; j < ts_vector.size(); j++)
          {
            ts::Value value = location_parameter(loc,
                                                 obsParameters[i].param.name(),
                                                 query.valueformatter,
                                                 query.timezone,
                                                 query.precisions[i]);

            location_ts.push_back(ts::TimedValue(ts_vector[j], value));
          }
          observation_result_with_added_fields->push_back(location_ts);
        }
        else if (SmartMet::Engine::Observation::is_time_parameter(paramname))
        {
          // add data for time fields
          Spine::Location dummyloc(0, 0, "", query.timezone);

          ts::TimeSeriesGroupPtr grp(new ts::TimeSeriesGroup);
          ts::TimeSeries time_ts;
          for (unsigned int j = 0; j < ts_vector.size(); j++)
          {
            std::string paramvalue = time_parameter(paramname,
                                                    ts_vector[j],
                                                    state.getTime(),
                                                    (loc ? *loc : dummyloc),
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
      if (query.toptions.all() || is_flash_or_mobile_producer(producer) ||
          producer == SYKE_PRODUCER)
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    if (settings.area_geoids.size() == 0 || is_flash_or_mobile_producer(producer))
    {
      // fetch data for places
      fetchObsEngineValuesForPlaces(state, producer, obsParameters, settings, query, outputData);
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
      throw Spine::Exception(BCP, "BUG: processObsEngineQuery producer list empty");

    for (const auto producer : areaproducers)
    {
      if (!isObsProducer(producer))
        continue;
      // in contrast to area query
      bool locationQueryExecuted = false;

      // Settings which are the same for all locations
      Engine::Observation::Settings settings;
      setCommonObsSettings(
          settings, producer, producerDataPeriod, state.getTime(), obsParameters, query);

      // Set settings for mobile and external data
      setMobileAndExternalDataSettings(settings, query);

      if (query.loptions->locations().empty())
      {
        // This is not beautiful
        Spine::LocationPtr loc;
        Spine::TaggedLocation tloc("", loc);

        // Update settings for this particular location
        setLocationObsSettings(
            settings, producer, producerDataPeriod, state.getTime(), tloc, obsParameters, query);

        // Single locations are fetched all at once
        // Area locations are fetched area by area
        if (locationQueryExecuted && settings.area_geoids.size() == 0)
          continue;

        std::vector<TimeSeriesData> tsdatavector;
        outputData.push_back(make_pair("_obs_", tsdatavector));

        fetchObsEngineValues(state, producer, tloc, obsParameters, settings, query, outputData);
      }
      else
      {
        for (const auto& tloc : query.loptions->locations())
        {
          // Update settings for this particular location
          setLocationObsSettings(
              settings, producer, producerDataPeriod, state.getTime(), tloc, obsParameters, query);

          // Single locations are fetched all at once
          // Area locations are fetched area by area
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
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    // first timestep is here in utc
    boost::posix_time::ptime first_timestep = masterquery.latestTimestep;

    bool firstProducer = outputData.empty();

    for (const auto& tloc : masterquery.loptions->locations())
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void Plugin::processQuery(const State& state, Spine::Table& table, Query& masterquery)
{
  try
  {
    // if only location related parameters queried, use shortcut
    if (is_plain_location_query(masterquery.poptions.parameters()))
    {
      fetchStaticLocationValues(masterquery, table, 0, 0);
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

    const bool producerMissing = masterquery.timeproducers.empty();
    if (producerMissing)
      masterquery.timeproducers.push_back(AreaProducers());

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
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

    // Options
    high_resolution_clock::time_point t1 = high_resolution_clock::now();
    Query query(state, request, itsConfig);
    high_resolution_clock::time_point t2 = high_resolution_clock::now();

    data.setPaging(query.startrow, query.maxresults);

    std::string producer_option =
        Spine::optional_string(request.getParameter(PRODUCER_PARAM),
                               Spine::optional_string(request.getParameter(STATIONTYPE_PARAM), ""));
    // At least one of location specifiers must be set

#ifndef WITHOUT_OBSERVATION
    if (query.geoids.size() == 0 && query.fmisids.size() == 0 && query.lpnns.size() == 0 &&
        query.wmos.size() == 0 && query.boundingBox.size() == 0 &&
        !is_flash_or_mobile_producer(producer_option) && query.loptions->locations().size() == 0)
#else
    if (query.loptions->locations().size() == 0)
#endif
      throw Spine::Exception(BCP, "No location option given!");

    // The formatter knows which mimetype to send
    boost::shared_ptr<Spine::TableFormatter> formatter(
        Spine::TableFormatterFactory::create(query.format));
    std::string mime = formatter->mimetype() + "; charset=UTF-8";
    response.setHeader("Content-Type", mime.c_str());

    // Calculate the hash value for the product. Zero value implies
    // the product is not cacheable.

    auto product_hash = hash_value(state, query, request);

    high_resolution_clock::time_point t3 = high_resolution_clock::now();

    std::string timeheader = Fmi::to_string(duration_cast<microseconds>(t2 - t1).count()) + '+' +
                             Fmi::to_string(duration_cast<microseconds>(t3 - t2).count());

    if (product_hash != 0)
    {
      response.setHeader("ETag", fmt::format("\"{:x}-timeseries\"", product_hash));

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

    processQuery(state, data, query);

    high_resolution_clock::time_point t4 = high_resolution_clock::now();
    timeheader.append("+").append(
        Fmi::to_string(std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3).count()));

    data.setMissingText(query.valueformatter.missing());

    // The names of the columns
    Spine::TableFormatter::Names headers;
    for (const Spine::Parameter& p : query.poptions.parameters())
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

    std::ostringstream output;
    formatter->format(output, data, headers, request, formatter_options);
    high_resolution_clock::time_point t5 = high_resolution_clock::now();
    timeheader.append("+").append(
        Fmi::to_string(std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4).count()));

    // TODO: Should use std::move when it has become available
    boost::shared_ptr<std::string> result(new std::string());
    std::string tmp = output.str();
    std::swap(tmp, *result);

    // Too many flash data requests with empty output filling the logs...
#if 0    
    if (result->size() == 0)
    {
      std::cerr << "Warning: Empty output for request " << request.getQueryString() << " from "
                << request.getClientIP() << std::endl;
    }
#endif

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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
  // We need this in the catch block
  bool isdebug = false;

  try
  {
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

    Spine::Exception ex(BCP, "Request processing exception!", nullptr);
    ex.addParameter("URI", theRequest.getURI());
    ex.addParameter("ClientIP", theRequest.getClientIP());
    ex.printError();

    if (isdebug)
    {
      // Delivering the exception information as HTTP content
      std::string fullMessage = ex.getHtmlStackTrace();
      theResponse.setContent(fullMessage);
      theResponse.setStatus(Spine::HTTP::Status::ok);
    }
    else
    {
      theResponse.setStatus(Spine::HTTP::Status::bad_request);
    }

    // Adding the first exception information into the response header

    std::string firstMessage = ex.what();
    boost::algorithm::replace_all(firstMessage, "\n", " ");
    firstMessage = firstMessage.substr(0, 300);
    theResponse.setHeader("X-TimeSeriesPlugin-Error", firstMessage.c_str());
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

    /* GeoEngine */
    auto engine = itsReactor->getSingleton("Geonames", nullptr);
    if (!engine)
      throw Spine::Exception(BCP, "Geonames engine unavailable");
    itsGeoEngine = reinterpret_cast<Engine::Geonames::Engine*>(engine);

    /* GisEngine */
    engine = itsReactor->getSingleton("Gis", nullptr);
    if (!engine)
      throw Spine::Exception(BCP, "Gis engine unavailable");
    itsGisEngine = reinterpret_cast<Engine::Gis::Engine*>(engine);

    // Read the geometries from PostGIS database
    itsGisEngine->populateGeometryStorage(itsConfig.getPostGISIdentifiers(), itsGeometryStorage);

    /* QEngine */
    engine = itsReactor->getSingleton("Querydata", nullptr);
    if (!engine)
      throw Spine::Exception(BCP, "Querydata engine unavailable");
    itsQEngine = reinterpret_cast<Engine::Querydata::Engine*>(engine);

#ifndef WITHOUT_OBSERVATION
    if (!itsConfig.obsEngineDisabled())
    {
      /* ObsEngine */
      engine = itsReactor->getSingleton("Observation", nullptr);
      if (!engine)
        throw Spine::Exception(BCP, "Observation engine unavailable");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    if (itsCache != nullptr)
      itsCache->shutdown();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Destructor
 */
// ----------------------------------------------------------------------

Plugin::~Plugin()
{
  if (itsCache != nullptr)
    itsCache->shutdown();
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
