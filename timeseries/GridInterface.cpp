// ======================================================================
/*!
 * \brief SmartMet TimeSeries plugin implementation
 */
// ======================================================================
#include "GridInterface.h"
#include "LocationTools.h"
#include "PostProcessing.h"
#include "State.h"
#include "UtilityFunctions.h"
#include <engines/grid/Engine.h>
#include <fmt/format.h>
#include <gis/CoordinateTransformation.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/ImageFunctions.h>
#include <grid-files/common/ImagePaint.h>
#include <grid-files/common/ShowFunction.h>
#include <grid-files/identification/GridDef.h>
#include <macgyver/Astronomy.h>
#include <macgyver/CharsetTools.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeFormatter.h>
#include <macgyver/TimeParser.h>
#include <macgyver/TimeZoneFactory.h>
#include <timeseries/ParameterKeywords.h>
#include <timeseries/ParameterTools.h>
#include <timeseries/TimeSeriesUtility.h>

#define FUNCTION_TRACE FUNCTION_TRACE_OFF

using namespace std;

namespace
{
  Fmi::DateTime y1900(Fmi::Date(1900, 1, 1));
  Fmi::DateTime y1970(Fmi::Date(1970, 1, 1));
  Fmi::DateTime y2100(Fmi::Date(2100, 1, 1));

  Fmi::TimeZonePtr utc("Etc/UTC");
} // anonymous namespace

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{

const uint TimeseriesFunctionFlag = 1 << 31;


namespace
{
void erase_redundant_timesteps(TS::TimeSeries& ts, std::set<Fmi::LocalDateTime>& aggregationTimes)
{
  FUNCTION_TRACE
  try
  {
    TS::TimeSeries no_redundant;
    no_redundant.reserve(ts.size());
    std::set<Fmi::LocalDateTime> newTimes;

    for (const auto& tv : ts)
    {
      if (aggregationTimes.find(tv.time) == aggregationTimes.end() &&
          newTimes.find(tv.time) == newTimes.end())
      {
        no_redundant.emplace_back(tv);
        newTimes.insert(tv.time);
      }
      else
      {
        // std::cout << "REMOVE " << lTime << "\n";
      }
    }

    ts = no_redundant;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

TS::TimeSeriesPtr erase_redundant_timesteps(TS::TimeSeriesPtr ts,
                                            std::set<Fmi::LocalDateTime>& aggregationTimes)
{
  FUNCTION_TRACE
  try
  {
    erase_redundant_timesteps(*ts, aggregationTimes);
    return ts;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

TS::TimeSeriesGroupPtr erase_redundant_timesteps(TS::TimeSeriesGroupPtr tsg,
                                                 std::set<Fmi::LocalDateTime>& aggregationTimes)
{
  FUNCTION_TRACE
  try
  {
    for (auto& t : *tsg)
      erase_redundant_timesteps(t.timeseries, aggregationTimes);

    return tsg;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}
}  // namespace

GridInterface::GridInterface(Engine::Grid::Engine* engine, const Fmi::TimeZones& timezones)
    : itsGridEngine(engine), itsTimezones(timezones)
{
  FUNCTION_TRACE
  try
  {
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

bool GridInterface::isGridProducer(const std::string& producer)
{
  FUNCTION_TRACE
  try
  {
    return itsGridEngine->isGridProducer(producer);
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

bool GridInterface::containsGridProducer(const Query& masterquery)
{
  FUNCTION_TRACE
  try
  {
    for (const auto& areaproducers : masterquery.timeproducers)
      for (const auto& producer : areaproducers)
        if (isGridProducer(producer))
          return true;
    return false;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

bool GridInterface::containsParameterWithGridProducer(const Query& masterquery)
{
  FUNCTION_TRACE
  try
  {
    const char replaceChar[] = {'(', ')', '{', '}', '[', ']', ';', ',', ' ', '/', '\\', '\0'};

    for (const auto& paramfunc : masterquery.poptions.parameterFunctions())
    {
      Spine::Parameter param = paramfunc.parameter;
      // printf("PARAM %s\n",param.name().c_str());

      const uint len = param.name().length();
      char buf[len + 1];
      strcpy(buf, param.name().c_str());
      for (uint t = 0; t < len; t++)
      {
        for (uint c = 0; c < 12; c++)
        {
          if (buf[t] == replaceChar[c])
          {
            buf[t] = ':';
            c = 12;
          }
        }
      }

      std::vector<std::string> partList;

      splitString(buf, ':', partList);
      for (const auto& producer : partList)
      {
        // printf("  -- PRODUCER [%s]\n",producer.c_str());
        if (!producer.empty() && isGridProducer(producer))
          return true;
      }
    }
    return false;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void GridInterface::getDataTimes(const AreaProducers& areaproducers,
                                 std::string& startTime,
                                 std::string& endTime)
{
  FUNCTION_TRACE
  try
  {
    startTime = "30000101T000000";
    endTime = "15000101T000000";

    std::shared_ptr<ContentServer::ServiceInterface> contentServer =
        itsGridEngine->getContentServer_sptr();
    for (const auto& producer : areaproducers)
    {
      std::vector<std::string> pnameList;
      itsGridEngine->getProducerNameList(producer, pnameList);

      for (const auto& pname : pnameList)
      {
        T::ProducerInfo producerInfo;
        if (itsGridEngine->getProducerInfoByName(pname, producerInfo))
        {
          std::set<std::string> contentTimeList;
          contentServer->getContentTimeListByProducerId(
              0, producerInfo.mProducerId, contentTimeList);
          if (!contentTimeList.empty())
          {
            std::string s = *contentTimeList.begin();
            std::string e = *contentTimeList.rbegin();

            if (s < startTime)
              startTime = s;

            if (e > endTime)
              endTime = e;

            return;
          }
        }
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void GridInterface::insertFileQueries(QueryServer::Query& query,
                                      const QueryServer::QueryStreamer_sptr& queryStreamer)
{
  FUNCTION_TRACE
  try
  {
    // Adding queries into the QueryStreamer. These queries fetch actual GRIB1/GRIB2 files
    // accoding to the file attribute definitions (query.mAttributeList). The queryStreamer sends
    // these queries to the queryServer and returns the results (= GRIB files) to the HTTP client.

    for (auto param = query.mQueryParameterList.begin(); param != query.mQueryParameterList.end();
         ++param)
    {
      for (auto val = param->mValueList.begin(); val != param->mValueList.end(); ++val)
      {
        QueryServer::Query newQuery;

        newQuery.mSearchType = QueryServer::Query::SearchType::TimeSteps;
        // newQuery.mProducerNameList;
        newQuery.mForecastTimeList.insert((*val)->mForecastTimeUTC);
        newQuery.mAttributeList = query.mAttributeList;
        newQuery.mCoordinateType = query.mCoordinateType;
        newQuery.mAreaCoordinates = query.mAreaCoordinates;
        newQuery.mRadius = query.mRadius;
        newQuery.mTimezone = query.mTimezone;
        newQuery.mStartTime = (*val)->mForecastTimeUTC;
        newQuery.mEndTime = (*val)->mForecastTimeUTC;
        newQuery.mTimesteps = 1;
        newQuery.mTimestepSizeInMinutes = 60;
        newQuery.mAnalysisTime = (*val)->mAnalysisTime;
        if ((*val)->mGeometryId > 0)
          newQuery.mGeometryIdList.insert((*val)->mGeometryId);

        newQuery.mLanguage = query.mLanguage;
        newQuery.mGenerationFlags = 0;
        newQuery.mFlags = 0;
        newQuery.mMaxParameterValues = 10;

        QueryServer::QueryParameter newParam;

        newParam.mId = param->mId;
        newParam.mType = QueryServer::QueryParameter::Type::GridFile;
        newParam.mLocationType = QueryServer::QueryParameter::LocationType::Geometry;
        newParam.mParam = param->mParam;
        newParam.mOrigParam = param->mOrigParam;
        newParam.mSymbolicName = param->mSymbolicName;
        newParam.mParameterKeyType = (*val)->mParameterKeyType;
        newParam.mParameterKey = (*val)->mParameterKey;
        newParam.mProducerName = param->mProducerName;
        newParam.mGeometryId = (*val)->mGeometryId;
        // newParam.mParameterLevelIdType = (*val)->mParameterLevelIdType;
        newParam.mParameterLevelId = (*val)->mParameterLevelId;
        newParam.mParameterLevel = (*val)->mParameterLevel;
        newParam.mForecastType = (*val)->mForecastType;
        newParam.mForecastNumber = (*val)->mForecastNumber;
        newParam.mAreaInterpolationMethod = param->mAreaInterpolationMethod;
        newParam.mTimeInterpolationMethod = param->mTimeInterpolationMethod;
        newParam.mLevelInterpolationMethod = param->mLevelInterpolationMethod;
        // newParam.mContourLowValues = param->mContourLowValues;
        // newParam.mContourHighValues = param->mContourHighValues;
        // newParam.mContourColors = param->mContourColors;
        newParam.mProducerId = (*val)->mProducerId;
        newParam.mGenerationFlags = (*val)->mGenerationFlags;
        newParam.mPrecision = param->mPrecision;
        newParam.mTemporary = param->mTemporary;
        newParam.mFunction = param->mFunction;
        newParam.mFunctionParams = param->mFunctionParams;
        newParam.mTimestepsBefore = param->mTimestepsBefore;
        newParam.mTimestepsAfter = param->mTimestepsAfter;
        newParam.mTimestepSizeInMinutes = param->mTimestepSizeInMinutes;
        newParam.mFlags = param->mFlags;

        newQuery.mQueryParameterList.emplace_back(newParam);

        queryStreamer->addQuery(newQuery);
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

bool GridInterface::isValidDefaultRequest(
    const std::vector<uint>& defaultGeometries,
    const std::vector<std::vector<T::Coordinate>>& polygonPath,
    T::GeometryId_set& geometryIdList)
{
  try
  {
    // This method was needed when there was not enough data for serving requests without
    // producer definition (= default request). For example, we had data only in scandinavian
    // area and we could not serve requests outside of this area. This method was used in
    // order to check if we had any data in the requested location. Otherwise the request
    // was forwarded to the queryEngine.

    if (defaultGeometries.empty())
      return true;

    for (const auto& geom : defaultGeometries)
    {
      bool match = true;
      for (const auto& cList : polygonPath)
      {
        for (const auto& coordinate : cList)
        {
          double grid_i = ParamValueMissing;
          double grid_j = ParamValueMissing;
          if (!Identification::gridDef.getGridPointByGeometryIdAndLatLonCoordinates(
                  geom, coordinate.y(), coordinate.x(), grid_i, grid_j))
            match = false;
        }
      }

      if (match)
        geometryIdList.insert(geom);
    }

    return !geometryIdList.empty();
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void GridInterface::prepareQueryTimes(QueryServer::Query& gridQuery,
                                      const Query& masterquery,
                                      const Spine::LocationPtr& loc)
{
  FUNCTION_TRACE
  try
  {
    // Timezone accoring to the requested location.

    std::string timezoneName = loc->timezone;
    Fmi::TimeZonePtr tz = itsTimezones.time_zone_from_string(loc->timezone);

    // If the query contains a specific timezone definition then we should use it instead of
    // the location based time zone.

    if (masterquery.timezone != "localtime")
    {
      timezoneName = masterquery.timezone;
      tz = itsTimezones.time_zone_from_string(timezoneName);
    }

    // These boolean variables define if the start time and the end time is defined in UTC.

    bool startTimeUTC = masterquery.toptions.startTimeUTC;
    bool endTimeUTC = masterquery.toptions.endTimeUTC;

    // The orginal start time and the end time of the query
    const auto mk_ldt = [](const Fmi::DateTime& time, const Fmi::TimeZonePtr& tz, bool is_utc) -> Fmi::LocalDateTime
      {
        if (is_utc)
          return Fmi::LocalDateTime(time, tz);
        else
          return Fmi::LocalDateTime(time.date(), time.time_of_day(), tz);
      };

    Fmi::LocalDateTime startTime = mk_ldt(masterquery.toptions.startTime, tz, startTimeUTC);
    Fmi::LocalDateTime endTime = mk_ldt(masterquery.toptions.endTime, tz, endTimeUTC);

    // These variables will contain the actual start time and end time used in grid-query after
    // we have fixed and checked some things.

    Fmi::LocalDateTime grid_startTime = startTime;
    Fmi::LocalDateTime grid_endTime = endTime;

    // Number of the requested timesteps.

    uint steps = 24;
    if (masterquery.toptions.timeSteps)
      steps = *masterquery.toptions.timeSteps;

    // Size of the requested timestep in seconds.

    uint step = 3600;
    if (masterquery.toptions.timeStep && *masterquery.toptions.timeStep != 0)
      step = *masterquery.toptions.timeStep * 60;

    // The start time of the query is rounded to the next valid timestep. I.e. the
    // time-step query must start from the valid timestep, which are counted
    // from the beginning of the day (by using localtime).

    if (masterquery.toptions.mode == TS::TimeSeriesGeneratorOptions::TimeSteps ||
        masterquery.toptions.mode == TS::TimeSeriesGeneratorOptions::FixedTimes ||
        masterquery.toptions.mode == TS::TimeSeriesGeneratorOptions::GraphTimes)
    {
      Fmi::DateTime startT = startTime.local_time();
      unsigned seconds = startT.time_of_day().total_seconds();

      uint tstep = step;
      if (masterquery.toptions.mode == TS::TimeSeriesGeneratorOptions::GraphTimes)
        tstep = 3600;

      seconds = tstep * ((seconds + tstep - 1) / tstep);
      grid_startTime = Fmi::LocalDateTime(startT.date(), Fmi::Seconds(seconds), startTime.zone());
    }


    // If the daylight saving time is in the request time interval then we need
    // to add an extra hour to the end time of the query.

    bool daylightSavingEnds = grid_startTime.dst_on() and not grid_endTime.dst_on();
    if (daylightSavingEnds)
    {
      if (masterquery.toptions.timeSteps && *masterquery.toptions.timeSteps != 0)
      {
        // Adding one hour to the end time because of the daylight saving.

        auto ptime = grid_endTime;
        ptime = ptime + Fmi::Hours(1);
        grid_endTime = ptime;
      }
    }

    // This is a request in a request sequence. The start time in this case is the same as
    // the end time of the previous request.

    // FIXME: masterquery.latestTimestamp can be in UTC or localtime depending on startTimeUTC
    //        That causes problem when lastTimestep happens to be exactly at change from DST to
    //        normal time (requires changes in Query.cpp and probaly somwhere else).

    Fmi::LocalDateTime latestTime = mk_ldt(masterquery.latestTimestep, tz, startTimeUTC);
    Fmi::DateTime latestTimeUTC = latestTime.utc_time();

    if (latestTime != startTime)
      grid_startTime = latestTime;

    // At this point we should know the actual start and end times of the request.

    if (masterquery.toptions.days.empty() &&
        (masterquery.toptions.startTimeData || masterquery.toptions.endTimeData ||
         masterquery.toptions.mode == TS::TimeSeriesGeneratorOptions::DataTimes ||
         masterquery.toptions.mode == TS::TimeSeriesGeneratorOptions::GraphTimes))
    {
      // This is a time-range request, which means that
      //
      //   1) we do not know the actual start time or the end time of the request
      //      because they depend on the actual data (startTime == data ||  endtime == data).
      //      This means that we cannot count timesteps in advance. I.e. the Grid-engine/QueryServer
      //      should count these steps if needed.
      //
      //      or
      //
      //   2) we should fetch all data between the given interval and using their original
      //      timestamps (timestep == data) instead on predefined timesteps.

      if (masterquery.toptions.startTimeData)
      {
        // startTime is "data", which means that we should read all the data from the beginning
        gridQuery.mFlags = gridQuery.mFlags | QueryServer::Query::Flags::StartTimeFromData;
        grid_startTime = y1900; // "19000101T000000";

        if (!masterquery.toptions.endTimeData)
          gridQuery.mTimesteps = steps;
      }

      if (masterquery.toptions.endTimeData)
      {
        // endTime is "data", which means that we should read all the data to the end
        gridQuery.mFlags = gridQuery.mFlags | QueryServer::Query::Flags::EndTimeFromData;
        grid_endTime = y2100;
      }

      if (masterquery.toptions.mode == TS::TimeSeriesGeneratorOptions::DataTimes)
      {
        gridQuery.mFlags = gridQuery.mFlags | QueryServer::Query::Flags::TimeStepIsData;
      }

      if (masterquery.toptions.mode == TS::TimeSeriesGeneratorOptions::GraphTimes)
      {
        gridQuery.mFlags = gridQuery.mFlags | QueryServer::Query::Flags::TimeStepIsData |
                           QueryServer::Query::Flags::ForceStartTime;
      }

      if (masterquery.toptions.mode == TS::TimeSeriesGeneratorOptions::TimeSteps)
      {
        gridQuery.mTimesteps = 0;
        gridQuery.mTimestepSizeInMinutes = step / 60;
      }

      if (latestTime != startTime)
      {
        // This is a request which start time is that same as the end time of the previous
        // requests. That's why we should inform the QueryServer that it should ignore
        // the first time step (because the previous query has already fetched that time).
        // We are using flags, because we do not know what will be the next time in
        // the sequence. I.e. we do not the correct start time.

        gridQuery.mFlags = gridQuery.mFlags | QueryServer::Query::Flags::StartTimeNotIncluded;
      }

      gridQuery.mSearchType = QueryServer::Query::SearchType::TimeRange;

      if (masterquery.toptions.timeSteps && *masterquery.toptions.timeSteps > 0)
      {
        gridQuery.mMaxParameterValues = *masterquery.toptions.timeSteps;
        if (daylightSavingEnds)
          gridQuery.mMaxParameterValues++;

        gridQuery.mTimesteps = gridQuery.mMaxParameterValues;
        if (grid_startTime == grid_endTime)
          grid_endTime = y2100; // "21000101T000000"
      }
    }
    else
    {
      // This is a time-step request, which means that we shoud generate valid timesteps

      Fmi::DateTime s = grid_startTime.utc_time();
      Fmi::DateTime e = grid_endTime.utc_time();

      if (masterquery.toptions.timeSteps)
      {
        if (daylightSavingEnds)
          steps = steps + 3600 / step;
        e = s + Fmi::Seconds(steps * step);
      }

      if (!masterquery.toptions.timeList.empty())
      {
        step = 60;
        if (masterquery.toptions.timeSteps && *masterquery.toptions.timeSteps > 0)
          e = s + Fmi::Hours(10 * 365 * 24);
      }

      uint stepCount = 0;
      while (s <= e)
      {
        bool additionOk = true;
        const Fmi::LocalDateTime sLoc(s, tz);

        if (latestTime != startTime.utc_time() && s <= latestTimeUTC)
          additionOk = false;

        if (additionOk && !masterquery.toptions.timeList.empty())
        {
          // If we have a list of predefined hours (for example, hour=4,8,12) the we should
          // ignore timesteps that does not match these hours. These hours are defined in
          // localtime and that's why need to convert UTC timesteps back to localtimes.

          const Fmi::TimeDuration part = sLoc.local_time().time_of_day();
          const int hours = part.hours();
          const int minutes = part.minutes();
          uint idx = hours * 100 + minutes;
          if (masterquery.toptions.timeList.find(idx) == masterquery.toptions.timeList.end())
            additionOk = false;
        }

        if (additionOk && !masterquery.toptions.days.empty())
        {
          auto d = sLoc.local_time().date().day();
          if (masterquery.toptions.days.find(d) == masterquery.toptions.days.end())
            additionOk = false;
        }

        if (additionOk)
        {
          stepCount++;
          gridQuery.mForecastTimeList.insert(toTimeT(s));

          if (masterquery.toptions.timeSteps && stepCount == steps)
            s = e;
        }
        s = s + Fmi::Seconds(step);
      }
    }

    gridQuery.mStartTime = (grid_startTime.utc_time() - y1970).total_seconds();
    gridQuery.mEndTime = (grid_endTime.utc_time() - y1970).total_seconds();
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void GridInterface::prepareProducer(QueryServer::Query& gridQuery,
                                    const Query& masterquery,
                                    int origLevelId,
                                    const AreaProducers& areaproducers,
                                    int& levelId,
                                    int& geometryId)
{
  FUNCTION_TRACE
  try
  {
    levelId = origLevelId;
    geometryId = -1;

    // Producers
    for (const auto& areaproducer : areaproducers)
    {
      std::string producerName = itsGridEngine->getProducerAlias(areaproducer, levelId);

      Engine::Grid::ParameterDetails_vec parameters;
      itsGridEngine->getParameterDetails(producerName, parameters);

      for (const auto& param : parameters)
      {
        if (masterquery.leveltype.empty() && levelId < 0 && !param.mLevelId.empty())
          levelId = toInt32(param.mLevelId);

        if (!param.mGeometryId.empty())
          geometryId = toInt32(param.mGeometryId);

        if (!param.mProducerName.empty())
          gridQuery.mProducerNameList.emplace_back(param.mProducerName);
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void GridInterface::prepareGeneration(QueryServer::Query& gridQuery,
                                      const Query& masterquery,
                                      bool& sameParamAnalysisTime)
{
  FUNCTION_TRACE
  try
  {
    // Generations: Enabling all generations
    gridQuery.mGenerationFlags = 0;

    if (masterquery.origintime)
    {
      if (masterquery.origintime == Fmi::DateTime(Fmi::DateTime::POS_INFINITY))
      {
        // Generation: latest, newest
        gridQuery.mFlags = gridQuery.mFlags | QueryServer::Query::Flags::LatestGeneration;
      }
      else if (masterquery.origintime == Fmi::DateTime(Fmi::DateTime::NEG_INFINITY))
      {
        // Generation: oldest
        gridQuery.mFlags = gridQuery.mFlags | QueryServer::Query::Flags::OldestGeneration;
      }
      else
      {
        gridQuery.mAnalysisTime = Fmi::to_iso_string(*masterquery.origintime);
      }
    }

    gridQuery.mAttributeList = masterquery.attributeList;

    T::Attribute* attr = masterquery.attributeList.getAttribute("query.analysisTime");
    if (attr != nullptr)
    {
      // This attribute forces that all requested parameters have the same origintime/analysisTime
      // (attr=query.analysisTime:same)

      if (strcasecmp(attr->mValue.c_str(), "same") == 0)
        gridQuery.mFlags = gridQuery.mFlags | QueryServer::Query::Flags::SameAnalysisTime;
    }

    attr = masterquery.attributeList.getAttribute("analysisTime.status");
    if (attr != nullptr)
    {
      // This attribute enables requesting parameters from generations that are not yet ready
      // (attr=analysisTime.status:any)

      if (strcasecmp(attr->mValue.c_str(), "any") == 0)
        gridQuery.mFlags = gridQuery.mFlags | QueryServer::Query::Flags::AcceptNotReadyGenerations;
    }

    sameParamAnalysisTime = false;
    attr = masterquery.attributeList.getAttribute("param.analysisTime");
    if (attr != nullptr)
    {
      if (strcasecmp(attr->mValue.c_str(), "same") == 0)
        sameParamAnalysisTime = true;
    }
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

bool GridInterface::isBuildInParameter(const char* parameter)
{
  FUNCTION_TRACE
  try
  {
    const char* buildIdParameters[] = {
        "origintime", "modtime", "mtime", "level", "lat", "lon", "latlon", "lonlat", "@", nullptr};

    int c = 0;
    while (buildIdParameters[c] != nullptr)
    {
      if (strncasecmp(buildIdParameters[c], parameter, strlen(buildIdParameters[c])) == 0)
        return true;
      c++;
    }
    return AdditionalParameters::isAdditionalParameter(parameter);
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void GridInterface::prepareLocation(QueryServer::Query& gridQuery,
                                    const Query& masterquery,
                                    const Spine::LocationPtr& loc,
                                    const T::GeometryId_set& geometryIdList,
                                    std::vector<std::vector<T::Coordinate>>& polygonPath,
                                    uchar& locationType)
{
  FUNCTION_TRACE
  try
  {
    gridQuery.mLanguage = masterquery.language;
    gridQuery.mGeometryIdList = geometryIdList;

    gridQuery.mAreaCoordinates = polygonPath;
    gridQuery.mRadius = loc->radius;

    locationType = QueryServer::QueryParameter::LocationType::Point;
    switch (loc->type)
    {
      case Spine::Location::Place:
      case Spine::Location::CoordinatePoint:
        if (loc->radius == 0)
          locationType = QueryServer::QueryParameter::LocationType::Point;
        else
          locationType = QueryServer::QueryParameter::LocationType::Circle;
        break;

      case Spine::Location::Wkt:
      {
        OGRwkbGeometryType geomType =
            masterquery.wktGeometries.getGeometry(loc->name)->getGeometryType();

        switch (geomType)
        {
          case wkbMultiLineString:
          case wkbMultiPoint:
          case wkbLineString:
            locationType = QueryServer::QueryParameter::LocationType::Path;
            break;

          default:
            locationType = QueryServer::QueryParameter::LocationType::Polygon;
            break;
        }
      }
      break;

      case Spine::Location::Area:
      case Spine::Location::BoundingBox:
        locationType = QueryServer::QueryParameter::LocationType::Polygon;
        break;

      case Spine::Location::Path:
        locationType = QueryServer::QueryParameter::LocationType::Path;
        break;
    }
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void GridInterface::prepareQueryParameters(QueryServer::Query& gridQuery,
                                           const Query& masterquery,
                                           uint mode,
                                           int levelId,
                                           int geometryId,
                                           uchar locationType,
                                           bool sameParamAnalysisTime,
                                           double origLevel,
                                           const AreaProducers& areaproducers)
{
  FUNCTION_TRACE
  try
  {
    // Converting parameter information into parameter objects that are used by the
    // grid-engine/QueryServer.

    uint idx = 0;
    uint gIdx = 0;

    for (auto paramfunc = masterquery.poptions.parameterFunctions().begin();
         paramfunc != masterquery.poptions.parameterFunctions().end();
         ++paramfunc)
    {
      Spine::Parameter param = paramfunc->parameter;

      // Defining a query parameter object that is used by the grid-engine/QueryServer
      QueryServer::QueryParameter qParam;

      qParam.mId = gIdx;
      qParam.mLocationType = locationType;

      if (strcasecmp(masterquery.format.c_str(), "INFO") == 0)
        qParam.mFlags = QueryServer::QueryParameter::Flags::NoReturnValues;

      if (sameParamAnalysisTime)
        qParam.mFlags = qParam.mFlags | QueryServer::QueryParameter::Flags::SameAnalysisTime;

      if (gIdx < masterquery.precisions.size())
        qParam.mPrecision = masterquery.precisions[gIdx];

      if (levelId > 0)
        qParam.mParameterLevelId = levelId;

      if (geometryId > 0)
        qParam.mGeometryId = geometryId;

      std::string pname = param.originalName();
      qParam.mOrigParam = pname;

      itsGridEngine->getParameterAlias(pname, pname);
      qParam.mParam = pname;

      // The parameter has ".raw" ending then there should be no area interpolation.

      auto pos = qParam.mParam.find(".raw");
      if (pos != std::string::npos)
      {
        qParam.mAreaInterpolationMethod = T::AreaInterpolationMethod::Nearest;
        qParam.mParam.erase(pos, 4);
      }

      qParam.mSymbolicName = qParam.mParam;
      qParam.mParameterKey = qParam.mParam;

      // Testing if the parameter is a build-in parameter (not actual data parameter).

      if (isBuildInParameter(qParam.mSymbolicName.c_str()))
        qParam.mParameterKeyType = T::ParamKeyTypeValue::BUILD_IN;

      // Numberic parameter is a newbase id:

      if (Fmi::stoi_opt(qParam.mParam))
        qParam.mParameterKeyType = T::ParamKeyTypeValue::NEWBASE_ID;

      // Agregation intervals:

      if ((paramfunc->functions.innerFunction.exists() || paramfunc->functions.outerFunction.exists()))
      {
        qParam.mFlags = qParam.mFlags | TimeseriesFunctionFlag;
        if (masterquery.maxAggregationIntervals.find(param.name()) != masterquery.maxAggregationIntervals.end())
        {
          uint aggregationIntervalBehind = masterquery.maxAggregationIntervals.at(param.name()).behind;
          uint aggregationIntervalAhead = masterquery.maxAggregationIntervals.at(param.name()).ahead;

          if (aggregationIntervalBehind > 0 || aggregationIntervalAhead > 0)
          {
            qParam.mTimestepsBefore = aggregationIntervalBehind / 60;
            qParam.mTimestepsAfter = aggregationIntervalAhead / 60;
            qParam.mTimestepSizeInMinutes = 60;
            qParam.mFlags = qParam.mFlags | QueryServer::QueryParameter::Flags::AggregationParameter;
          }
        }
      }

      // Level type.

      if (qParam.mParameterLevelId <= 0 && !masterquery.leveltype.empty())
      {
        if (strcasecmp(masterquery.leveltype.c_str(), "pressure") == 0)
          qParam.mParameterLevelId = 2;
        else if (strcasecmp(masterquery.leveltype.c_str(), "model") == 0)
          qParam.mParameterLevelId = 3;
        else if (isdigit(*(masterquery.leveltype.c_str())))
          qParam.mParameterLevelId = toInt32(masterquery.leveltype);
      }

      // It's good to realize that the request does not always contain the parameter level type.
      // That's because earlier the producer name defines details related to the parameter geometry
      // or the level type. Nowadays we prefer that all essential information is encoded into
      // the requested parameter (For example, T-K:ECG:1008:6:2).

      // If the geometry and the level type is unknown, then we need to searching these details
      // from the producer's parameter definition file. Otherwise we cannot know the exact detais
      // needed in order to fetch correct data.

      Engine::Grid::ParameterDetails_vec parameters;

      for (const auto& areaproducer : areaproducers)
      {
        std::string producerName = areaproducer;
        producerName = itsGridEngine->getProducerName(producerName);
        producerName = itsGridEngine->getProducerAlias(producerName, levelId);

        std::string key = producerName + ";" + qParam.mParam;

        itsGridEngine->getParameterDetails(producerName, qParam.mParam, parameters);

        size_t len = parameters.size();
        if (len > 0 && strcasecmp(parameters[0].mProducerName.c_str(), key.c_str()) != 0)
        {
          if (parameters[0].mLevel > "")
            qParam.mParameterLevel = toInt32(parameters[0].mLevel);

          if (parameters[0].mLevelId > "")
            qParam.mParameterLevelId = toInt32(parameters[0].mLevelId);

          if (parameters[0].mForecastType > "")
            qParam.mForecastType = toInt32(parameters[0].mForecastType);

          if (parameters[0].mForecastNumber > "")
            qParam.mForecastNumber = toInt32(parameters[0].mForecastNumber);

          if (parameters[0].mGeometryId > "")
          {
            qParam.mProducerName = parameters[0].mProducerName;
            qParam.mGeometryId = toInt32(parameters[0].mGeometryId);
          }
        }
      }

      switch (mode)
      {
        case 0:
          if (qParam.mParameterLevelId == 2)
          {
            // Grib uses Pa and querydata hPa, so we have to convert the value.
            qParam.mParameterLevel = C_INT(qParam.mParameterLevel * 100);
            //qParam.mFlags |= QueryServer::QueryParameter::Flags::PressureLevels;
          }
          break;

        case 1:
          qParam.mParameterLevelId = 2;
          // Grib uses Pa and querydata hPa, so we have to convert the value.
          qParam.mParameterLevel = C_INT(qParam.mParameterLevel * 100);
          qParam.mFlags |= QueryServer::QueryParameter::Flags::PressureLevels;
          break;

        case 2:
          //qParam.mParameterLevelId = 0;
          qParam.mFlags |= QueryServer::QueryParameter::Flags::MetricLevels;
          break;
      }


      if (qParam.mParameterLevel < 0)
        qParam.mParameterLevel = origLevel;

      size_t len = parameters.size();
      if (len > 1)
        qParam.mAlternativeParamId = idx + 1;

      gridQuery.mQueryParameterList.emplace_back(qParam);
      idx++;
      gIdx++;

      if (len > 1)
      {
        // The producer parameter definition file might contain some alternative mappings for some
        // parameters. The idea is that when the primary mappings does not return any data then
        // there might be a secondary mapping that returns data.

        for (uint t = 1; t < len; t++)
        {
          QueryServer::QueryParameter qp(qParam);
          qp.mId = idx;
          qp.mPrecision = qParam.mPrecision;
          qp.mFlags |= QueryServer::QueryParameter::Flags::AlternativeParameter;
          if ((t + 1) < len)
            qp.mAlternativeParamId = idx + 1;
          else
            qp.mAlternativeParamId = 0;

          if (parameters[t].mLevel > "")
            qp.mParameterLevel = toInt32(parameters[t].mLevel);

          if (parameters[t].mLevelId > "")
            qp.mParameterLevelId = toInt32(parameters[t].mLevelId);

          if (parameters[t].mForecastType > "")
            qp.mForecastType = toInt32(parameters[t].mForecastType);

          if (parameters[t].mForecastNumber > "")
            qp.mForecastNumber = toInt32(parameters[t].mForecastNumber);

          if (parameters[t].mGeometryId > "")
          {
            qp.mProducerName = parameters[t].mProducerName;
            qp.mGeometryId = toInt32(parameters[t].mGeometryId);
          }

          gridQuery.mQueryParameterList.emplace_back(qp);
          idx++;
        }
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void GridInterface::prepareGridQuery(QueryServer::Query& gridQuery,
                                     const Query& masterquery,
                                     uint mode,
                                     int origLevelId,
                                     double origLevel,
                                     const AreaProducers& areaproducers,
                                     const Spine::TaggedLocation& /* tloc */,
                                     const Spine::LocationPtr& loc,
                                     const T::GeometryId_set& geometryIdList,
                                     std::vector<std::vector<T::Coordinate>>& polygonPath)
{
  FUNCTION_TRACE
  try
  {
    int levelId = origLevelId;
    int geometryId = -1;
    bool sameParamAnalysisTime = false;
    uchar locationType = 0;

    prepareProducer(gridQuery, masterquery, origLevelId, areaproducers, levelId, geometryId);
    prepareGeneration(gridQuery, masterquery, sameParamAnalysisTime);
    prepareLocation(gridQuery, masterquery, loc, geometryIdList, polygonPath, locationType);
    prepareQueryTimes(gridQuery, masterquery, loc);
    prepareQueryParameters(gridQuery,
                           masterquery,
                           mode,
                           levelId,
                           geometryId,
                           locationType,
                           sameParamAnalysisTime,
                           origLevel,
                           areaproducers);
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

int GridInterface::getParameterIndex(QueryServer::Query& gridQuery, const std::string& param)
{
  FUNCTION_TRACE
  try
  {
    size_t pLen = gridQuery.mQueryParameterList.size();
    for (size_t p = 0; p < pLen; p++)
    {
      if (strcasecmp(gridQuery.mQueryParameterList[p].mParam.c_str(), param.c_str()) == 0 ||
          strcasecmp(gridQuery.mQueryParameterList[p].mSymbolicName.c_str(), param.c_str()) == 0)
        return p;
    }
    return -1;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void GridInterface::findLevelId(Query& masterquery,
                                const AreaProducers& areaproducers,
                                int& levelId,
                                std::string& geometryIdStr)
{
  FUNCTION_TRACE
  try
  {
    levelId = -1;

    // Checking if the level type is defined in the request.

    if (strcasecmp(masterquery.leveltype.c_str(), "pressure") == 0)
    {
      levelId = 2;
    }
    else if (strcasecmp(masterquery.leveltype.c_str(), "model") == 0)
    {
      levelId = 3;
    }

    // Checking it the level type is defined in the producer's alias definition.

    if (areaproducers.size() == 1 && masterquery.pressures.empty() && masterquery.heights.empty())
    {
      std::string producerName = *areaproducers.begin();
      if (masterquery.leveltype.empty())
      {
        // The level type is empty. Maybe it is included into producer's alias definition.

        Engine::Grid::ParameterDetails_vec parameters;
        itsGridEngine->getParameterDetails(producerName, parameters);

        size_t len = parameters.size();
        for (size_t t = 0; t < len; t++)
        {
          if (masterquery.leveltype.empty() && levelId < 0)
          {
            if (parameters[t].mLevelId > "")
            {
              // It seems that the preferred level id is included into the producer alias
              // definition.

              levelId = toInt32(parameters[t].mLevelId);
            }

            if (parameters[t].mGeometryId > "")
            {
              // The producer's alias definition contains also geometry definition. Let's pick it
              // for later use.

              geometryIdStr = parameters[t].mGeometryId;
            }
          }
        }
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void GridInterface::findLevels(Query& masterquery,
                               const AreaProducers& areaproducers,
                               uint mode,
                               int& levelId,
                               std::vector<double>& levels)
{
  FUNCTION_TRACE
  try
  {
    std::set<double> tmpLevels;

    switch (mode)
    {
      case 0:
        if (masterquery.levels.empty())
        {
          // There are no levels listen in the request. However, if the level type is defined, then
          // we should deliver all levels from the current level type from the given producer.

          if (areaproducers.size() == 1 && masterquery.pressures.empty() &&
              masterquery.heights.empty())
          {
            std::string producerName = *areaproducers.begin();

            if (levelId == 2)
            {
              // Fetching pressure levels.
              itsGridEngine->getProducerParameterLevelList(producerName, 2, 0.01, tmpLevels);
              for (auto lev = tmpLevels.rbegin(); lev != tmpLevels.rend(); ++lev)
                levels.emplace_back(*lev*100);
            }
            else if (levelId == 3)
            {
              // Fetching model levels
              itsGridEngine->getProducerParameterLevelList(producerName, 3, 1, tmpLevels);
              for (const auto& lev : tmpLevels)
                levels.emplace_back(lev);
            }
            else if (masterquery.leveltype.empty())
            {
              std::set<T::ParamLevelId> levelIdList;
              itsGridEngine->getProducerParameterLevelIdList(producerName, levelIdList);
              if (levelIdList.size() == 1)
              {
                // The producer supports only one level type

                auto levId = *levelIdList.begin();
                switch (levId)
                {
                  case 2:  // Pressure level
                    itsGridEngine->getProducerParameterLevelList(producerName, 2, 0.01, tmpLevels);
                    for (auto lev = tmpLevels.rbegin(); lev != tmpLevels.rend(); ++lev)
                      levels.emplace_back(*lev*100);
                    break;

                  case 3:  // model
                    itsGridEngine->getProducerParameterLevelList(producerName, 3, 1, tmpLevels);
                    for (const auto& lev : tmpLevels)
                      levels.emplace_back(lev);
                    break;
                }
              }
            }
          }

          if (tmpLevels.empty() && masterquery.pressures.empty() && masterquery.heights.empty())
            levels.emplace_back(-1);
        }
        else
        {
          // There are levels listed in the request.

          if (levelId == 2)
          {
            // If the level type is "pressure" then we should use these levels in reverse order.
            for (auto level = masterquery.levels.rbegin(); level != masterquery.levels.rend();
                 ++level)
              levels.emplace_back((double)(*level*100));
          }
          else
          {
            // Copying levels from the request.
            for (const auto& level : masterquery.levels)
              levels.emplace_back((double)level);
          }
        }
        break;

      case 1:  // OP
        levelId = 2;
        for (const auto& level : masterquery.pressures)
          levels.emplace_back((double)level*100);
        break;

      case 2:
        if (levelId == 2)
          levelId = 0;

        for (auto level : masterquery.heights)
          levels.push_back(static_cast<double>(level));
        break;
    }
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void GridInterface::exteractCoordinatesAndAggrecationTimes(
    std::shared_ptr<QueryServer::Query>& gridQuery,
    Fmi::TimeZonePtr tz,
    T::Coordinate_vec& coordinates,
    std::set<Fmi::LocalDateTime>& aggregationTimes)
{
  FUNCTION_TRACE
  try
  {
    bool timezoneIsUTC = false;
    if (!tz || tz.is_utc())
      timezoneIsUTC = true;

    int pLen = C_INT(gridQuery->mQueryParameterList.size());
    for (int p = 0; p < pLen; p++)
    {
      uint tLen = gridQuery->mQueryParameterList[p].mValueList.size();
      if (tLen > 0)
      {
        std::string prevLocalTime;
        for (uint t = 0; t < tLen; t++)
        {
          auto dt = Fmi::date_time::from_time_t(
              gridQuery->mQueryParameterList[p].mValueList[t]->mForecastTimeUTC);
          Fmi::LocalDateTime queryTime(dt, tz);
          std::string lt = Fmi::to_iso_string(queryTime.local_time());

          if ((gridQuery->mQueryParameterList[p].mValueList[t]->mFlags &
               QueryServer::ParameterValues::Flags::AggregationValue) != 0 ||
              (!timezoneIsUTC && prevLocalTime == lt))
          {
            // This value is added for aggregation or it has same localtime
            // as the previous timestep. We should remove it after aggregation.

            aggregationTimes.insert(queryTime);
          }

          if (coordinates.empty())
          {
            uint len = gridQuery->mQueryParameterList[p].mValueList[t]->mValueList.getLength();
            if (len > 0)
            {
              for (uint v = 0; v < len; v++)
              {
                T::GridValue val;
                if (gridQuery->mQueryParameterList[p].mValueList[t]->mValueList.getGridValueByIndex(
                        v, val))
                {
                  coordinates.emplace_back(T::Coordinate(val.mX, val.mY));
                }
              }
            }
          }
          prevLocalTime = lt;
        }
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void GridInterface::exteractQueryResult(std::shared_ptr<QueryServer::Query>& gridQuery,
                                        const State& state,
                                        Query& masterquery,
                                        TS::OutputData& outputData,
                                        const QueryServer::QueryStreamer_sptr& queryStreamer,
                                        const AreaProducers& areaproducers,
                                        Fmi::TimeZonePtr tz,
                                        const Spine::TaggedLocation& tloc,
                                        const Spine::LocationPtr& loc,
                                        const std::string& country,
                                        int levelId,
                                        double level)
{
  FUNCTION_TRACE
  try
  {
    TS::Value missing_value = TS::None();
    AdditionalParameters additionalParameters(itsTimezones,
                                              masterquery.outlocale,
                                              *masterquery.timeformatter,
                                              masterquery.valueformatter);

    const TS::OptionParsers::ParameterFunctionList& paramFuncs =
        masterquery.poptions.parameterFunctions();

    T::Coordinate_vec coordinates;
    std::set<Fmi::LocalDateTime> aggregationTimes;
    exteractCoordinatesAndAggrecationTimes(gridQuery, tz, coordinates, aggregationTimes);

    // Going through all parameters

    std::map<ulonglong, uint> pidList;

    int pIdx = 0;
    int pLen = C_INT(gridQuery->mQueryParameterList.size());
    for (int pp = 0; pp < pLen; pp++)
    {
      int pid = pp;
      int ai = gridQuery->mQueryParameterList[pp].mAlternativeParamId;

      if ((gridQuery->mQueryParameterList[pp].mFlags &
           QueryServer::QueryParameter::Flags::AlternativeParameter) == 0)
      {
        std::vector<TS::TimeSeriesData> aggregatedData;

        TS::TimeSeriesPtr tsForParameter(new TS::TimeSeries);
        TS::TimeSeriesPtr tsForNonGridParam(new TS::TimeSeries);
        TS::TimeSeriesGroupPtr tsForGroup(new TS::TimeSeriesGroup());

        int columns = 0, rows = 0;

        if (C_INT(gridQuery->mQueryParameterList.size()) > pp &&
            !gridQuery->mQueryParameterList[pp].mValueList.empty())
          gridQuery->mQueryParameterList[pp].getValueListSize(columns, rows);

        if (columns == 0 && ai > 0)
        {
          pid = ai;
          pidList[pIdx] = ai;
          gridQuery->mQueryParameterList[pid].getValueListSize(columns, rows);
        }

        int rLen = gridQuery->mQueryParameterList[pid].mValueList.size();
        int tLen = gridQuery->mForecastTimeList.size();
        bool processed = false;

        bool funct = false;
        if (gridQuery->mQueryParameterList[pid].mFlags & TimeseriesFunctionFlag)
          funct = true;

        if (columns > 0 && rLen <= tLen)
        {
          processed = true;
          // The query has returned at least some values for the parameter.

          for (int col = 0; col < columns; col++)
          {
            TS::TimeSeries ts;

            for (int t = 0; t < tLen; t++)
            {
              if (t > rLen)
              {
                Fmi::Exception exception(BCP, "Not enough timesteps!");
                exception.addParameter("Parameter", gridQuery->mQueryParameterList[pid].mParam);
                exception.addParameter("t", std::to_string(t));
                exception.addParameter("rLen", std::to_string(rLen));
                exception.addParameter("tLen", std::to_string(tLen));
                exception.addDetail(
                    "A possible reason: grid-engine and timeseries aggregation used at the same "
                    "time.");
                throw exception;
              }

              auto dt = Fmi::date_time::from_time_t(
                  gridQuery->mQueryParameterList[pid].mValueList[t]->mForecastTimeUTC);
              Fmi::LocalDateTime queryTime(dt, tz);

              T::GridValue val;

              auto rec = gridQuery->mQueryParameterList[pid].getValueListRecord(col, t);
              if (rec && (rec->mValue != ParamValueMissing || rec->mValueString.length() > 0))
              {
                pidList.insert(std::pair<ulonglong, uint>(((ulonglong)pIdx << 32) + t, pp));
              }
              else if (ai > 0)
              {
                rec = gridQuery->mQueryParameterList[ai].getValueListRecord(col, t);
                if (rec)
                  pidList.insert(std::pair<ulonglong, uint>(((ulonglong)pIdx << 32) + t, ai));
              }

              if (rec && (rec->mValue != ParamValueMissing || rec->mValueString.length() > 0))
              {
                if (rec->mValueString.length() > 0)
                {
                  // The parameter value is a string
                  TS::TimedValue tsValue(queryTime, TS::Value(rec->mValueString));
                  if (columns == 1  &&  !funct)
                  {
                    // The parameter has only single value in the timestep
                    tsForParameter->emplace_back(tsValue);
                  }
                  else
                  {
                    // The parameter has multiple values in the same timestep
                    ts.emplace_back(tsValue);
                  }
                }
                else
                {
                  // The parameter value is numeric
                  TS::TimedValue tsValue(queryTime, TS::Value(rec->mValue));
                  if (columns == 1  &&  !funct)
                  {
                    // The parameter has only single value in the timestep
                    tsForParameter->emplace_back(tsValue);
                  }
                  else
                  {
                    // The parameter has multiple values in the same timestep
                    ts.emplace_back(tsValue);
                  }
                }
              }
              else
              {
                // The parameter value is missing

                TS::TimedValue tsValue(queryTime, missing_value);
                if (columns == 1  &&  !funct)
                {
                  // The parameter has only single value in the timestep
                  tsForParameter->emplace_back(tsValue);
                }
                else
                {
                  // The parameter has multiple values in the same timestep
                  ts.emplace_back(tsValue);
                }
              }
            }

            if (columns > 1 || funct)
            {
              T::GridValue val;
              if (gridQuery->mQueryParameterList[pid].mValueList[0]->mValueList.getGridValueByIndex(
                      col, val))
              {
                TS::LonLatTimeSeries llSeries(TS::LonLat(val.mX, val.mY), ts);
                tsForGroup->emplace_back(llSeries);
              }
              else
              {
                TS::LonLatTimeSeries llSeries(TS::LonLat(0, 0), ts);
                tsForGroup->emplace_back(llSeries);
              }
            }
          }
        }

        // If "processed" variable is false then the query has returned no values for the parameter.
        // This usually means that the parameter is not a data parameter. It is most likely "a
        // special parameter" that is based on the given query location, time, level, etc.

        if (!processed && coordinates.size() > 1)
        {
          const char* cname[] = {
              "lat", "latitude", "lon", "longitude", "latlon", "lonlat", nullptr};
          uint aaa = 0;
          while (cname[aaa] != nullptr &&
                 strcasecmp(gridQuery->mQueryParameterList[pid].mParam.c_str(), cname[aaa]) != 0)
            aaa++;

          if (cname[aaa] != nullptr)
          {
            // ################################################################################
            // # The requested parameter is a latlon coordinate or a part of it.
            // ################################################################################

            processed = true;

            int len = coordinates.size();
            for (int i = 0; i < len; i++)
            {
              TS::TimeSeries ts;
              for (int t = 0; t < tLen; t++)
              {
                auto dt = Fmi::date_time::from_time_t(
                    gridQuery->mQueryParameterList[pid].mValueList[t]->mForecastTimeUTC);
                Fmi::LocalDateTime queryTime(dt, tz);

                switch (aaa)
                {
                  case 0:  // lat
                  case 1:  // latitude
                  {
                    TS::TimedValue tsValue(queryTime, TS::Value(coordinates[i].y()));
                    ts.emplace_back(tsValue);
                  }
                  break;

                  case 2:  // lon
                  case 3:  // longitude
                  {
                    TS::TimedValue tsValue(queryTime, TS::Value(coordinates[i].x()));
                    ts.emplace_back(tsValue);
                  }
                  break;
                  case 4:  // latlon
                  {
                    TS::TimedValue tsValue(queryTime,
                                           TS::LonLat(coordinates[i].y(), coordinates[i].x()));
                    ts.emplace_back(tsValue);
                  }
                  break;
                  case 5:  // lonlat
                  {
                    TS::TimedValue tsValue(queryTime,
                                           TS::LonLat(coordinates[i].x(), coordinates[i].y()));
                    ts.emplace_back(tsValue);
                  }
                  break;
                }
              }
              TS::LonLatTimeSeries llSeries(TS::LonLat(0, 0), ts);
              tsForGroup->emplace_back(llSeries);
            }
          }
        }

        if (!processed)
        {
          std::string paramValue;
          int tLen = gridQuery->mForecastTimeList.size();
          int t = 0;

          for (auto ft = gridQuery->mForecastTimeList.begin();
               ft != gridQuery->mForecastTimeList.end();
               ++ft)
          {
            auto dt = Fmi::date_time::from_time_t(*ft);
            Fmi::LocalDateTime queryTime(dt, tz);
            /*
                            if (xLen == 1)
                            {
                              size_t size =
               gridQuery->mQueryParameterList[pid].mValueList[t]->mValueData.size(); uint step = 1;
                              if (size > 0)
                                step = 256 / size;

                            }
                            else
            */
            if ((gridQuery->mQueryParameterList[pid].mFlags &
                 QueryServer::QueryParameter::Flags::InvisibleParameter) != 0)
            {
              // ################################################################################
              // # This is invisible parameter. We should do nothing to it.
              // ################################################################################
            }
            else if (strcasecmp(gridQuery->mQueryParameterList[pid].mParam.c_str(), "y") == 0)
            {
              // ################################################################################
              // # The requested parameter is Y-coordinate in the given CRS.
              // ################################################################################

              int len = coordinates.size();
              std::ostringstream output;
              if (coordinates.size() > 1)
                output << "[";

              if (masterquery.crs.empty() || masterquery.crs == "EPSG:4326")
              {
                for (int i = 0; i < len; i++)
                {
                  output << masterquery.valueformatter.format(
                      coordinates[i].y(), gridQuery->mQueryParameterList[pid].mPrecision);
                  if ((i + 1) < len)
                    output << " ";
                }
              }
              else
              {
                Fmi::CoordinateTransformation transformation("WGS84", masterquery.crs);

                for (int i = 0; i < len; i++)
                {
                  double lon = coordinates[i].x();
                  double lat = coordinates[i].y();
                  transformation.transform(lon, lat);

                  output << masterquery.valueformatter.format(
                      lat, gridQuery->mQueryParameterList[pid].mPrecision);
                  if ((i + 1) < len)
                    output << " ";
                }
              }

              if (coordinates.size() > 1)
                output << "]";

              TS::TimedValue tsValue(queryTime, TS::Value(output.str()));
              tsForNonGridParam->emplace_back(tsValue);
            }
            else if (strcasecmp(gridQuery->mQueryParameterList[pid].mParam.c_str(), "x") == 0)
            {
              // ################################################################################
              // # The requested parameter is X-coordinate in the given CRS.
              // ################################################################################

              int len = coordinates.size();
              std::ostringstream output;
              if (coordinates.size() > 1)
                output << "[";

              if (masterquery.crs.empty() || masterquery.crs == "EPSG:4326")
              {
                for (int i = 0; i < len; i++)
                {
                  output << masterquery.valueformatter.format(
                      coordinates[i].x(), gridQuery->mQueryParameterList[pid].mPrecision);
                  if ((i + 1) < len)
                    output << " ";
                }
              }
              else
              {
                Fmi::CoordinateTransformation transformation("WGS84", masterquery.crs);

                for (int i = 0; i < len; i++)
                {
                  double lon = coordinates[i].x();
                  double lat = coordinates[i].y();
                  transformation.transform(lon, lat);

                  output << masterquery.valueformatter.format(
                      lon, gridQuery->mQueryParameterList[pid].mPrecision);
                  if ((i + 1) < len)
                    output << " ";
                }
              }

              if (coordinates.size() > 1)
                output << "]";

              TS::TimedValue tsValue(queryTime, TS::Value(output.str()));
              tsForNonGridParam->emplace_back(tsValue);
            }
            else if (UtilityFunctions::is_special_parameter(
                         gridQuery->mQueryParameterList[pid].mParam))
            {
              // ################################################################################
              // # The requested parameter is a special parameter.
              // ################################################################################

              // These parameters might be partly same that is checked in the next two conditions.

              auto paramname = gridQuery->mQueryParameterList[pid].mParam;
              auto timezone = masterquery.timezone;
              if (timezone == LOCALTIME_PARAM)
                timezone = loc->timezone;

              if (TS::is_time_parameter(paramname))
              {
                TS::Value pValue = TS::time_parameter(paramname,
                                                      queryTime,
                                                      state.getTime(),
                                                      *loc,
                                                      timezone,
                                                      itsTimezones,
                                                      masterquery.outlocale,
                                                      *masterquery.timeformatter,
                                                      masterquery.timestring);

                TS::TimedValue tsValue(queryTime, pValue);
                tsForNonGridParam->emplace_back(tsValue);
              }
              else if (TS::is_location_parameter(gridQuery->mQueryParameterList[pid].mParam))
              {
                TS::Value pValue =
                    TS::location_parameter(loc,
                                           paramname,
                                           masterquery.valueformatter,
                                           timezone,
                                           gridQuery->mQueryParameterList[pid].mPrecision,
                                           masterquery.crs);
                TS::TimedValue tsValue(queryTime, pValue);
                tsForNonGridParam->emplace_back(tsValue);
              }
            }
            else if (additionalParameters.getParameterValueByLocation(
                         gridQuery->mQueryParameterList[pid].mParam,
                         tloc.tag,
                         loc,
                         country,
                         gridQuery->mQueryParameterList[pid].mPrecision,
                         paramValue))
            {
              // ################################################################################
              // # The requested parameter is counted according to the given location.
              // ################################################################################

              TS::TimedValue tsValue(queryTime, paramValue);
              tsForNonGridParam->emplace_back(tsValue);
            }
            else if (additionalParameters.getParameterValueByLocationAndTime(
                         gridQuery->mQueryParameterList[pid].mParam,
                         tloc.tag,
                         loc,
                         queryTime,
                         tz,
                         gridQuery->mQueryParameterList[pid].mPrecision,
                         paramValue))
            {
              // ################################################################################
              // # The requested parameter is counted according to the given location and time.
              // ################################################################################

              TS::TimedValue tsValue(queryTime, paramValue);
              tsForNonGridParam->emplace_back(tsValue);
            }
            else if (gridQuery->mQueryParameterList[pid].mParam.substr(0, 1) == "@")
            {
              // ################################################################################
              // # Details releated to a requested parameter (identified by index number)
              // ################################################################################

              // When the caller want to get more details about a result parameter, he/she can
              // request that by using syntax: '@' <detailId>-<paramIndex>. For example:
              //
              //    @P-0    => The producer name of the first parameter in the result (index starts
              //    from zero).
              //    @P-1    => The producer name of the second parameter in the result.
              //    @I-3    => The complete parameter identifier (for example: T-K:ECG:1093:6:2:3:7)

              std::string n;
              int idx = -1;

              if (gridQuery->mQueryParameterList[pid].mParam.substr(2, 1) == "-")
              {
                n = gridQuery->mQueryParameterList[pid].mParam.substr(1, 1);
                idx = std::stoi(gridQuery->mQueryParameterList[pid].mParam.substr(3));
              }
              else if (gridQuery->mQueryParameterList[pid].mParam.substr(3, 1) == "-")
              {
                n = gridQuery->mQueryParameterList[pid].mParam.substr(1, 2);
                idx = std::stoi(gridQuery->mQueryParameterList[pid].mParam.substr(4));
              }

              if (idx >= 0 && idx < pLen)
              {
                auto cpid = pidList.find(((ulonglong)(idx) << 32) + t);
                if (cpid != pidList.end())
                {
                  uint i = cpid->second;
                  std::string producerName;
                  T::ProducerInfo producer;
                  if (itsGridEngine->getProducerInfoById(
                          gridQuery->mQueryParameterList[i].mValueList[t]->mProducerId, producer))
                    producerName = producer.mName;

                  if (n == "I")
                  {
                    auto tmp = fmt::format(
                        "{}:{}:{}:{}:{}:{}:{}",
                        gridQuery->mQueryParameterList[i].mValueList[t]->mParameterKey,
                        producerName,
                        gridQuery->mQueryParameterList[i].mValueList[t]->mGeometryId,
                        C_INT(gridQuery->mQueryParameterList[i].mValueList[t]->mParameterLevelId),
                        C_INT(gridQuery->mQueryParameterList[i].mValueList[t]->mParameterLevel),
                        C_INT(gridQuery->mQueryParameterList[i].mValueList[t]->mForecastType),
                        C_INT(gridQuery->mQueryParameterList[i].mValueList[t]->mForecastNumber));
                    TS::TimedValue tsValue(queryTime, tmp);
                    tsForNonGridParam->emplace_back(tsValue);
                  }
                  else if (n == "P")  // Produceer
                  {
                    TS::TimedValue tsValue(queryTime, producerName);
                    tsForNonGridParam->emplace_back(tsValue);
                  }
                  else if (n == "G")  // Generation
                  {
                    T::GenerationInfo info;
                    if (itsGridEngine->getGenerationInfoById(
                            C_INT(gridQuery->mQueryParameterList[i].mValueList[t]->mGenerationId),
                            info))
                    {
                      TS::TimedValue tsValue(queryTime, info.mName);
                      tsForNonGridParam->emplace_back(tsValue);
                    }
                    else
                    {
                      TS::TimedValue tsValue(
                          queryTime,
                          C_INT(gridQuery->mQueryParameterList[i].mValueList[t]->mGenerationId));
                      tsForNonGridParam->emplace_back(tsValue);
                    }
                  }
                  else if (n == "GM")  // Geometry
                  {
                    TS::TimedValue tsValue(
                        queryTime,
                        C_INT(gridQuery->mQueryParameterList[i].mValueList[t]->mGeometryId));
                    tsForNonGridParam->emplace_back(tsValue);
                  }
                  else if (n == "LT")  // Level type
                  {
                    TS::TimedValue tsValue(
                        queryTime,
                        C_INT(gridQuery->mQueryParameterList[i].mValueList[t]->mParameterLevelId));
                    tsForNonGridParam->emplace_back(tsValue);
                  }
                  else if (n == "L")  // Level
                  {
                    TS::TimedValue tsValue(
                        queryTime,
                        C_INT(gridQuery->mQueryParameterList[i].mValueList[t]->mParameterLevel));
                    tsForNonGridParam->emplace_back(tsValue);
                  }
                  else if (n == "FT")  // Forcast type
                  {
                    TS::TimedValue tsValue(
                        queryTime,
                        C_INT(gridQuery->mQueryParameterList[i].mValueList[t]->mForecastType));
                    tsForNonGridParam->emplace_back(tsValue);
                  }
                  else if (n == "FN")  // Forecast number
                  {
                    TS::TimedValue tsValue(
                        queryTime,
                        C_INT(gridQuery->mQueryParameterList[i].mValueList[t]->mForecastNumber));
                    tsForNonGridParam->emplace_back(tsValue);
                  }
                  else if (n == "AT")  // Analysis time / origintime
                  {
                    TS::TimedValue tsValue(
                        queryTime, gridQuery->mQueryParameterList[i].mValueList[t]->mAnalysisTime);
                    tsForNonGridParam->emplace_back(tsValue);
                  }
                  else
                  {
                    TS::TimedValue tsValue(queryTime, missing_value);
                    tsForNonGridParam->emplace_back(tsValue);
                  }
                }
                else
                {
                  TS::TimedValue tsValue(queryTime, missing_value);
                  tsForNonGridParam->emplace_back(tsValue);
                }
              }
            }
            else if (gridQuery->mQueryParameterList[pid].mParam == "level")
            {
              // ################################################################################
              // # The requested parameter is the parameter level.
              // ################################################################################

              // This parameter was used earier, but it is not very informative anymore, because
              // each parameter can have different level.

              int idx = 0;
              int levelValue = level;
              if (levelValue > 0  &&  levelId == 2)
                levelValue = level / 100;

              while (idx < pLen && levelValue <= 0)
              {
                if (gridQuery->mQueryParameterList[idx].mValueList[t]->mParameterLevel > 0)
                {
                  levelValue = gridQuery->mQueryParameterList[idx].mValueList[t]->mParameterLevel;
                  if (levelValue < 0)
                    level = 0;

                  if (gridQuery->mQueryParameterList[idx].mValueList[t]->mParameterLevelId == 2)
                    levelValue = levelValue / 100;
                }
                idx++;
              }
              if (levelValue < 0)
                levelValue = 0;

              TS::TimedValue tsValue(queryTime, levelValue);
              tsForNonGridParam->emplace_back(tsValue);
            }
            else if (gridQuery->mQueryParameterList[pid].mParam == "model" ||
                     gridQuery->mQueryParameterList[pid].mParam == "producer")
            {
              // ################################################################################
              // # The requested parameter is the parameter model/producer
              // ################################################################################

              // This parameter was used earier, but it is not very informative anymore, because
              // each parameter can have different model/producer.

              int idx = 0;
              while (idx < pLen)
              {
                if (gridQuery->mQueryParameterList[idx].mValueList[t]->mProducerId > 0)
                {
                  T::ProducerInfo producer;
                  if (itsGridEngine->getProducerInfoById(
                          gridQuery->mQueryParameterList[idx].mValueList[t]->mProducerId, producer))
                  {
                    TS::TimedValue tsValue(queryTime, producer.mName);
                    tsForNonGridParam->emplace_back(tsValue);
                    idx = pLen + 10;
                  }
                }
                idx++;
              }

              if (idx == pLen)
              {
                std::string producer = "Unknown";
                if (gridQuery->mProducerNameList.size() == 1)
                {
                  std::vector<std::string> pnameList;
                  itsGridEngine->getProducerNameList(gridQuery->mProducerNameList[0], pnameList);
                  if (!pnameList.empty())
                    producer = pnameList[0];
                }

                TS::TimedValue tsValue(queryTime, producer);
                tsForNonGridParam->emplace_back(tsValue);
              }
            }
            else if (gridQuery->mQueryParameterList[pid].mParam == "origintime")
            {
              // ################################################################################
              // # The requested parameter is the parameter origintime / analysis time.
              // ################################################################################

              // This parameter was used earier, but it is not very informative anymore, because
              // each parameter can have different origintime/analysis time.

              int idx = 0;
              while (idx < pLen)
              {
                if (gridQuery->mQueryParameterList[idx].mValueList[t]->mGenerationId > 0)
                {
                  T::GenerationInfo info;
                  bool res = itsGridEngine->getGenerationInfoById(
                      gridQuery->mQueryParameterList[idx].mValueList[t]->mGenerationId, info);
                  if (res)
                  {
                    Fmi::LocalDateTime origTime(toTimeStamp(info.mAnalysisTime), tz);
                    TS::TimedValue tsValue(queryTime, masterquery.timeformatter->format(origTime));
                    tsForNonGridParam->emplace_back(tsValue);
                    idx = pLen + 10;
                  }
                }
                idx++;
              }
              if (idx == pLen)
              {
                TS::TimedValue tsValue(queryTime, std::string("Unknown"));
                tsForNonGridParam->emplace_back(tsValue);
              }
            }
            else if (gridQuery->mQueryParameterList[pid].mParam == "modtime" ||
                     gridQuery->mQueryParameterList[pid].mParam == "mtime")
            {
              // ################################################################################
              // # The requested parameter is the parameter modification time.
              // ################################################################################

              // This parameter was used earier, but it is not very informative anymore, because
              // each parameter can have different modification time.

              int idx = 0;
              while (idx < pLen)
              {
                if (gridQuery->mQueryParameterList[idx].mValueList[t]->mModificationTime > 0)
                {
                  auto utcT = Fmi::date_time::from_time_t(
                      gridQuery->mQueryParameterList[idx].mValueList[t]->mModificationTime);
                  Fmi::LocalDateTime modTime(utcT, tz);
                  TS::TimedValue tsValue(queryTime, masterquery.timeformatter->format(modTime));
                  tsForNonGridParam->emplace_back(tsValue);
                  idx = pLen + 10;
                }
                idx++;
              }

              if (idx == pLen)
              {
                TS::TimedValue tsValue(queryTime, std::string("Unknown"));
                tsForNonGridParam->emplace_back(tsValue);
              }
            }
            else if ((gridQuery->mQueryParameterList[pid].mFlags &
                      QueryServer::QueryParameter::Flags::NoReturnValues) != 0 &&
                     (gridQuery->mQueryParameterList[pid].mValueList[t]->mFlags &
                      QueryServer::ParameterValues::Flags::DataAvailable) != 0)
            {
              // ################################################################################
              // # The requested parameter is not expected to return any values.
              // ################################################################################

              // When the request format is "INFO", no data is fetched. However, it shows the exact
              // identification of the parameters that would be fetched in a normal request. This
              // feature can be used in order to check that data comes from expected producers,
              // generations, geometeries, level types and levels.

              std::string producerName;
              T::ProducerInfo producer;
              if (itsGridEngine->getProducerInfoById(
                      gridQuery->mQueryParameterList[pid].mValueList[t]->mProducerId, producer))
                producerName = producer.mName;

              auto tmp = fmt::format(
                  "{}:{}:{}:{}:{}:{}:{} {} flags:{}",
                  gridQuery->mQueryParameterList[pid].mValueList[t]->mParameterKey,
                  producerName,
                  gridQuery->mQueryParameterList[pid].mValueList[t]->mGeometryId,
                  C_INT(gridQuery->mQueryParameterList[pid].mValueList[t]->mParameterLevelId),
                  C_INT(gridQuery->mQueryParameterList[pid].mValueList[t]->mParameterLevel),
                  C_INT(gridQuery->mQueryParameterList[pid].mValueList[t]->mForecastType),
                  C_INT(gridQuery->mQueryParameterList[pid].mValueList[t]->mForecastNumber),
                  gridQuery->mQueryParameterList[pid].mValueList[t]->mAnalysisTime,
                  C_INT(gridQuery->mQueryParameterList[pid].mValueList[t]->mFlags));

              TS::TimedValue tsValue(queryTime, std::string(tmp));
              tsForParameter->emplace_back(tsValue);
            }
            else if (rLen > tLen)
            {
              // It seems that there are more values available for each time step than
              // expected. This usually happens if the requested parameter definition does not
              // contain enough information in order to identify an unique parameter. For
              // example, if the parameter definition does not contain level information then
              // the result is that we get values from several levels.

              std::set<std::string> pList;

              for (int r = 0; r < rLen; r++)
              {
                if (gridQuery->mQueryParameterList[pid].mValueList[r]->mForecastTimeUTC == *ft)
                {
                  std::string producerName;
                  T::ProducerInfo producer;
                  if (itsGridEngine->getProducerInfoById(
                          gridQuery->mQueryParameterList[pid].mValueList[r]->mProducerId, producer))
                    producerName = producer.mName;

                  std::string tmp = fmt::format(
                      "{}:{}:{}:{}:{}:{}",
                      gridQuery->mQueryParameterList[pid].mValueList[r]->mParameterKey,
                      C_INT(gridQuery->mQueryParameterList[pid].mValueList[r]->mParameterLevelId),
                      C_INT(gridQuery->mQueryParameterList[pid].mValueList[r]->mParameterLevel),
                      C_INT(gridQuery->mQueryParameterList[pid].mValueList[r]->mForecastType),
                      C_INT(gridQuery->mQueryParameterList[pid].mValueList[r]->mForecastNumber),
                      producerName);

                  if (pList.find(tmp) == pList.end())
                    pList.insert(tmp);
                }
              }

              std::string tmp = "### MULTI-MATCH ### ";
              for (const auto& p : pList)
              {
                tmp += p;
                tmp += ' ';
              }

              TS::TimedValue tsValue(queryTime, tmp);
              tsForNonGridParam->emplace_back(tsValue);
            }
            else
            {
              TS::TimedValue tsValue(queryTime, missing_value);
              tsForParameter->emplace_back(tsValue);
            }
            t++;
          }

          if (!tsForNonGridParam->empty())
          {
            TS::TimeSeriesPtr aggregatedTs = TS::aggregate(
                tsForNonGridParam,
                paramFuncs[pIdx].functions,
                tsForNonGridParam->getTimes());
            aggregatedTs = erase_redundant_timesteps(aggregatedTs, aggregationTimes);
            aggregatedData.emplace_back(aggregatedTs);
          }
        }

        if (!tsForParameter->empty())
        {
          TS::TimeSeriesPtr aggregatedTs = TS::aggregate(
              tsForParameter,
              paramFuncs[pIdx].functions,
              tsForParameter->getTimes());
          aggregatedTs = erase_redundant_timesteps(aggregatedTs, aggregationTimes);
          aggregatedData.emplace_back(aggregatedTs);
        }

        if (!tsForGroup->empty())
        {
          TS::TimeSeriesGroupPtr aggregatedTsg = TS::aggregate(
              tsForGroup,
              paramFuncs[pIdx].functions,
              tsForGroup->front().getTimes());
          aggregatedTsg = erase_redundant_timesteps(aggregatedTsg, aggregationTimes);
          aggregatedData.emplace_back(aggregatedTsg);
        }

        PostProcessing::store_data(aggregatedData, masterquery, outputData);
        pIdx++;
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void GridInterface::processGridQuery(const State& state,
                                     Query& masterquery,
                                     TS::OutputData& outputData,
                                     const QueryServer::QueryStreamer_sptr& queryStreamer,
                                     const AreaProducers& areaproducers,
                                     const ProducerDataPeriod& /* producerDataPeriod */,
                                     const Spine::TaggedLocation& tloc,
                                     const Spine::LocationPtr& loc,
                                     const std::string& country,
                                     T::GeometryId_set& geometryIdList,
                                     std::vector<std::vector<T::Coordinate>>& polygonPath)
{
  FUNCTION_TRACE
  try
  {
    TS::Value missing_value = TS::None();

    auto latestTimestep = masterquery.latestTimestep;

    std::string timezoneName = loc->timezone;
    Fmi::TimeZonePtr localtz = itsTimezones.time_zone_from_string(loc->timezone);
    Fmi::TimeZonePtr tz = localtz;

    if (masterquery.timezone != "localtime")
    {
      timezoneName = masterquery.timezone;
      tz = itsTimezones.time_zone_from_string(timezoneName);
    }

    int qLevelId = -1;
    std::string geometryIdStr;

    findLevelId(masterquery, areaproducers, qLevelId, geometryIdStr);

    for (uint mode = 0; mode < 3; mode++)
    {
      std::vector<double> levels;
      findLevels(masterquery, areaproducers, mode, qLevelId, levels);

      // Requesting data level by level.

      for (const auto level : levels)
      {
        masterquery.latestTimestep = latestTimestep;

        std::shared_ptr<QueryServer::Query> originalGridQuery(new QueryServer::Query());

        if (geometryIdStr > "")
        {
          // The producer's alias definition is forcing the query to use a certain geometry.
          splitString(geometryIdStr, ',', originalGridQuery->mGeometryIdList);
        }

        // Preparing the Query object.
        prepareGridQuery(*originalGridQuery,
                         masterquery,
                         mode,
                         qLevelId,
                         level,
                         areaproducers,
                         tloc,
                         loc,
                         geometryIdList,
                         polygonPath);

        std::vector<TS::TimeSeriesData> tsdatavector;
        outputData.emplace_back(make_pair(get_location_id(tloc.loc), tsdatavector));

        if (queryStreamer != nullptr)
        {
          originalGridQuery->mFlags |= QueryServer::Query::Flags::GeometryHitNotRequired;
        }

        // Executing the query. The reqsult query object is returned as a shared pointer to a
        // Query-object. This object can point to the original query-object as well as a cached
        // query-object.

        std::shared_ptr<QueryServer::Query> gridQuery =
            itsGridEngine->executeQuery(originalGridQuery);

        if (queryStreamer != nullptr)
        {
          queryStreamer->init(0, itsGridEngine->getQueryServer_sptr());
          insertFileQueries(*gridQuery, queryStreamer);
        }

        exteractQueryResult(gridQuery,
                            state,
                            masterquery,
                            outputData,
                            queryStreamer,
                            areaproducers,
                            tz,
                            tloc,
                            loc,
                            country,
                            qLevelId,
                            level);
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
