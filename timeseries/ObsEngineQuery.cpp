#include "ObsEngineQuery.h"
#include "LocationTools.h"
#include "PostProcessing.h"
#include "State.h"
#include "UtilityFunctions.h"
#include <macgyver/Exception.h>
#include <macgyver/TimeParser.h>
#include <newbase/NFmiSvgTools.h>
#include <timeseries/ParameterKeywords.h>
#include <timeseries/ParameterTools.h>
#include <timeseries/TimeSeriesInclude.h>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
namespace
{
void print_settings(const Engine::Observation::Settings& settings)
{
  std::cout << settings;
}

TS::TimeSeriesGenerator::LocalTimeList get_timesteps(const TS::TimeSeries ts)
{
  try
  {
    TS::TimeSeriesGenerator::LocalTimeList timesteps;

    for (const TS::TimedValue& tv : ts)
      timesteps.push_back(tv.time);

    return timesteps;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

TS::TimeSeries generate_timeseries(
    const State& state,
    const std::vector<boost::local_time::local_date_time>& timestep_vector,
    const TS::Value& value)
{
  try
  {
    TS::TimeSeries timeseries(state.getLocalTimePool());

    for (const auto& timestep : timestep_vector)
      timeseries.emplace_back(TS::TimedValue(timestep, value));

    return timeseries;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool is_wkt_point(const Spine::LocationPtr& loc)
{
  try
  {
    if (loc->type == Spine::Location::Wkt)
    {
      auto geom = get_ogr_geometry(loc->name, loc->radius);

      if (!geom)
        return false;

      auto type = geom->getGeometryType();

      return (type == wkbPoint);
    }

    return false;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

TS::TimeSeriesGenerator::LocalTimeList get_all_timesteps(const Query& query,
                                                         const TS::TimeSeries& ts,
                                                         const boost::local_time::time_zone_ptr& tz)
{
  try
  {
    TS::TimeSeriesGenerator::LocalTimeList ret;

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

    for (const TS::TimedValue& tv : ts)
    {
      // Do not show timesteps beyond starttime/endtime
      if (tv.time.utc_time() >= startTimeAsUTC && tv.time.utc_time() <= endTimeAsUTC)
        ret.push_back(tv.time);
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

Spine::LocationPtr get_loc(const Query& query,
                           const State& state,
                           const std::string& producer,
                           int fmisid)
{
  try
  {
    Spine::LocationPtr loc;

    if (!UtilityFunctions::is_flash_or_mobile_producer(producer))
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

    return loc;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

std::vector<boost::local_time::local_date_time> get_actual_timesteps(const TS::TimeSeries& ts)
{
  try
  {
    std::vector<boost::local_time::local_date_time> timestep_vector;

    for (const auto& value : ts)
      timestep_vector.push_back(value.time);

    return timestep_vector;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void resolve_parameter_settings(const ObsParameters& obsParameters,
                                const Query& query,
                                const std::string& producer,
                                Engine::Observation::Settings& settings,
                                unsigned int& aggregationIntervalBehind,
                                unsigned int& aggregationIntervalAhead)
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
          (TS::is_location_parameter(pname) &&
           !UtilityFunctions::is_flash_or_mobile_producer(producer)) ||
          TS::is_time_parameter(pname))
        continue;

      // fmisid must be always included (except for flash) in queries in order to get location
      // info from geonames
      if (pname == FMISID_PARAM && !UtilityFunctions::is_flash_or_mobile_producer(producer))
        fmisid_index = settings.parameters.size();

      // all parameters are fetched at once
      settings.parameters.push_back(param);
    }

    // fmisid must be always included in order to get thelocation info from geonames
    if (fmisid_index == -1 && !UtilityFunctions::is_flash_or_mobile_producer(producer))
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

void resolve_time_settings(const std::string& producer,
                           const ProducerDataPeriod& producerDataPeriod,
                           const boost::posix_time::ptime& now,
                           Query& query,
                           unsigned int aggregationIntervalBehind,
                           unsigned int aggregationIntervalAhead,
                           Engine::Observation::Settings& settings,
                           const Fmi::TimeZones& timezones)
{
  try
  {
    // Below are listed optional settings, defaults are set while constructing an
    // ObsEngine::Oracle instance.

    boost::local_time::time_zone_ptr tz = timezones.time_zone_from_string(query.timezone);
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
            producerDataPeriod.getLocalStartTime(producer, query.timezone, timezones).utc_time();
      else
        query.toptions.startTime =
            producerDataPeriod.getLocalStartTime(producer, query.timezone, timezones).local_time();
      if (query.toptions.startTimeUTC)
        query.toptions.endTime =
            producerDataPeriod.getLocalEndTime(producer, query.timezone, timezones).utc_time();
      else
        query.toptions.endTime =
            producerDataPeriod.getLocalEndTime(producer, query.timezone, timezones).local_time();
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
}  // namespace

ObsEngineQuery::ObsEngineQuery(const Plugin& thePlugin) : itsPlugin(thePlugin) {}

#ifndef WITHOUT_OBSERVATION
void ObsEngineQuery::processObsEngineQuery(const State& state,
                                           Query& query,
                                           TS::OutputData& outputData,
                                           const AreaProducers& areaproducers,
                                           const ProducerDataPeriod& producerDataPeriod,
                                           const ObsParameters& obsParameters) const
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
        settings.requestLimits = itsPlugin.itsConfig.requestLimits();

        check_request_limit(itsPlugin.itsConfig.requestLimits(),
                            settings.parameters.size(),
                            TS::RequestLimitMember::PARAMETERS);
        if (!settings.taggedFMISIDs.empty())
          check_request_limit(itsPlugin.itsConfig.requestLimits(),
                              settings.taggedFMISIDs.size(),
                              TS::RequestLimitMember::LOCATIONS);

        if (query.debug)
          settings.debug_options = Engine::Observation::Settings::DUMP_SETTINGS;

#ifdef MYDEBUG
        print_settings(settings);
#endif

        std::vector<TS::TimeSeriesData> tsdatavector;
        outputData.emplace_back(make_pair("_obs_", tsdatavector));

        if (!item.is_area || UtilityFunctions::is_flash_or_mobile_producer(producer))
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

void fill_missing_location_params(TS::TimeSeries& ts)
{
  TS::Value missing_value = TS::None();
  TS::Value actual_value = missing_value;
  bool missing_values_exists = false;
  for (const auto& item : ts)
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
    for (auto& item : ts)
      if (item.value == missing_value)
        item.value = actual_value;
  }
}

TS::TimeSeriesVectorPtr ObsEngineQuery::handleObsParametersForPlaces(
    const State& state,
    const std::string& producer,
    const Spine::LocationPtr& loc,
    const Query& query,
    const ObsParameters& obsParameters,
    const TS::TimeSeriesVectorPtr& observation_result,
    const std::vector<boost::local_time::local_date_time>& timestep_vector,
    std::map<std::string, unsigned int>& parameterResultIndexes) const
{
  try
  {
    TS::TimeSeriesVectorPtr ret(new TS::TimeSeriesVector());
    unsigned int obs_result_field_index = 0;
    TS::Value missing_value = TS::None();

    // Iterate parameters and store values for all parameters
    // into ret data structure
    for (unsigned int i = 0; i < obsParameters.size(); i++)
    {
      const ObsParameter& obsParam = obsParameters.at(i);

      std::string paramname = obsParam.param.name();
      bool is_location_p = TS::is_location_parameter(paramname);

      if (is_location_p && !UtilityFunctions::is_flash_or_mobile_producer(producer))
      {
        // add data for location field
        TS::Value value = TS::location_parameter(loc,
                                                 obsParam.param.name(),
                                                 query.valueformatter,
                                                 query.timezone,
                                                 query.precisions[i],
                                                 query.crs);
        auto timeseries = generate_timeseries(state, timestep_vector, value);
        ret->emplace_back(timeseries);
        parameterResultIndexes.insert(std::make_pair(paramname, ret->size() - 1));
      }
      else if (TS::is_time_parameter(paramname))
      {
        // add data for time fields
        Spine::Location location(0, 0, "", query.timezone);
        TS::TimeSeries timeseries(state.getLocalTimePool());
        for (const auto& timestep : timestep_vector)
        {
          TS::Value value = TS::time_parameter(paramname,
                                               timestep,
                                               state.getTime(),
                                               *loc,
                                               query.timezone,
                                               itsPlugin.itsEngines.geoEngine->getTimeZones(),
                                               query.outlocale,
                                               *query.timeformatter,
                                               query.timestring);
          timeseries.emplace_back(TS::TimedValue(timestep, value));
        }
        ret->emplace_back(timeseries);
        parameterResultIndexes.insert(std::make_pair(paramname, ret->size() - 1));
      }
      else if (!obsParameters[i].duplicate)
      {
        // add data fields fetched from observation
        auto result = *observation_result;
        if (result[obs_result_field_index].empty())
          continue;

        auto result_at_index = result[obs_result_field_index];

        // If time independend special parameter contains missing values in some timesteps,
        // replace them with existing values to keep aggregation working

        if (is_location_p)
          fill_missing_location_params(result_at_index);

        ret->push_back(result_at_index);
        std::string pname_plus_snumber = TS::get_parameter_id(obsParameters[i].param);
        parameterResultIndexes.insert(std::make_pair(pname_plus_snumber, ret->size() - 1));
        obs_result_field_index++;
      }
    }
    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

TS::TimeSeriesVectorPtr ObsEngineQuery::doAggregationForPlaces(
    const State& state,
    const ObsParameters& obsParameters,
    const TS::TimeSeriesVectorPtr& observation_result,
    std::map<std::string, unsigned int>& parameterResultIndexes) const
{
  try
  {
    TS::TimeSeriesVectorPtr aggregated_observation_result(new TS::TimeSeriesVector());
    // iterate parameters and do aggregation
    for (const auto& obsParam : obsParameters)
    {
      std::string paramname = obsParam.param.name();
      std::string pname_plus_snumber = TS::get_parameter_id(obsParam.param);
      if (parameterResultIndexes.find(pname_plus_snumber) != parameterResultIndexes.end())
        paramname = pname_plus_snumber;
      else if (parameterResultIndexes.find(paramname) == parameterResultIndexes.end())
        continue;

      unsigned int resultIndex = parameterResultIndexes.at(paramname);
      TS::TimeSeries ts = (*observation_result)[resultIndex];
      TS::DataFunctions pfunc = obsParam.functions;
      TS::TimeSeriesPtr tsptr;
      // If inner function exists aggregation happens
      if (pfunc.innerFunction.exists())
      {
        tsptr = TS::Aggregator::aggregate(ts, pfunc);
        if (tsptr->empty())
          continue;
      }
      else
      {
        tsptr = boost::make_shared<TS::TimeSeries>(state.getLocalTimePool());
        *tsptr = ts;
      }
      aggregated_observation_result->push_back(*tsptr);
    }
    return aggregated_observation_result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void ObsEngineQuery::fetchObsEngineValuesForPlaces(const State& state,
                                                   const std::string& producer,
                                                   const ObsParameters& obsParameterss,
                                                   Engine::Observation::Settings& settings,
                                                   Query& query,
                                                   TS::OutputData& outputData) const
{
  try
  {
    TS::TimeSeriesVectorPtr observation_result;
    ObsParameters obsParameters = obsParameterss;

    // Quick query if there is no aggregation
    if (!query.timeAggregationRequested)
    {
      observation_result = itsPlugin.itsEngines.obsEngine->values(settings, query.toptions);
    }
    else
    {
      // Request all observations in order to do aggregation
      TS::TimeSeriesGeneratorOptions tmpoptions;
      tmpoptions.startTime = settings.starttime;
      tmpoptions.endTime = settings.endtime;
      tmpoptions.startTimeUTC = query.toptions.startTimeUTC;
      tmpoptions.endTimeUTC = query.toptions.endTimeUTC;
      settings.timestep = 1;

      // fetches results for all location and all parameters
      observation_result = itsPlugin.itsEngines.obsEngine->values(settings, tmpoptions);
    }
#ifdef MYDEBYG
    std::cout << "observation_result for places: " << *observation_result << std::endl;
#endif

    if (observation_result->empty())
      return;

    TS::Value missing_value = TS::None();
    int fmisid_index = get_fmisid_index(settings);

    TS::TimeSeriesGeneratorCache::TimeList tlist;
    auto tz = itsPlugin.itsEngines.geoEngine->getTimeZones().time_zone_from_string(query.timezone);
    // If query.toptions.startTime == query.toptions.endTime and timestep is missing
    // use minutes as timestep
    if (!query.toptions.timeStep && query.toptions.startTime == query.toptions.endTime)
    {
      query.toptions.timeStep = 0;
    }

    if (!query.toptions.all())
      tlist = itsPlugin.itsTimeSeriesCache->generate(query.toptions, tz);

    TS::TimeSeriesByLocation observation_result_by_location =
        PostProcessing::get_timeseries_by_fmisid(producer, observation_result, tlist, fmisid_index);

    // If producer is syke or flash accept all timesteps
    bool acceptAllTimesteps =
        (query.toptions.all() || UtilityFunctions::is_flash_producer(producer) ||
         UtilityFunctions::is_mobile_producer(producer) || producer == SYKE_PRODUCER);

    // iterate locations
    for (const auto& observation_result_location : observation_result_by_location)
    {
      observation_result = observation_result_location.second;
      int fmisid = observation_result_location.first;

      // Get location
      Spine::LocationPtr loc = get_loc(query, state, producer, fmisid);
      // Actual timesteps
      std::vector<boost::local_time::local_date_time> timestep_vector =
          get_actual_timesteps(observation_result->at(0));

      std::map<std::string, unsigned int> parameterResultIndexes;

      observation_result = handleObsParametersForPlaces(state,
                                                        producer,
                                                        loc,
                                                        query,
                                                        obsParameters,
                                                        observation_result,
                                                        timestep_vector,
                                                        parameterResultIndexes);

      auto aggregated_observation_result =
          doAggregationForPlaces(state, obsParameters, observation_result, parameterResultIndexes);

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

      if (acceptAllTimesteps)
      {
        auto aggtimes = get_all_timesteps(query, aggregated_observation_result->at(0), tz);

        // store observation data
        PostProcessing::store_data(
            TS::erase_redundant_timesteps(aggregated_observation_result, aggtimes),
            query,
            outputData);
      }
      else
      {
        // Else accept only the originally generated timesteps
        PostProcessing::store_data(
            TS::erase_redundant_timesteps(aggregated_observation_result, *tlist),
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

void ObsEngineQuery::handleSpecialParameter(const ObsParameter& obsparam,
                                            const std::string& areaName,
                                            TS::TimeSeriesGroupPtr& tsg) const
{
  try
  {
    // value of these special fields is different in different locations,
    if (obsparam.param.name() != STATIONNAME_PARAM && obsparam.param.name() != STATION_NAME_PARAM &&
        obsparam.param.name() != LON_PARAM && obsparam.param.name() != LAT_PARAM &&
        obsparam.param.name() != LATLON_PARAM && obsparam.param.name() != LONLAT_PARAM &&
        obsparam.param.name() != PLACE_PARAM && obsparam.param.name() != STATIONLON_PARAM &&
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
        TS::LonLatTimeSeries& llts = tsg->at(k);
        TS::TimeSeries& ts = llts.timeseries;
        TS::LonLatTimeSeries& lltsAt0 = tsg->at(0);
        TS::TimeSeries& tsAt0 = lltsAt0.timeseries;

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
      {
        TS::LonLatTimeSeries& llts = tsg->at(0);
        TS::TimeSeries& ts = llts.timeseries;
        for (TS::TimedValue& tv : ts)
          tv.value = areaName;
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

TS::TimeSeriesVectorPtr ObsEngineQuery::handleObsParametersForArea(
    const State& state,
    const std::string& producer,
    const Spine::LocationPtr& loc,
    const ObsParameters& obsParameters,
    const TS::TimeSeriesVector* tsv_observation_result,
    const std::vector<boost::local_time::local_date_time>& ts_vector,
    const Query& query) const
{
  try
  {
    TS::TimeSeriesVectorPtr observation_result_with_added_fields(new TS::TimeSeriesVector());

    unsigned int obs_result_field_index = 0;
    for (unsigned int i = 0; i < obsParameters.size(); i++)
    {
      std::string paramname = obsParameters[i].param.name();

      // add data for location fields
      if (TS::is_location_parameter(paramname) &&
          !UtilityFunctions::is_flash_or_mobile_producer(producer))
      {
        TS::TimeSeries location_ts(state.getLocalTimePool());

        for (const auto& ts : ts_vector)
        {
          TS::Value value = TS::location_parameter(loc,
                                                   obsParameters[i].param.name(),
                                                   query.valueformatter,
                                                   query.timezone,
                                                   query.precisions[i],
                                                   query.crs);

          location_ts.emplace_back(TS::TimedValue(ts, value));
        }
        observation_result_with_added_fields->push_back(location_ts);
      }
      else if (TS::is_time_parameter(paramname))
      {
        // add data for time fields
        Spine::Location dummyloc(0, 0, "", query.timezone);

        TS::TimeSeriesGroupPtr grp(new TS::TimeSeriesGroup);
        TS::TimeSeries time_ts(state.getLocalTimePool());

        for (const auto& ts : ts_vector)
        {
          TS::Value value = TS::time_parameter(paramname,
                                               ts,
                                               state.getTime(),
                                               (loc ? *loc : dummyloc),
                                               query.timezone,
                                               itsPlugin.itsEngines.geoEngine->getTimeZones(),
                                               query.outlocale,
                                               *query.timeformatter,
                                               query.timestring);

          time_ts.emplace_back(TS::TimedValue(ts, value));
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

    return observation_result_with_added_fields;
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

void ObsEngineQuery::fetchObsEngineValuesForArea(const State& state,
                                                 const std::string& producer,
                                                 const ObsParameters& obsParameters,
                                                 const std::string& areaName,
                                                 Engine::Observation::Settings& settings,
                                                 Query& query,
                                                 TS::OutputData& outputData) const
{
  try
  {
    // fetches results for all locations in the area and all parameters
    TS::TimeSeriesVectorPtr observation_result =
        itsPlugin.itsEngines.obsEngine->values(settings, query.toptions);

#ifdef MYDEBYG
    std::cout << "observation_result for area: " << *observation_result << std::endl;
#endif
    if (observation_result->empty())
      return;

    // lets find out actual timesteps: different locations may have different timesteps
    std::vector<boost::local_time::local_date_time> ts_vector;
    std::set<boost::local_time::local_date_time> ts_set;
    const TS::TimeSeries& ts = observation_result->at(0);
    for (const TS::TimedValue& tval : ts)
      ts_set.insert(tval.time);
    ts_vector.insert(ts_vector.end(), ts_set.begin(), ts_set.end());
    std::sort(ts_vector.begin(), ts_vector.end());

    int fmisid_index = get_fmisid_index(settings);

    // All timesteps from result set
    TS::TimeSeriesGeneratorCache::TimeList tlist_all =
        std::make_shared<TS::TimeSeriesGenerator::LocalTimeList>(
            TS::TimeSeriesGenerator::LocalTimeList());
    for (const auto& t : ts_vector)
      tlist_all->push_back(t);

    // Separate timeseries of different locations to their own data structures and add missing
    // timesteps
    TS::TimeSeriesByLocation tsv_area = PostProcessing::get_timeseries_by_fmisid(
        producer, observation_result, tlist_all, fmisid_index);

    std::vector<TS::FmisidTSVectorPair> tsv_area_with_added_fields;
    // add data for location- and time-related fields; these fields are added by timeseries
    // plugin
    for (TS::FmisidTSVectorPair& val : tsv_area)
    {
      TS::TimeSeriesVector* tsv_observation_result = val.second.get();

      const TS::TimeSeries& fmisid_ts = tsv_observation_result->at(fmisid_index);

      // fmisid may be missing for rows for which there is no data. Hence we extract
      // it from the full time timeseries once.
      int fmisid = get_fmisid_value(fmisid_ts);
      Spine::LocationPtr loc =
          get_location(state.getGeoEngine(), fmisid, FMISID_PARAM, query.language);

      auto observation_result_with_added_fields = handleObsParametersForArea(
          state, producer, loc, obsParameters, tsv_observation_result, ts_vector, query);

      tsv_area_with_added_fields.emplace_back(
          TS::FmisidTSVectorPair(val.first, observation_result_with_added_fields));
    }

#ifdef MYDEBUG
    std::cout << "observation_result after locally added fields: " << std::endl;

    for (TS::FmisidTSVectorPair& val : tsv_area_with_added_fields)
    {
      TS::TimeSeriesVector* tsv = val.second.get();
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

    std::vector<TS::TimeSeriesGroupPtr> tsg_vector;
    for (unsigned int i = 0; i < obsParameters.size(); i++)
      tsg_vector.emplace_back(TS::TimeSeriesGroupPtr(new TS::TimeSeriesGroup));

    // iterate locations

    for (const auto& tsv_area : tsv_area_with_added_fields)
    {
      const TS::TimeSeriesVector* tsv = tsv_area.second.get();

      // iterate fields
      for (unsigned int k = 0; k < tsv->size(); k++)
      {
        // add k:th time series to the group
        TS::LonLat lonlat(0, 0);  // location is not relevant here
        TS::LonLatTimeSeries lonlat_ts(lonlat, tsv->at(k));
        tsg_vector[k]->push_back(lonlat_ts);
      }
    }

#ifdef MYDEBUG
    std::cout << "timeseries groups: " << std::endl;
    for (unsigned int i = 0; i < tsg_vector.size(); i++)
    {
      TS::TimeSeriesGroupPtr tsg = tsg_vector.at(i);
      std::cout << "group#" << i << ": " << std::endl << *tsg << std::endl;
    }
#endif

    TS::Value missing_value = TS::None();
    // iterate parameters, aggregate, and store aggregated result

    for (const auto& obsparam : obsParameters)
    {
      unsigned int data_column = obsparam.data_column;
      TS::TimeSeriesGroupPtr tsg = tsg_vector.at(data_column);

      if (tsg->empty())
        continue;

      if (TS::special(obsparam.param))
        handleSpecialParameter(obsparam, areaName, tsg);

      TS::DataFunctions pfunc = obsparam.functions;
      // Do the aggregation if requasted
      TS::TimeSeriesGroupPtr aggregated_tsg(new TS::TimeSeriesGroup);
      if (pfunc.innerFunction.exists())
      {
        *aggregated_tsg = *(TS::aggregate(tsg, pfunc));
      }
      else
      {
        *aggregated_tsg = *tsg;
      }

#ifdef MYDEBUG
      std::cout << boost::posix_time::second_clock::universal_time() << " - aggregated group: "
                << ": " << std::endl
                << *aggregated_tsg << std::endl;
#endif

      std::vector<TS::TimeSeriesData> aggregatedData;

      TS::TimeSeriesGenerator::LocalTimeList tlist;

      // If all timesteps are requested or producer is syke or flash accept all timesteps
      if (query.toptions.all() || UtilityFunctions::is_flash_producer(producer) ||
          UtilityFunctions::is_mobile_producer(producer) || producer == SYKE_PRODUCER)
      {
        tlist = get_timesteps(aggregated_tsg->at(0).timeseries);
      }
      else
      {
        // Else accept only the original generated timesteps
        // Generate requested timesteps
        auto tz =
            itsPlugin.itsEngines.geoEngine->getTimeZones().time_zone_from_string(query.timezone);
        tlist = *(itsPlugin.itsTimeSeriesCache->generate(query.toptions, tz));
      }
      // store observation data
      aggregatedData.emplace_back(
          TS::TimeSeriesData(TS::erase_redundant_timesteps(aggregated_tsg, tlist)));
      PostProcessing::store_data(aggregatedData, query, outputData);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool ObsEngineQuery::isObsProducer(const std::string& producer) const
{
  return (itsPlugin.itsObsEngineStationTypes.find(producer) !=
          itsPlugin.itsObsEngineStationTypes.end());
}

void ObsEngineQuery::handleLocationSettings(
    const Query& query,
    const std::string& producer,
    const Spine::TaggedLocation& tloc,
    Engine::Observation::Settings& settings,
    std::vector<SettingsInfo>& settingsVector,
    Engine::Observation::StationSettings& stationSettings) const
{
  try
  {
    const auto& loc = tloc.loc;

    if (!loc)
      return;

    bool point_location =
        (((loc->type == Spine::Location::Place || loc->type == Spine::Location::CoordinatePoint) &&
          loc->radius == 0) ||
         is_wkt_point(loc));

    if (point_location && !UtilityFunctions::is_flash_or_mobile_producer(producer))
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

    if (!point_location)
    {
      Engine::Observation::Settings areaSettings = settings;

      std::string name;
      if (resolveAreaStations(loc, producer, query, areaSettings, name))
        settingsVector.emplace_back(areaSettings, query.groupareas, name);
      else if (!areaSettings.wktArea.empty())
        settings.wktArea = areaSettings.wktArea;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void ObsEngineQuery::getObsSettings(std::vector<SettingsInfo>& settingsVector,
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
    resolve_parameter_settings(obsParameters,
                               query,
                               producer,
                               settings,
                               aggregationIntervalBehind,
                               aggregationIntervalAhead);

    // Time related settings
    resolve_time_settings(producer,
                          producerDataPeriod,
                          now,
                          query,
                          aggregationIntervalBehind,
                          aggregationIntervalAhead,
                          settings,
                          itsPlugin.itsEngines.geoEngine->getTimeZones());

    // Handle endtime=now
    if (query.latestObservation)
      settings.wantedtime = settings.endtime;

    Engine::Observation::StationSettings stationSettings;

    for (const auto& tloc : query.loptions->locations())
    {
      handleLocationSettings(query, producer, tloc, settings, settingsVector, stationSettings);
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
    if (!query.boundingBox.empty() && UtilityFunctions::is_flash_producer(producer))
    {
      stationSettings.bounding_box_settings["minx"] = query.boundingBox.at("minx");
      stationSettings.bounding_box_settings["miny"] = query.boundingBox.at("miny");
      stationSettings.bounding_box_settings["maxx"] = query.boundingBox.at("maxx");
      stationSettings.bounding_box_settings["maxy"] = query.boundingBox.at("maxy");
    }

    if (!UtilityFunctions::is_flash_or_mobile_producer(producer) ||
        UtilityFunctions::is_icebuoy_or_copernicus_producer(producer))
      settings.taggedFMISIDs =
          itsPlugin.itsEngines.obsEngine->translateToFMISID(settings, stationSettings);

    if (!settings.taggedFMISIDs.empty() || !settings.boundingBox.empty() ||
        !settings.taggedLocations.empty())
      settingsVector.emplace_back(settings, false, "");

    if (settingsVector.empty() && UtilityFunctions::is_flash_or_mobile_producer(producer))
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

void ObsEngineQuery::getCommonObsSettings(Engine::Observation::Settings& settings,
                                          const std::string& producer,
                                          Query& query) const
{
  try
  {
    // At least one of location specifiers must be set
    if (UtilityFunctions::is_flash_producer(producer))
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
    settings.stationgroups = query.stationgroups;
    if (producer == Engine::Observation::FMI_IOT_PRODUCER)
      settings.stationtype_specifier = query.iot_producer_specifier;
    settings.maxdistance = query.maxdistance_meters();
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
    settings.useDataCache = query.useDataCache;
    // Data filtering settings
    settings.dataFilter = query.dataFilter;
    // Option to prevent queries to database
    settings.preventDatabaseQuery = itsPlugin.itsConfig.obsEngineDatabaseQueryPrevented();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool ObsEngineQuery::resolveAreaStations(const Spine::LocationPtr& location,
                                         const std::string& producer,
                                         const Query& query,
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

      std::string wktString;
      if (loc->type == Spine::Location::Path)
      {
        resolveStationsForPath(producer,
                               loc,
                               loc_name_original,
                               loc_name,
                               query,
                               settings,
                               isWkt,
                               wktString,
                               stationSettings);
      }
      else if (loc->type == Spine::Location::Area)
      {
        resolveStationsForArea(producer,
                               loc,
                               loc_name_original,
                               loc_name,
                               query,
                               settings,
                               isWkt,
                               wktString,
                               stationSettings);
      }
      else if (loc->type == Spine::Location::BoundingBox &&
               !UtilityFunctions::is_flash_producer(producer))
      {
        resolveStationsForBBox(producer,
                               loc,
                               loc_name_original,
                               loc_name,
                               query,
                               settings,
                               isWkt,
                               wktString,
                               stationSettings);
      }
      else if (!UtilityFunctions::is_flash_producer(producer) &&
               loc->type == Spine::Location::Place && loc->radius > 0)
      {
        resolveStationsForPlaceWithRadius(
            producer, loc, loc_name, settings, wktString, stationSettings);
      }
      else if (!UtilityFunctions::is_flash_producer(producer) &&
               loc->type == Spine::Location::CoordinatePoint && loc->radius > 0)
      {
        resolveStationsForCoordinatePointWithRadius(producer,
                                                    loc,
                                                    loc_name_original,
                                                    loc_name,
                                                    query,
                                                    settings,
                                                    isWkt,
                                                    wktString,
                                                    stationSettings);
      }

#ifdef MYDEBUG
      std::cout << "WKT of buffered area: " << std::endl << wktString << std::endl;
      std::cout << "#" << stationSettings.fmisids.size() << " stations found" << std::endl;
      for (auto fmisid : stationSettings.fmisids)
        std::cout << "fmisid: " << fmisid << std::endl;
#endif

      if (UtilityFunctions::is_flash_or_mobile_producer(producer) && !wktString.empty())
      {
        settings.wktArea = wktString;
      }
      if (!stationSettings.fmisids.empty() &&
          (!UtilityFunctions::is_flash_or_mobile_producer(producer) ||
           UtilityFunctions::is_icebuoy_or_copernicus_producer(producer)))
      {
        settings.taggedFMISIDs =
            itsPlugin.itsEngines.obsEngine->translateToFMISID(settings, stationSettings);

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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void ObsEngineQuery::resolveStationsForPath(
    const std::string& producer,
    const Spine::LocationPtr& loc,
    const std::string& loc_name_original,
    const std::string& loc_name,
    const Query& query,
    const Engine::Observation::Settings& settings,
    bool isWkt,
    std::string& wktString,
    Engine::Observation::StationSettings& stationSettings) const
{
  try
  {
    const OGRGeometry* pGeo = nullptr;
    // if no radius has been given use 200 meters
    double radius = (loc->radius == 0 ? 200 : loc->radius * 1000);

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
              BCP, "Path " + loc_name + "is invalid, because it has odd number of coordinates!");

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

        std::unique_ptr<OGRGeometry> geom = get_ogr_geometry(wkt, radius);
        wktString = Fmi::OGR::exportToWkt(*geom);
      }
      else
      {
        // path is fetched from database
        pGeo = itsPlugin.itsGeometryStorage.getOGRGeometry(loc_name, wkbMultiLineString);

        if (!pGeo)
          throw Fmi::Exception(BCP, "Path " + loc_name + " not found in PostGIS database!");

        std::unique_ptr<OGRGeometry> poly;
        poly.reset(Fmi::OGR::expandGeometry(pGeo, radius));
        wktString = Fmi::OGR::exportToWkt(*poly);
      }
    }
    if (!UtilityFunctions::is_flash_or_mobile_producer(producer) ||
        UtilityFunctions::is_icebuoy_or_copernicus_producer(producer))
      stationSettings.fmisids =
          get_fmisids_for_wkt(itsPlugin.itsEngines.obsEngine, settings, wktString);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void ObsEngineQuery::resolveStationsForArea(
    const std::string& producer,
    const Spine::LocationPtr& loc,
    const std::string& loc_name_original,
    const std::string& loc_name,
    const Query& query,
    const Engine::Observation::Settings& settings,
    bool isWkt,
    std::string& wktString,
    Engine::Observation::StationSettings& stationSettings) const
{
  try
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
      pGeo = itsPlugin.itsGeometryStorage.getOGRGeometry(loc_name, wkbPolygon);
      if (pGeo == nullptr)
        pGeo = itsPlugin.itsGeometryStorage.getOGRGeometry(loc_name, wkbMultiPolygon);

      if (!pGeo)
        throw Fmi::Exception(BCP, "Area " + loc_name + " not found in PostGIS database!");

      std::unique_ptr<OGRGeometry> poly;
      poly.reset(Fmi::OGR::expandGeometry(pGeo, loc->radius * 1000));

      wktString = Fmi::OGR::exportToWkt(*poly);
    }

    if (!UtilityFunctions::is_flash_or_mobile_producer(producer) ||
        UtilityFunctions::is_icebuoy_or_copernicus_producer(producer))
      stationSettings.fmisids =
          get_fmisids_for_wkt(itsPlugin.itsEngines.obsEngine, settings, wktString);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void ObsEngineQuery::resolveStationsForBBox(
    const std::string& producer,
    const Spine::LocationPtr& loc,
    const std::string& loc_name_original,
    const std::string& loc_name,
    const Query& query,
    const Engine::Observation::Settings& settings,
    bool isWkt,
    std::string& wktString,
    Engine::Observation::StationSettings& stationSettings) const
{
  try
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
    if (!UtilityFunctions::is_flash_or_mobile_producer(producer) ||
        UtilityFunctions::is_icebuoy_or_copernicus_producer(producer))
      stationSettings.fmisids =
          get_fmisids_for_wkt(itsPlugin.itsEngines.obsEngine, settings, wktString);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void ObsEngineQuery::resolveStationsForPlaceWithRadius(
    const std::string& producer,
    const Spine::LocationPtr& loc,
    const std::string& loc_name,
    const Engine::Observation::Settings& settings,
    std::string& wktString,
    Engine::Observation::StationSettings& stationSettings) const
{
  try
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
    if (!UtilityFunctions::is_flash_or_mobile_producer(producer) ||
        UtilityFunctions::is_icebuoy_or_copernicus_producer(producer))
      stationSettings.fmisids =
          get_fmisids_for_wkt(itsPlugin.itsEngines.obsEngine, settings, wktString);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void ObsEngineQuery::resolveStationsForCoordinatePointWithRadius(
    const std::string& producer,
    const Spine::LocationPtr& loc,
    const std::string& loc_name_original,
    const std::string& loc_name,
    const Query& query,
    const Engine::Observation::Settings& settings,
    bool isWkt,
    std::string& wktString,
    Engine::Observation::StationSettings& stationSettings) const

{
  try
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

    if (!UtilityFunctions::is_flash_or_mobile_producer(producer) ||
        UtilityFunctions::is_icebuoy_or_copernicus_producer(producer))
      stationSettings.fmisids =
          get_fmisids_for_wkt(itsPlugin.itsEngines.obsEngine, settings, wktString);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<ObsParameter> ObsEngineQuery::getObsParameters(const Query& query) const
{
  try
  {
    std::vector<ObsParameter> ret;
    if (itsPlugin.itsEngines.obsEngine == nullptr)
      return ret;

    // if observation query exists, sort out obengine parameters
    std::set<std::string> stationTypes = itsPlugin.itsEngines.obsEngine->getValidStationTypes();

    bool done = false;
    for (const auto& areaproducers : query.timeproducers)
    {
      for (const auto& producer : areaproducers)
      {
        if (stationTypes.find(producer) != stationTypes.end())
        {
          std::map<std::string, unsigned int> parameter_columns;
          unsigned int column_index = 0;
          for (const TS::ParameterAndFunctions& paramfuncs : query.poptions.parameterFunctions())
          {
            Spine::Parameter parameter(paramfuncs.parameter);

            std::string parameter_id = TS::get_parameter_id(parameter);

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

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
