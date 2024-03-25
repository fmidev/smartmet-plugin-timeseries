#include "QueryProcessingHub.h"
#include "GridInterface.h"
#include "LocationTools.h"
#include "LonLatDistance.h"
#include "Plugin.h"
#include "PostProcessing.h"
#include "State.h"
#include <grid-files/common/GeneralFunctions.h>
#include <macgyver/Hash.h>
#include <timeseries/ParameterKeywords.h>
#include <timeseries/ParameterTools.h>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
namespace
{
void parameters_hash_value(const Spine::HTTP::Request& request, std::size_t& hash)
{
  try
  {
    for (const auto& name_value : request.getParameterMap())
    {
      const auto& name = name_value.first;
      if (name != "hour" && name != "time" && name != "day" && name != "timestep" &&
          name != "timesteps" && name != "starttime" && name != "startstep" && name != "endtime")
      {
        Fmi::hash_combine(hash, Fmi::hash_value(name_value.first));
        Fmi::hash_combine(hash, Fmi::hash_value(name_value.second));
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

#ifndef WITHOUT_OBSERVATION
bool obs_producers_exists(const Query& masterquery, ObsEngineQuery obsEngineQuery)
{
  try
  {
    for (const AreaProducers& areaproducers : masterquery.timeproducers)
    {
      for (const auto& areaproducer : areaproducers)
      {
        if (obsEngineQuery.isObsProducer(areaproducer))
          return true;
      }
    }
    return false;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
#endif

Spine::LocationPtr get_loc(const Query& masterquery,
                           const Query q,
                           const Spine::TaggedLocation& tloc,
                           const Engine::Geonames::Engine& geoEngine,
                           const Engine::Gis::GeometryStorage& geometryStorage)
{
  try
  {
    Spine::LocationPtr loc = tloc.loc;

    std::string place = get_name_base(loc->name);
    if (loc->type == Spine::Location::Wkt)
    {
      loc = masterquery.wktGeometries.getLocation(tloc.loc->name);
    }
    else if (loc->type == Spine::Location::Path || loc->type == Spine::Location::Area)
    {
      NFmiSvgPath svgPath;
      loc = get_location_for_area(tloc, geometryStorage, q.language, geoEngine, &svgPath);
    }
    else if (loc->type == Spine::Location::BoundingBox)
    {
      // get location info for center coordinates
      std::unique_ptr<Spine::Location> tmp = get_bbox_location(place, q.language, geoEngine);

      tmp->name = tloc.tag;
      tmp->type = tloc.loc->type;
      tmp->radius = tloc.loc->radius;
      loc.reset(tmp.release());
    }

    return loc;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Spine::LocationPtr get_nearest_loc(const Query& masterquery, const Spine::TaggedLocation& tloc)
{
  try
  {
    // Find nearest location
    Spine::LocationPtr nearest_loc = nullptr;

    std::pair<double, double> from_location(tloc.loc->longitude, tloc.loc->latitude);
    double distance = -1;
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

    return nearest_loc;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Spine::TaggedLocationList get_tloc_list(const Query& masterquery,
                                        const Spine::TaggedLocation& tloc,
                                        const Engine::Gis::GeometryStorage& geometryStorage)
{
  try
  {
    //	Spine::TaggedLocationList ret;

    if (tloc.loc->type == Spine::Location::Wkt)
    {
      const OGRGeometry* geom = masterquery.wktGeometries.getGeometry(tloc.loc->name);
      if (geom)
        return get_locations_inside_geometry(masterquery.inKeywordLocations, *geom);

      return {};
    }

    if (tloc.loc->type == Spine::Location::Area)
    {
      // Find locations inside Area
      const OGRGeometry* geom = get_ogr_geometry(tloc, geometryStorage);
      if (geom)
        return get_locations_inside_geometry(masterquery.inKeywordLocations, *geom);

      return {};
    }

    if (tloc.loc->type == Spine::Location::BoundingBox)
    {
      // Find locations inside Bounding Box
      Spine::BoundingBox bbox(get_name_base(tloc.loc->name));

      std::string wkt = ("POLYGON((" + Fmi::to_string(bbox.xMin) + " " + Fmi::to_string(bbox.yMin) +
                         "," + Fmi::to_string(bbox.xMin) + " " + Fmi::to_string(bbox.yMax) + "," +
                         Fmi::to_string(bbox.xMax) + " " + Fmi::to_string(bbox.yMax) + "," +
                         Fmi::to_string(bbox.xMax) + " " + Fmi::to_string(bbox.yMin) + "," +
                         Fmi::to_string(bbox.xMin) + " " + Fmi::to_string(bbox.yMin) + "))");
      std::unique_ptr<OGRGeometry> geom = get_ogr_geometry(wkt);
      if (geom)
        return get_locations_inside_geometry(masterquery.inKeywordLocations, *geom);

      return {};
    }

    if (tloc.loc->type == Spine::Location::CoordinatePoint ||
        tloc.loc->type == Spine::Location::Place)
    {
      if (tloc.loc->radius == 0)
      {
        auto nearest_loc = get_nearest_loc(masterquery, tloc);

        if (nearest_loc)
        {
          Spine::TaggedLocationList ret;
          ret.emplace_back(Spine::TaggedLocation(nearest_loc->name, nearest_loc));
          return ret;
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
          return get_locations_inside_geometry(masterquery.inKeywordLocations, *geom);
      }
    }

    return {};
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void check_in_keyword_locations(Query& masterquery,
                                const Engine::Gis::GeometryStorage& geometryStorage)
{
  try
  {
    // If inkeyword given resolve locations
    if (!masterquery.inKeywordLocations.empty())
    {
      Spine::TaggedLocationList tloc_list;
      for (const auto& tloc : masterquery.loptions->locations())
      {
        auto tlocs = get_tloc_list(masterquery, tloc, geometryStorage);
        tloc_list.insert(tloc_list.end(), tlocs.begin(), tlocs.end());
      }
      masterquery.loptions->setLocations(tloc_list);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void fetch_static_location_values(const Query& query,
                                  const Engine::Geonames::Engine& geoEngine,
                                  const Engine::Gis::GeometryStorage& geometryStorage,
                                  Spine::Table& data)
{
  try
  {
    unsigned int column = 0;
    unsigned int row = 0;

    for (const Spine::Parameter& param : query.poptions.parameters())
    {
      row = 0;
      const auto& pname = param.name();
      for (const auto& tloc : query.loptions->locations())
      {
        Spine::LocationPtr loc = tloc.loc;
        if (loc->type == Spine::Location::Path || loc->type == Spine::Location::Area)
          loc = get_location_for_area(tloc, geometryStorage, query.language, geoEngine);
        else if (loc->type == Spine::Location::Wkt)
          loc = query.wktGeometries.getLocation(tloc.loc->name);

        std::string val = TS::location_parameter(
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

}  // namespace

QueryProcessingHub::QueryProcessingHub(const Plugin& thePlugin)
    : itsQEngineQuery(thePlugin), itsObsEngineQuery(thePlugin), itsGridEngineQuery(thePlugin)
{
}

boost::shared_ptr<std::string> QueryProcessingHub::processQuery(
    const State& state,
    Spine::Table& table,
    Query& masterquery,
    const QueryServer::QueryStreamer_sptr& queryStreamer,
    size_t& product_hash)
{
  try
  {
    const auto& thePlugin = state.getPlugin();
    const auto& theEngines = thePlugin.itsEngines;

    check_in_keyword_locations(masterquery, thePlugin.itsGeometryStorage);

    // if only location related parameters queried, use shortcut
    if (TS::is_plain_location_query(masterquery.poptions.parameters()))
    {
      fetch_static_location_values(
          masterquery, *theEngines.geoEngine, thePlugin.itsGeometryStorage, table);
      return {};
    }

    ProducerDataPeriod producerDataPeriod;

    // producerDataPeriod contains information of data periods of different producers
#ifndef WITHOUT_OBSERVATION
    producerDataPeriod.init(
        state, *theEngines.qEngine, theEngines.obsEngine, masterquery.timeproducers);
#else
    producerDataPeriod.init(state, *theEngines.qEngine, masterquery.timeproducers);
#endif

    // the result data is stored here during the query
    TS::OutputData outputData;

    const bool producerMissing = masterquery.timeproducers.empty();
    if (producerMissing)
      masterquery.timeproducers.emplace_back(AreaProducers());

#ifndef WITHOUT_OBSERVATION
    const ObsParameters obsParameters = itsObsEngineQuery.getObsParameters(masterquery);
#endif

    Fmi::DateTime latestTimestep = masterquery.latestTimestep;
    bool startTimeUTC = masterquery.toptions.startTimeUTC;

    // This loop will iterate through the producers, collecting as much
    // data in order as is possible. The later producers patch the data
    // *after* the first ones if possible.

    std::size_t producer_group = 0;
    for (const AreaProducers& areaproducers : masterquery.timeproducers)
    {
      Query q = masterquery;

      q.timeproducers.clear();  // not used
      // set latestTimestep for the query
      q.latestTimestep = latestTimestep;

      if (producer_group != 0)
        q.toptions.startTimeUTC = startTimeUTC;
      q.toptions.endTimeUTC = masterquery.toptions.endTimeUTC;

#ifndef WITHOUT_OBSERVATION
      if (!areaproducers.empty() && !thePlugin.itsConfig.obsEngineDisabled() &&
          itsObsEngineQuery.isObsProducer(areaproducers.front()))
      {
        itsObsEngineQuery.processObsEngineQuery(
            state, q, outputData, areaproducers, producerDataPeriod, obsParameters);
      }
      else
#endif
          if (itsGridEngineQuery.isGridEngineQuery(areaproducers, masterquery))
      {
        bool processed = itsGridEngineQuery.processGridEngineQuery(
            state, q, outputData, queryStreamer, areaproducers, producerDataPeriod);

        if (processed)
        {
          // We need different hash calculcations for the grid requests.
          product_hash = Fmi::bad_hash;
        }
        // If the query was not processed then we should call the QEngine instead.
        else
        {
          itsQEngineQuery.processQEngineQuery(
              state, q, outputData, areaproducers, producerDataPeriod);
        }
      }
      else
      {
        itsQEngineQuery.processQEngineQuery(
            state, q, outputData, areaproducers, producerDataPeriod);
      }

      // get the latestTimestep from previous query
      latestTimestep = q.latestTimestep;
      startTimeUTC = q.toptions.startTimeUTC;
      ++producer_group;
    }

#ifndef WITHOUT_OBSERVATION
    PostProcessing::fix_precisions(masterquery, obsParameters);
#endif

    // insert data into the table
    PostProcessing::fill_table(masterquery, outputData, table);

    return nullptr;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

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
std::size_t QueryProcessingHub::hash_value(const State& state,
                                           const Spine::HTTP::Request& request,
                                           Query masterquery) const
{
  try
  {
    // Initial hash value = geonames hash (may change during reloads) + querystring hash
    auto hash = state.getGeoEngine().hash_value();

    // Calculate a hash for the query. We can ignore the time series options
    // since we later on generate a hash for the generated time series.
    // In particular this permits us to ignore the endtime=x setting, which
    // Ilmanet sets to a precision of one second.
    parameters_hash_value(request, hash);

    // If the query depends on locations only, that's it!

    if (TS::is_plain_location_query(masterquery.poptions.parameters()))
      return hash;

#ifndef WITHOUT_OBSERVATION

    // Check here first if any of the producers is an observation.
    // If so, return zero
    if (obs_producers_exists(masterquery, itsObsEngineQuery))
      return Fmi::bad_hash;

#endif

    // Maintain a list of handled querydata/timezone combinations

    std::set<std::size_t> handled_timeseries;

    // Basically we mimic what is done in processQuery as far as is needed

    ProducerDataPeriod producerDataPeriod;

    // producerDataPeriod contains information of data periods of different producers

    const auto& thePlugin = state.getPlugin();
#ifndef WITHOUT_OBSERVATION
    producerDataPeriod.init(state,
                            *thePlugin.itsEngines.qEngine,
                            thePlugin.itsEngines.obsEngine,
                            masterquery.timeproducers);
#else
    producerDataPeriod.init(state, *thePlugin.itsEngines.qEngine, masterquery.timeproducers);
#endif

    // the result data is stored here during the query

    bool producerMissing = (masterquery.timeproducers.empty());

    if (producerMissing)
    {
      masterquery.timeproducers.emplace_back(AreaProducers());
    }

    Fmi::DateTime latestTimestep = masterquery.latestTimestep;

    bool startTimeUTC = masterquery.toptions.startTimeUTC;

    // This loop will iterate through the producers.

    std::size_t producer_group = 0;
    bool firstProducer = true;

    for (const AreaProducers& areaproducers : masterquery.timeproducers)
    {
      Query q = masterquery;

      q.timeproducers.clear();  // not used
      // set latestTimestep for the query
      q.latestTimestep = latestTimestep;

      if (producer_group != 0)
        q.toptions.startTimeUTC = startTimeUTC;
      q.toptions.endTimeUTC = masterquery.toptions.endTimeUTC;

#ifndef WITHOUT_OBSERVATION
      if (!areaproducers.empty() && !thePlugin.itsConfig.obsEngineDisabled() &&
          itsObsEngineQuery.isObsProducer(areaproducers.front()))
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
        Fmi::DateTime first_timestep = q.latestTimestep;

        for (const auto& tloc : q.loptions->locations())
        {
          Query subquery = q;

          if (subquery.timezone == LOCALTIME_PARAM)
            subquery.timezone = tloc.loc->timezone;

          subquery.toptions.startTime = first_timestep;

          if (!firstProducer)
            subquery.toptions.startTime += Fmi::Minutes(1);  // WHY???????
          firstProducer = false;

          // producer can be alias, get actual producer
          std::string producer(
              itsQEngineQuery.selectProducer(*(tloc.loc), subquery, areaproducers));
          bool isClimatologyProducer =
              (producer.empty()
                   ? false
                   : thePlugin.itsEngines.qEngine->getProducerConfig(producer).isclimatology);

          Fmi::LocalDateTime data_period_endtime(producerDataPeriod.getLocalEndTime(
              producer, subquery.timezone, thePlugin.getTimeZones()));

          // We do not need to iterate over the parameters here like processQEngineQuery does

          // every parameter starts from the same row
          if (!data_period_endtime.is_not_a_date_time()
               && subquery.toptions.endTime > data_period_endtime.local_time()
               && !isClimatologyProducer)
          {
            subquery.toptions.endTime = data_period_endtime.local_time();
          }

          {
            // Emulate fetchQEngineValues here
            Spine::LocationPtr loc = get_loc(masterquery,
                                             q,
                                             tloc,
                                             *thePlugin.itsEngines.geoEngine,
                                             thePlugin.itsGeometryStorage);

            if (subquery.timezone == LOCALTIME_PARAM)
              subquery.timezone = loc->timezone;

            // Select the producer for the coordinate
            producer = itsQEngineQuery.selectProducer(*loc, subquery, areaproducers);

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

            auto pos_flag = handled_timeseries.insert(timeseries_hash);

            if (pos_flag.second)  // if insert was successful
            {
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

              auto tz = thePlugin.getTimeZones().time_zone_from_string(subquery.timezone);
              auto tlist = thePlugin.itsTimeSeriesCache->generate(subquery.toptions, tz);

              // This is enough to generate an unique hash for the request, even
              // though the timesteps may not be exactly the same as those used
              // in generating the result.

              Fmi::hash_combine(hash, querydata_hash);
              Fmi::hash_combine(hash, Fmi::hash_value(*tlist));
            }
          }

          // get the latest_timestep from previous query
          q.latestTimestep = subquery.latestTimestep;
          q.lastpoint = subquery.lastpoint;
        }
      }

      // get the latestTimestep from previous query
      latestTimestep = q.latestTimestep;

      startTimeUTC = q.toptions.startTimeUTC;
      ++producer_group;
    }

    return hash;
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
