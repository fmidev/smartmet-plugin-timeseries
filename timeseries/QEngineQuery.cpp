#include "QEngineQuery.h"
#include "State.h"
#include "LocationTools.h"
#include "UtilityFunctions.h"
#include "PostProcessing.h"
#include <timeseries/ParameterKeywords.h>
#include <timeseries/ParameterTools.h>
#include <timeseries/TableFeeder.h>
#include <timeseries/TimeSeriesInclude.h>
#include <timeseries/TimeSeriesOutput.h>
#include <macgyver/Exception.h>
#include <newbase/NFmiIndexMaskTools.h>
#include <newbase/NFmiSvgTools.h>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
namespace
{

bool is_wkt_area(const Spine::LocationPtr& loc)
{
  try
	{
	  if(loc->type == Spine::Location::Wkt)
		{
		  auto geom = get_ogr_geometry(loc->name, loc->radius);

		  if(!geom)
			return false;

		  auto type = geom->getGeometryType();
	
		  return (type == wkbPolygon || type == wkbMultiPolygon);
		}

		return false;
	}
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}


bool is_point_query(const Spine::LocationPtr& loc)
{
  try
	{
	  if((loc->type == Spine::Location::Place || loc->type == Spine::Location::CoordinatePoint) && loc->radius == 0)
		return true;

	  if(loc->type == Spine::Location::Wkt)
		{
		  auto geom = get_ogr_geometry(loc->name, loc->radius);		  
		  return (geom && geom->getGeometryType() == wkbPoint);
		}
		return false;
	}
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}


Spine::LocationList get_indexmask_locations(const NFmiIndexMask& indexmask,
                                            const Spine::LocationPtr& loc,
                                            const Engine::Querydata::Q& qi,
                                            const Engine::Geonames::Engine& geoengine)
{
  try
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
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Spine::TaggedLocationList get_locations_for_area(const NFmiIndexMask& indexmask,
												 const Spine::TaggedLocation& tloc,
												 const Spine::LocationPtr& area_loc,
												 const Engine::Querydata::Q& qi,
												 const Engine::Geonames::Engine& geoengine)
{
  try
	{
	  Spine::TaggedLocationList tloclist;

	  for (const auto& mask : indexmask)
		{
		  NFmiPoint coord = qi->latLon(mask);
		  Spine::Location location(*area_loc);
		  location.longitude = coord.X();
		  location.latitude = coord.Y();
		  location.dem = geoengine.demHeight(location.longitude, location.latitude);
		  location.covertype = geoengine.coverType(location.longitude, location.latitude);
		  location.type = Spine::Location::CoordinatePoint;
		  location.radius = 0;
		  Spine::LocationPtr locPtr = boost::make_shared<Spine::Location>(location);
		  Spine::TaggedLocation new_tloc(tloc.tag, locPtr);
		  tloclist.emplace_back(new_tloc);
		}

	  return tloclist;
	}
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

NFmiIndexMask get_bbox_indexmask(const Spine::LocationPtr& loc,
								 const Engine::Querydata::Q& qi)
{
  try
	{
	  NFmiIndexMask indexmask;

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

	  return indexmask;
 	}
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
} 

NFmiIndexMask get_area_indexmask(const Spine::TaggedLocation& tloc,
								 const Spine::LocationPtr& loc,
								 const Engine::Querydata::Q& qi,
								 const Engine::Gis::GeometryStorage& geometryStorage,
								 NFmiSvgPath& svgPath)
{
  try
	{
	  NFmiIndexMask indexmask;

	  if (loc->type != Spine::Location::Wkt)  // SVG for WKT has been extracted earlier
		get_svg_path(tloc, geometryStorage, svgPath);
	  indexmask = NFmiIndexMaskTools::MaskExpand(qi->grid(), svgPath, loc->radius);

	  return indexmask;
 	}
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
} 

Spine::TaggedLocationList get_tloclist(const Query& query,
									   const Spine::TaggedLocation& tloc,
									   const Spine::LocationPtr& loc,
									   const Spine::LocationPtr& area_loc,
									   const Engine::Querydata::Q& qi,
									   const Engine::Geonames::Engine& geoengine,
									   const Engine::Gis::GeometryStorage& geometryStorage,
									   bool bbox_area,
									   NFmiSvgPath& svgPath)
{
  try
  {
	if (loc->type == Spine::Location::Wkt)
	  svgPath = query.wktGeometries.getSvgPath(tloc.loc->name);
	
	NFmiIndexMask indexmask;
	
	if(bbox_area)
	  indexmask = get_bbox_indexmask(loc, qi);
	else 
	  indexmask = get_area_indexmask(tloc,
									 loc,
									 qi,
									 geometryStorage,
									 svgPath);
	
	return get_locations_for_area(indexmask,
								  tloc,
								  area_loc,
								  qi,
								  geoengine);	
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
} 


} // anonymous

QEngineQuery::QEngineQuery(const Plugin& thePlugin) : itsPlugin(thePlugin)
{
}

// If query.groupareas is false, find out locations inside area and process them individaually
void QEngineQuery::resolveAreaLocations(Query& query,
										const State& state,
										const AreaProducers& areaproducers) const
{
  if (query.groupareas)
    return;

  Spine::TaggedLocationList tloclist;
  for (const auto& tloc : query.loptions->locations())
  {
    Spine::LocationPtr loc = tloc.loc;
    Spine::LocationPtr area_loc;
	NFmiSvgPath svgPath;

    if (tloc.loc->type == Spine::Location::BoundingBox)
      area_loc = get_bbox_location(get_name_base(tloc.loc->name), query.language, *itsPlugin.itsEngines.geoEngine);
    else
      area_loc = get_location_for_area(tloc, itsPlugin.itsGeometryStorage, query.language, *itsPlugin.itsEngines.geoEngine, &svgPath);

    auto producer = selectProducer(*area_loc, query, areaproducers);

    if (producer.empty())
      return;

    auto qi = (query.origintime ? state.get(producer, *query.origintime) : state.get(producer));

	bool bbox_area = (qi->isGrid() && loc->type == Spine::Location::BoundingBox);
	bool area_area = (qi->isGrid() && (loc->type == Spine::Location::Area || 
									   is_wkt_area(loc) ||
									   ((loc->type == Spine::Location::Place || 
										 loc->type == Spine::Location::CoordinatePoint) &&
										loc->radius > 0)));

	if(bbox_area || area_area)
	  {
		auto tlocs = get_tloclist(query,
								  tloc,
								  loc,
								  area_loc,
								  qi,
								  *itsPlugin.itsEngines.geoEngine,
								  itsPlugin.itsGeometryStorage,
								  bbox_area,
								  svgPath);

		tloclist.insert(tloclist.end(), tlocs.begin(), tlocs.end());
	  }
    else
	  {
		tloclist.emplace_back(tloc);
	  }
  }

  query.loptions->setLocations(tloclist);
}


void QEngineQuery::processQEngineQuery(const State& state,
									   Query& masterquery,
									   TS::OutputData& outputData,
									   const AreaProducers& areaproducers,
									   const ProducerDataPeriod& producerDataPeriod) const
{
  try
  {
    // If user wants to get grid points of area to separate lines, resolve coordinates inside area
    if (!masterquery.groupareas)
      resolveAreaLocations(masterquery, state, areaproducers);

    check_request_limit(itsPlugin.itsConfig.requestLimits(),
                        masterquery.poptions.parameterFunctions().size(),
                        TS::RequestLimitMember::PARAMETERS);

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

      Query q = masterquery;
      QueryLevelDataCache queryLevelDataCache;

      std::vector<TS::TimeSeriesData> tsdatavector;
      outputData.emplace_back(make_pair(location_id, tsdatavector));

      if (masterquery.timezone == LOCALTIME_PARAM)
        q.timezone = tloc.loc->timezone;

      q.toptions.startTime = first_timestep;
      if (!firstProducer)
        q.toptions.startTime += boost::posix_time::minutes(1);

      // producer can be alias, get actual producer
      std::string producer(selectProducer(*(tloc.loc), q, areaproducers));
      bool isClimatologyProducer =
          (producer.empty() ? false : itsPlugin.itsEngines.qEngine->getProducerConfig(producer).isclimatology);

      boost::local_time::local_date_time data_period_endtime(
          producerDataPeriod.getLocalEndTime(producer, q.timezone, itsPlugin.itsEngines.geoEngine->getTimeZones()));

      // Reset for each new location, since fetchQEngineValues modifies it
      auto old_start_time = q.toptions.startTime;

      int column = 0;
      for (const TS::ParameterAndFunctions& paramfunc : q.poptions.parameterFunctions())
      {
        // reset to original start time for each new location
        q.toptions.startTime = old_start_time;

        // every parameter starts from the same row
        if (q.toptions.endTime > data_period_endtime.local_time() &&
            !data_period_endtime.is_not_a_date_time() && !isClimatologyProducer)
        {
          q.toptions.endTime = data_period_endtime.local_time();
        }
        fetchQEngineValues(state,
                           paramfunc,
                           q.precisions[column],
                           tloc,
                           q,
                           areaproducers,
                           producerDataPeriod,
                           queryLevelDataCache,
                           outputData);
        column++;
        check_request_limit(itsPlugin.itsConfig.requestLimits(),
                            TS::number_of_elements(outputData),
                            TS::RequestLimitMember::ELEMENTS);
      }
      // get the latest_timestep from previous query
      masterquery.latestTimestep = q.latestTimestep;
      masterquery.lastpoint = q.lastpoint;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void QEngineQuery::fetchQEngineValues(const State& state,
									  const TS::ParameterAndFunctions& paramfunc,
									  int precision,
									  const Spine::TaggedLocation& tloc,
									  Query& query,
									  const AreaProducers& areaproducers,
									  const ProducerDataPeriod& producerDataPeriod,
									  QueryLevelDataCache& queryLevelDataCache,
									  TS::OutputData& outputData) const
{
  try
  {
    Spine::LocationPtr loc = tloc.loc;
    std::string place = get_name_base(loc->name);
    std::string paramname = paramfunc.parameter.name();

    NFmiSvgPath svgPath;
    bool isWkt = false;
	loc = resolveLocation(tloc, query, svgPath, isWkt);

    if (query.timezone == LOCALTIME_PARAM)
      query.timezone = loc->timezone;

    // Select the producer for the coordinate
    auto producer = selectProducer(*loc, query, areaproducers);

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

    // No area operations allowed for non-grid data
	bool isPointQuery = is_point_query(loc);
    if (!qi->isGrid() && !isPointQuery)
      return;

    // If we accept nearest valid points, find it now for this location
    // This is fast if the nearest point is already valid
    NFmiPoint nearestpoint(kFloatMissing, kFloatMissing);
    if (query.findnearestvalidpoint)
    {
      NFmiPoint latlon(loc->longitude, loc->latitude);
      nearestpoint = qi->validPoint(latlon, query.maxdistance_kilometers());
    }

    std::string country = state.getGeoEngine().countryName(loc->iso2, query.language);

    std::vector<TS::TimeSeriesData> aggregatedData;  // store here data of all levels

    // If no pressures/heights are chosen, loading all or just chosen data levels.
    //
    // Otherwise loading chosen data levels if any, then the chosen pressure
    // and/or height levels (interpolated values at chosen pressures/heights)

    bool loadDataLevels =
        (!query.levels.empty() || (query.pressures.empty() && query.heights.empty()));
    std::string levelType(loadDataLevels ? "data:" : "");
    auto itPressure = query.pressures.begin();
    auto itHeight = query.heights.begin();

    if (loadDataLevels)
      check_request_limit(
          itsPlugin.itsConfig.requestLimits(), query.levels.size(), TS::RequestLimitMember::LEVELS);
    if (itPressure != query.pressures.end())
      check_request_limit(
          itsPlugin.itsConfig.requestLimits(), query.pressures.size(), TS::RequestLimitMember::LEVELS);
    if (itHeight != query.heights.end())
      check_request_limit(
          itsPlugin.itsConfig.requestLimits(), query.heights.size(), TS::RequestLimitMember::LEVELS);

    std::set<int> received_levels;
    // Loop over the levels
    for (qi->resetLevel();;)
    {
      boost::optional<float> pressure;
      boost::optional<float> height;
      float levelValue = 0;

      if (loadDataLevels)
      {
        if (!qi->nextLevel())
		  {
			// No more native/data levels; load/interpolate pressure and height
			// levels if any
			loadDataLevels = false;
		  }
        else
		  {
			// check if only some levels are chosen
			int level = static_cast<int>(qi->levelValue());
			if (!query.levels.empty())
			  {
				if (query.levels.find(level) == query.levels.end())
				  continue;
			  }
			received_levels.insert(level);
			check_request_limit(itsPlugin.itsConfig.requestLimits(), received_levels.size(), TS::RequestLimitMember::LEVELS);
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

      auto tz = itsPlugin.itsEngines.geoEngine->getTimeZones().time_zone_from_string(query.timezone);
	  auto tlist = generateTList(query, producer, producerDataPeriod);

      if (tlist.empty())
        return;

      check_request_limit(
          itsPlugin.itsConfig.requestLimits(), tlist.size(), TS::RequestLimitMember::TIMESTEPS);

      auto querydata_tlist = generateQEngineQueryTimes(query, paramname);

#ifdef MYDEBUG
      std::cout << std::endl << "producer: " << producer << std::endl;
      std::cout << "data period start time: "
                << producerDataPeriod.getLocalStartTime(producer, query.timezone, itsPlugin.itsEngines.geoEngine->getTimeZones())
                << std::endl;
      std::cout << "data period end time: "
                << producerDataPeriod.getLocalEndTime(producer, query.timezone, itsPlugin.itsEngines.geoEngine->getTimeZones())
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

	  if(isPointQuery)
      {
		pointQuery(query,
				   producer,
				   paramfunc,
				   tloc,
				   querydata_tlist,
				   tlist,
				   cacheKey,
				   state,
				   qi,
				   nearestpoint,
				   precision,
				   loadDataLevels,
				   pressure,
				   height,
				   queryLevelDataCache,
				   aggregatedData);
      }
      else
      {
		areaQuery(query,
				  producer,
				  paramfunc,
				  tloc,
				  querydata_tlist,
				  tlist,
				  cacheKey,
				  state,
				  qi,
				  nearestpoint,
				  precision,
				  loadDataLevels,
				  pressure,
				  height,
				  queryLevelDataCache,
				  aggregatedData);
      }
	}  // levels

    // store level-data
	PostProcessing::store_data(aggregatedData, query, outputData);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

TS::TimeSeriesGenerator::LocalTimeList QEngineQuery::generateQEngineQueryTimes(const Query& query, 
																			   const std::string& paramname) const
{
  try
  {
    auto tz = itsPlugin.itsEngines.geoEngine->getTimeZones().time_zone_from_string(query.timezone);
    auto tlist = itsPlugin.itsTimeSeriesCache->generate(query.toptions, tz);

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

    TS::TimeSeriesGeneratorOptions topt = query.toptions;

    topt.startTime = (query.toptions.startTimeUTC ? timeseriesStartTime.utc_time()
                                                  : timeseriesStartTime.local_time());
    topt.endTime =
        (query.toptions.endTimeUTC ? timeseriesEndTime.utc_time() : timeseriesEndTime.local_time());

    // generate timelist for aggregation
    tlist = itsPlugin.itsTimeSeriesCache->generate(topt, tz);
    qdtimesteps.insert(tlist->begin(), tlist->end());

    // for aggregation generate timesteps also between fixed times
    if (topt.mode == TS::TimeSeriesGeneratorOptions::FixedTimes ||
        topt.mode == TS::TimeSeriesGeneratorOptions::TimeSteps)
    {
      topt.mode = TS::TimeSeriesGeneratorOptions::DataTimes;
      topt.timeSteps = boost::none;
      topt.timeStep = boost::none;
      topt.timeList.clear();

      // generate timelist for aggregation
      tlist = itsPlugin.itsTimeSeriesCache->generate(topt, tz);
      qdtimesteps.insert(tlist->begin(), tlist->end());
    }

    // add timesteps to LocalTimeList
    TS::TimeSeriesGenerator::LocalTimeList ret;
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

void QEngineQuery::pointQuery(const Query& theQuery,
							  const std::string& theProducer,
							  const TS::ParameterAndFunctions& theParamFunc,
							  const Spine::TaggedLocation& theTLoc,
							  const TS::TimeSeriesGenerator::LocalTimeList& theQueryDataTlist,
							  const TS::TimeSeriesGenerator::LocalTimeList& theRequestedTList,
							  const std::pair<float, std::string>& theCacheKey,
							  const State& theState,
							  const Engine::Querydata::Q& theQ,
							  const NFmiPoint& theNearestPoint,
							  int thePrecision,
							  bool theLoadDataLevels,
							  boost::optional<float> thePressure,
							  boost::optional<float> theHeight,
							  QueryLevelDataCache& theQueryLevelDataCache,
							  std::vector<TS::TimeSeriesData>& theAggregatedData) const
{
  try
	{
        TS::TimeSeriesPtr querydata_result;
		const auto& paramname = theParamFunc.parameter.name();
		auto loc = theTLoc.loc;
		NFmiSvgPath svgPath;
		bool isWkt = false;
		loc = resolveLocation(theTLoc, theQuery, svgPath, isWkt);
		const auto country = itsPlugin.itsEngines.geoEngine->countryName(loc->iso2, theQuery.language);

        // if we have fetched the data for this parameter earlier, use it
        if (theQueryLevelDataCache.itsTimeSeries.find(theCacheKey) !=
            theQueryLevelDataCache.itsTimeSeries.end())
        {
          querydata_result = theQueryLevelDataCache.itsTimeSeries[theCacheKey];
        }
        else if (paramname == "fmisid" || paramname == "lpnn" || paramname == "wmo")
        {
          querydata_result = boost::make_shared<TS::TimeSeries>(theState.getLocalTimePool());
          for (const auto& t : theQueryDataTlist)
          {
            if (loc->fmisid && paramname == "fmisid")
            {
              querydata_result->emplace_back(TS::TimedValue(t, *(loc->fmisid)));
            }
            else
            {
              querydata_result->emplace_back(TS::TimedValue(t, TS::None()));
            }
          }
        }
        else if (UtilityFunctions::is_special_parameter(paramname))
        {
          querydata_result = boost::make_shared<TS::TimeSeries>(theState.getLocalTimePool());
          UtilityFunctions::get_special_parameter_values(paramname,
                                                         thePrecision,
                                                         theQueryDataTlist,
                                                         loc,
                                                         theQuery,
                                                         theState,
                                                         itsPlugin.itsEngines.geoEngine->getTimeZones(),
                                                         querydata_result);
        }
        else
        {
          Spine::Parameter param = TS::get_query_param(theParamFunc.parameter);

          Engine::Querydata::ParameterOptions querydata_param(param,
                                                              theProducer,
                                                              *loc,
                                                              country,
                                                              theTLoc.tag,
                                                              *theQuery.timeformatter,
                                                              theQuery.timestring,
                                                              theQuery.language,
                                                              theQuery.outlocale,
                                                              theQuery.timezone,
                                                              theQuery.findnearestvalidpoint,
                                                              theNearestPoint,
                                                              theQuery.lastpoint,
                                                              theState.getLocalTimePool());

          // one location, list of local times (no radius -> pointforecast)
          querydata_result = theLoadDataLevels ? theQ->values(querydata_param, theQueryDataTlist)
                             : thePressure
                                 ? theQ->valuesAtPressure(querydata_param, theQueryDataTlist, *thePressure)
                                 : theQ->valuesAtHeight(querydata_param, theQueryDataTlist, *theHeight);
        }
        if (!querydata_result->empty())
        {
          if (theParamFunc.parameter.name() == "x" || theParamFunc.parameter.name() == "y")
			TS::transform_wgs84_coordinates(theParamFunc.parameter.name(), theQuery.crs, *loc, *querydata_result);

          theQueryLevelDataCache.itsTimeSeries.insert(make_pair(theCacheKey, querydata_result));
        }

        theAggregatedData.emplace_back(TS::TimeSeriesData(TS::erase_redundant_timesteps(
            TS::aggregate(querydata_result, theParamFunc.functions), theRequestedTList)));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Spine::LocationList QEngineQuery::getLocationListForPath(const Query& theQuery,
														 const Spine::TaggedLocation& theTLoc,
														 const std::string& place,
														 const NFmiSvgPath svgPath,
														 const State& theState,
														 bool isWkt) const
{
  try
	{
	  Spine::LocationList llist;

	  if (isWkt)
		{
		  OGRwkbGeometryType geomType =
			theQuery.wktGeometries.getGeometry(place)->getGeometryType();
		  if (geomType == wkbMultiLineString)
			{
			  // OGRMultiLineString -> handle each LineString separately
			  std::list<NFmiSvgPath> svgList = theQuery.wktGeometries.getSvgPaths(place);
			  for (const auto& svg : svgList)
				{
				  Spine::LocationList ll =
					get_location_list(svg, theTLoc.tag, theQuery.step, theState.getGeoEngine());
				  if (!ll.empty())
					llist.insert(llist.end(), ll.begin(), ll.end());
				}
			}
		  else if (geomType == wkbMultiPoint)
			{
			  llist = theQuery.wktGeometries.getLocations(place);
			}
		  else if (geomType == wkbLineString)
			{
			  // For LineString svgPath has been extracted earlier
			  llist = get_location_list(svgPath, theTLoc.tag, theQuery.step, theState.getGeoEngine());
			}
		}
	  else
		{
		  llist = get_location_list(svgPath, theTLoc.tag, theQuery.step, theState.getGeoEngine());
		}

	  return llist;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}


TS::TimeSeriesGroupPtr QEngineQuery::getQEngineValuesForArea(const Query& theQuery,
															 const std::string& theProducer,
															 const TS::ParameterAndFunctions& theParamFunc,
															 const Spine::TaggedLocation& theTLoc,
															 const Spine::LocationPtr& loc,
															 const TS::TimeSeriesGenerator::LocalTimeList& theQueryDataTlist,
															 const State& theState,
															 const Engine::Querydata::Q& theQ,
															 const NFmiPoint& theNearestPoint,
															 int thePrecision,
															 bool theLoadDataLevels,
															 boost::optional<float> thePressure,
															 boost::optional<float> theHeight,
															 const std::string& paramname,
															 const Spine::LocationList& llist) const
{
  try
	{
	  TS::TimeSeriesGroupPtr querydata_result;

	  if (UtilityFunctions::is_special_parameter(paramname))
		{
		  querydata_result = boost::make_shared<TS::TimeSeriesGroup>();
		  UtilityFunctions::get_special_parameter_values(paramname,
														 thePrecision,
														 theQueryDataTlist,
														 llist,
														 theQuery,
														 theState,
														 itsPlugin.itsEngines.geoEngine->getTimeZones(),
														 querydata_result);
		}
	  else
		{
		  Spine::Parameter param = TS::get_query_param(theParamFunc.parameter);
		  const auto country = itsPlugin.itsEngines.geoEngine->countryName(loc->iso2, theQuery.language);

		  Engine::Querydata::ParameterOptions querydata_param(param,
															  theProducer,
															  *loc,
															  country,
															  theTLoc.tag,
															  *theQuery.timeformatter,
															  theQuery.timestring,
															  theQuery.language,
															  theQuery.outlocale,
															  theQuery.timezone,
															  theQuery.findnearestvalidpoint,
															  theNearestPoint,
															  theQuery.lastpoint,
															  theState.getLocalTimePool());
		  
		  // list of locations, list of local times
		  querydata_result =
			theLoadDataLevels
			? theQ->values(
						   querydata_param, llist, theQueryDataTlist, theQuery.maxdistance_kilometers())
			: thePressure ? theQ->valuesAtPressure(querydata_param,
												   llist,
												   theQueryDataTlist,
												   theQuery.maxdistance_kilometers(),
												   *thePressure)
			: theQ->valuesAtHeight(querydata_param,
								   llist,
								   theQueryDataTlist,
								   theQuery.maxdistance_kilometers(),
								   *theHeight);
		}
	  
	  return querydata_result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Spine::LocationList QEngineQuery::getLocationListForArea(const Spine::TaggedLocation& theTLoc,
														 const Spine::LocationPtr& loc,
														 const Engine::Querydata::Q& theQ,
														 NFmiSvgPath& svgPath,
														 bool isWkt) const
{
  try
	{
	  NFmiIndexMask mask;

	  if (loc->type == Spine::Location::BoundingBox)
		{
		  const auto place = get_name_base(loc->name);
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
		  mask = NFmiIndexMaskTools::MaskExpand(theQ->grid(), boundingBoxPath, loc->radius);
		}
	  else if (loc->type == Spine::Location::Area || loc->type == Spine::Location::Place ||
			   loc->type == Spine::Location::CoordinatePoint)
		{
		  if (!isWkt)  // SVG for WKT has been extracted earlier
			get_svg_path(theTLoc, itsPlugin.itsGeometryStorage, svgPath);
		  // If SVG has been extarcted earier the radius is already included
		  mask = NFmiIndexMaskTools::MaskExpand(theQ->grid(), svgPath, isWkt ? 0 : loc->radius);
		}
	  // Indexmask (indexed locations on the area)
	  return get_indexmask_locations(mask, loc, theQ, *itsPlugin.itsEngines.geoEngine);	  
 }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}



void QEngineQuery::areaQuery(const Query& theQuery,
							  const std::string& theProducer,
							  const TS::ParameterAndFunctions& theParamFunc,
							  const Spine::TaggedLocation& theTLoc,
							  const TS::TimeSeriesGenerator::LocalTimeList& theQueryDataTlist,
							  const TS::TimeSeriesGenerator::LocalTimeList& theRequestedTList,
							  const std::pair<float, std::string>& theCacheKey,
							  const State& theState,
							  const Engine::Querydata::Q& theQ,
							  const NFmiPoint& theNearestPoint,
							  int thePrecision,
							  bool theLoadDataLevels,
							  boost::optional<float> thePressure,
							  boost::optional<float> theHeight,
							  QueryLevelDataCache& theQueryLevelDataCache,
							  std::vector<TS::TimeSeriesData>& theAggregatedData) const
{
  try
	{
	  auto loc = theTLoc.loc;


	  auto place = get_name_base(loc->name);
	  const auto& paramname = theParamFunc.parameter.name();
	  bool isWkt = false;
	  NFmiSvgPath svgPath;
	  loc = resolveLocation(theTLoc, theQuery, svgPath, isWkt);
	  const auto country = itsPlugin.itsEngines.geoEngine->countryName(loc->iso2, theQuery.language);

	  TS::TimeSeriesGroupPtr querydata_result;
	  
	  if (theQueryLevelDataCache.itsTimeSeriesGroups.find(theCacheKey) !=
		  theQueryLevelDataCache.itsTimeSeriesGroups.end())
        {
          querydata_result = theQueryLevelDataCache.itsTimeSeriesGroups[theCacheKey];
        }
	  else
        {
          if (loc->type == Spine::Location::Path)
			{
			  Spine::LocationList llist = getLocationListForPath(theQuery,
																 theTLoc,
																 place,
																 svgPath,
																 theState,
																 isWkt);

			  check_request_limit(itsPlugin.itsConfig.requestLimits(),
								  llist.size(),
								  TS::RequestLimitMember::LOCATIONS);

			  querydata_result = getQEngineValuesForArea(theQuery,
														 theProducer,
														 theParamFunc,
														 theTLoc,
														 loc,
														 theQueryDataTlist,
														 theState,
														 theQ,
														 theNearestPoint,
														 thePrecision,
														 theLoadDataLevels,
														 thePressure,
														 theHeight,
														 paramname,
														 llist);			  
			}
          else if (theQ->isGrid() &&
                   (loc->type == Spine::Location::BoundingBox ||
                    loc->type == Spine::Location::Area || loc->type == Spine::Location::Place ||
                    loc->type == Spine::Location::CoordinatePoint))
			{		  
			  Spine::LocationList llist = getLocationListForArea(theTLoc,
																 loc,
																 theQ,
																 svgPath,
																 isWkt);
			  check_request_limit(
								  itsPlugin.itsConfig.requestLimits(), llist.size(), TS::RequestLimitMember::LOCATIONS);

			  querydata_result = getQEngineValuesForArea(theQuery,
														 theProducer,
														 theParamFunc,
														 theTLoc,
														 loc,
														 theQueryDataTlist,
														 theState,
														 theQ,
														 theNearestPoint,
														 thePrecision,
														 theLoadDataLevels,
														 thePressure,
														 theHeight,
														 paramname,
														 llist);			  
			}
		  if (!querydata_result->empty())
			{


			  // if the value is not dependent on location inside area
			  // we just need to have the first one
			  if (!TS::parameter_is_arithmetic(theParamFunc.parameter))
				{
				  auto dataIndependentValue = querydata_result->at(0);
				  querydata_result->clear();
				  querydata_result->push_back(dataIndependentValue);
				}
			  
			  if (theParamFunc.parameter.name() == "x" || theParamFunc.parameter.name() == "y")
				TS::transform_wgs84_coordinates(theParamFunc.parameter.name(), theQuery.crs, *querydata_result);
			  
			  theQueryLevelDataCache.itsTimeSeriesGroups.insert(make_pair(theCacheKey, querydata_result));			  
			}  // area handling
		}

	  if (!querydata_result->empty())
		theAggregatedData.emplace_back(TS::TimeSeriesData(TS::erase_redundant_timesteps(
																						TS::aggregate(querydata_result, theParamFunc.functions), theRequestedTList)));
	  
	}
  catch (...)
	{
	  throw Fmi::Exception::Trace(BCP, "Operation failed!");
	}
}



Engine::Querydata::Producer QEngineQuery::selectProducer(const Spine::Location& location,
														 const Query& query,
														 const AreaProducers& areaproducers) const
{
  try
  {
    // Allow querydata to use data specific max distances if the maxdistance option
    // was not given in the query string


    bool use_data_max_distance = !query.maxdistanceOptionGiven;

    if (areaproducers.empty())
    {
      return itsPlugin.itsEngines.qEngine->find(location.longitude,
												location.latitude,
												query.maxdistance_kilometers(),
												use_data_max_distance,
												query.leveltype);
    }
	
    // Allow listed producers only
    return itsPlugin.itsEngines.qEngine->find(areaproducers,
											  location.longitude,
											  location.latitude,
											  query.maxdistance_kilometers(),
											  use_data_max_distance,
											  query.leveltype);
  }
  catch (...)
	{
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Spine::LocationPtr QEngineQuery::resolveLocation(const Spine::TaggedLocation& tloc,
												 const Query& query,
												 NFmiSvgPath& svgPath,
												 bool& isWkt) const
{
  try
  {
    Spine::LocationPtr loc = tloc.loc;
    std::string place = get_name_base(loc->name);

    if (loc->type == Spine::Location::Wkt)
    {
      loc = query.wktGeometries.getLocation(tloc.loc->name);
      svgPath = query.wktGeometries.getSvgPath(tloc.loc->name);
      isWkt = true;
    }
    else if (loc->type == Spine::Location::Path || loc->type == Spine::Location::Area)
    {
      loc = get_location_for_area(tloc, itsPlugin.itsGeometryStorage, query.language, *itsPlugin.itsEngines.geoEngine, &svgPath);
    }
    else if (loc->type == Spine::Location::BoundingBox)
    {
      // get location info for center coordinate
      std::unique_ptr<Spine::Location> tmp =
          get_bbox_location(place, query.language, *itsPlugin.itsEngines.geoEngine);

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

TS::TimeSeriesGenerator::LocalTimeList QEngineQuery::generateTList(const Query& query,
																   const std::string& producer,
																   const ProducerDataPeriod& producerDataPeriod) const
{
  try
	{
      auto tz = itsPlugin.itsEngines.geoEngine->getTimeZones().time_zone_from_string(query.timezone);
      auto tlist = *itsPlugin.itsTimeSeriesCache->generate(query.toptions, tz);
      bool isClimatologyProducer =
          (producer.empty() ? false : itsPlugin.itsEngines.qEngine->getProducerConfig(producer).isclimatology);

      // remove timesteps that are later than last timestep in query data file
      // except from climatology
      if (!tlist.empty() && !isClimatologyProducer)
      {
        boost::local_time::local_date_time data_period_endtime =
            producerDataPeriod.getLocalEndTime(producer, query.timezone, itsPlugin.itsEngines.geoEngine->getTimeZones());

        while (!tlist.empty() && !data_period_endtime.is_not_a_date_time() &&
               *(--tlist.end()) > data_period_endtime)
        {
          tlist.pop_back();
        }
      }

	  return tlist;
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
