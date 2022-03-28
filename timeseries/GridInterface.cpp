// ======================================================================
/*!
 * \brief SmartMet TimeSeries plugin implementation
 */
// ======================================================================
#include "GridInterface.h"
#include "UtilityFunctions.h"
#include "LocationTools.h"
#include "State.h"
#include <engines/grid/Engine.h>
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
#include <gis/CoordinateTransformation.h>

#define FUNCTION_TRACE FUNCTION_TRACE_OFF

using namespace std;
using namespace boost::posix_time;
using namespace boost::gregorian;
using namespace boost::local_time;

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
GridInterface::GridInterface(Engine::Grid::Engine* engine, const Fmi::TimeZones& timezones) :
    itsGridEngine(engine), itsTimezones(timezones)
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

GridInterface::~GridInterface()
{
  FUNCTION_TRACE
  try
  {
  }
  catch (...)
  {
    Fmi::Exception exception(BCP, "Destructor failed", nullptr);
    exception.printError();
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
    for (auto areaproducers = masterquery.timeproducers.begin(); areaproducers != masterquery.timeproducers.end(); ++areaproducers)
    {
      for (auto producer = areaproducers->begin(); producer != areaproducers->end(); ++producer)
      {
        if (isGridProducer(*producer))
        {
          return true;
        }
      }
    }
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
    const char removeChar[] =
    { '(', ')', '{', '}', '[', ']', ';', ' ', '\0' };

    // BOOST_FOREACH (const Spine::ParameterAndFunctions&
    // paramfunc,masterquery.poptions.parameterFunctions())

    for (const auto& paramfunc : masterquery.poptions.parameterFunctions())
    {
      Spine::Parameter param = paramfunc.parameter;

      char buf[1000];
      strcpy(buf, param.name().c_str());
      uint c1 = 0;
      uint c2 = 0;
      while (buf[c1] != '\0')
      {
        bool removeRequested = false;
        uint c = 0;
        while (removeChar[c] != '\0' && !removeRequested)
        {
          if (buf[c1] == removeChar[c])
            removeRequested = true;

          c++;
        }

        if (!removeRequested)
        {
          buf[c2] = buf[c1];
          c2++;
        }
        c1++;
      }
      buf[c2] = '\0';

      std::vector < std::string > partList;

      splitString(param.name(), ':', partList);
      for (const auto& producer : partList)
      {
        if (isGridProducer(producer))
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

T::ParamLevelId GridInterface::getLevelId(const char* producerName, const Query& masterquery)
{
  FUNCTION_TRACE
  try
  {
    T::ParamLevelId cnt[20] =
    { 0 };

    T::ProducerInfo producerInfo;
    if (itsGridEngine->getProducerInfoByName(producerName, producerInfo))
    {
      for (auto level = masterquery.levels.rbegin(); level != masterquery.levels.rend(); ++level)
      {
        T::ParamLevelId levelId = itsGridEngine->getFmiParameterLevelId(producerInfo.mProducerId, *level);
        if (levelId == 0)
        {
          // Did not find a level id. Maybe the level value is given in hPa. Let's try with Pa.
          levelId = itsGridEngine->getFmiParameterLevelId(producerInfo.mProducerId, C_INT(*level * 100));
          if (levelId != 0)
            cnt[levelId % 20]++;
        }
        else
        {
          cnt[levelId % 20]++;
        }
      }
    }

    T::ParamLevelId max = 0;
    for (T::ParamLevelId t = 0; t < 20; t++)
      if (cnt[t] > max)
        max = t;

    return max;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void GridInterface::getDataTimes(const AreaProducers& areaproducers, std::string& startTime, std::string& endTime)
{
  FUNCTION_TRACE
  try
  {
    startTime = "30000101T000000";
    endTime = "15000101T000000";

    std::shared_ptr < ContentServer::ServiceInterface > contentServer = itsGridEngine->getContentServer_sptr();
    for (auto it = areaproducers.begin(); it != areaproducers.end(); ++it)
    {
      std::vector < std::string > pnameList;
      itsGridEngine->getProducerNameList(*it, pnameList);

      for (const auto& pname : pnameList)
      {
        T::ProducerInfo producerInfo;
        if (itsGridEngine->getProducerInfoByName(pname, producerInfo))
        {
          std::set < std::string > contentTimeList;
          contentServer->getContentTimeListByProducerId(0, producerInfo.mProducerId, contentTimeList);
          if (contentTimeList.size() > 0)
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

void GridInterface::insertFileQueries(QueryServer::Query& query, const QueryServer::QueryStreamer_sptr& queryStreamer)
{
  FUNCTION_TRACE
  try
  {
    // Adding queries into the QueryStreamer. These queries fetch actual GRIB1/GRIB2 files accoding
    // to the file attribute definitions (query.mAttributeList). The queryStreamer sends these
    // queries to the queryServer and returns the results (= GRIB files) to the HTTP client.

    for (auto param = query.mQueryParameterList.begin(); param != query.mQueryParameterList.end(); ++param)
    {
      for (auto val = param->mValueList.begin(); val != param->mValueList.end(); ++val)
      {
        QueryServer::Query newQuery;

        newQuery.mSearchType = QueryServer::Query::SearchType::TimeSteps;
        // newQuery.mProducerNameList;
        newQuery.mForecastTimeList.insert((*val)->mForecastTimeUTC);
        newQuery.mAttributeList = query.mAttributeList;
        // newQuery.mCoordinateType;
        // newQuery.mAreaCoordinates;
        // newQuery.mRadius;
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
        //newParam.mParameterLevelIdType = (*val)->mParameterLevelIdType;
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
    bool ignoreGridGeometriesWhenPreloadReady,
    std::vector<std::vector<T::Coordinate>>& polygonPath,
    T::GeometryId_set& geometryIdList)
{
  try
  {
    if (defaultGeometries.empty())
      return true;

    if (ignoreGridGeometriesWhenPreloadReady)
    {
      auto dataServer = itsGridEngine->getDataServer_sptr();
      uint count = 0;

      if (dataServer->getGridMessagePreloadCount(0, count) == 0)
      {
        if (count == 0)
          return true;  // The dataServer has preload functionality active and all content is
                        // preloaded.
      }
    }

    for (auto geom = defaultGeometries.begin(); geom != defaultGeometries.end(); ++geom)
    {
      bool match = true;
      for (auto cList = polygonPath.begin(); cList != polygonPath.end(); ++cList)
      {
        for (const auto& coordinate : *cList)
        {
          double grid_i = ParamValueMissing;
          double grid_j = ParamValueMissing;
          if (!Identification::gridDef.getGridPointByGeometryIdAndLatLonCoordinates(*geom, coordinate.y(), coordinate.x(), grid_i, grid_j))
            match = false;
        }
      }

      if (match)
        geometryIdList.insert(*geom);
    }

    if (geometryIdList.size() > 0)
      return true;

    return false;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void GridInterface::prepareGridQuery(
    QueryServer::Query& gridQuery,
    AdditionalParameters& additionalParameters,
    const Query& masterquery,
    uint mode,
    int origLevelId,
    double origLevel,
    const AreaProducers& areaproducers,
    const Spine::TaggedLocation& /* tloc */,
    const Spine::LocationPtr& loc,
    T::GeometryId_set& geometryIdList,
    std::vector<std::vector<T::Coordinate>>& polygonPath)
{
  FUNCTION_TRACE
  try
  {
    const char* buildIdParameters[] =
    { "origintime", "modtime", "mtime", "level", "lat", "lon", "latlon", "lonlat", "@", nullptr };

    bool info = false;
    if (strcasecmp(masterquery.format.c_str(), "INFO") == 0)
      info = true;

    std::shared_ptr < ContentServer::ServiceInterface > contentServer = itsGridEngine->getContentServer_sptr();

    gridQuery.mLanguage = masterquery.language;

    std::string startTime = Fmi::to_iso_string(masterquery.toptions.startTime);
    std::string endTime = Fmi::to_iso_string(masterquery.toptions.endTime);

    //std::cout << startTime << " " << endTime << "\n";

    bool startTimeUTC = masterquery.toptions.startTimeUTC;
    bool endTimeUTC = masterquery.toptions.endTimeUTC;

    gridQuery.mAttributeList = masterquery.attributeList;

    T::Attribute* geomId = gridQuery.mAttributeList.getAttribute("grid.geometryId");
    T::Attribute* coordinateType = gridQuery.mAttributeList.getAttribute("contour.coordinateType");

    if (geomId != nullptr || coordinateType == nullptr)
    {
      gridQuery.mAttributeList.addAttribute("contour.coordinateType", Fmi::to_string(T::CoordinateTypeValue::GRID_COORDINATES));
    }

    std::string grid_startTime = startTime;
    std::string grid_endTime = endTime;
    gridQuery.mGeometryIdList = geometryIdList;

    /*

     // The QueryServer should be able to find this information by itself.

     if (!std::isnan(loc->dem))
     gridQuery.mAttributeList.setAttribute("dem", Fmi::to_string(loc->dem));

     gridQuery.mAttributeList.setAttribute("coverType", Fmi::to_string(loc->covertype));
     */

    std::string timezoneName = loc->timezone;
    boost::local_time::time_zone_ptr localtz = itsTimezones.time_zone_from_string(loc->timezone);

    boost::local_time::time_zone_ptr tz = localtz;
    bool daylightSavingActive = false;

    boost::local_time::time_zone_ptr utctz = itsTimezones.time_zone_from_string("UTC");

    if (masterquery.timezone != "localtime")
    {
      timezoneName = masterquery.timezone;
      tz = itsTimezones.time_zone_from_string(timezoneName);
    }

    uint steps = 24;
    if (masterquery.toptions.timeSteps)
      steps = *masterquery.toptions.timeSteps;

    uint step = 3600;

    if (masterquery.toptions.timeStep && *masterquery.toptions.timeStep != 0)
      step = *masterquery.toptions.timeStep * 60;

    std::string year = startTime.substr(0, 4);

    std::string daylightSavingTime;
    if (tz->has_dst())
      daylightSavingTime = Fmi::to_iso_string(tz->dst_local_end_time(Fmi::stoi(year)));

    // std::cout << "DAYLIGHT : " << daylightSavingTime << "\n";

    if (masterquery.toptions.mode == TS::TimeSeriesGeneratorOptions::TimeSteps || masterquery.toptions.mode == TS::TimeSeriesGeneratorOptions::FixedTimes)
    {
      std::string startT = startTime;
      if (startTimeUTC)
      {
        boost::local_time::local_date_time localTime(toTimeStamp(startTime), tz);
        startT = Fmi::to_iso_string(localTime.local_time());
      }

      startTimeUTC = false;
      grid_startTime = startTime.substr(0, 9) + "000000";

      while (grid_startTime < startT)
      {
        auto ptime = toTimeStamp(grid_startTime);
        ptime = ptime + boost::posix_time::seconds(step);
        grid_startTime = Fmi::to_iso_string(ptime);
      }

      grid_endTime = endTime.substr(0, 11) + "0000";
    }

    if (grid_startTime <= daylightSavingTime && grid_endTime >= daylightSavingTime)
    {
      daylightSavingActive = true;
      if (masterquery.toptions.timeSteps && *masterquery.toptions.timeSteps != 0)
      {
        // Adding one hour to the end time because of the daylight saving.

        auto ptime = toTimeStamp(grid_endTime);
        ptime = ptime + boost::posix_time::minutes(60);
        grid_endTime = Fmi::to_iso_string(ptime);
      }
    }

    // Converting the start time and the end time to the UTC time.

    if (!startTimeUTC)
    {
      grid_startTime = localTimeToUtc(grid_startTime, tz);
    }

    if (!endTimeUTC)
    {
      grid_endTime = localTimeToUtc(grid_endTime, tz);
    }

    std::string latestTime = Fmi::to_iso_string(masterquery.latestTimestep);
    std::string latestTimeUTC = latestTime;

    if (!masterquery.toptions.startTimeUTC)
      latestTimeUTC = localTimeToUtc(latestTime, tz);

    if (latestTime != startTime)
      grid_startTime = latestTimeUTC;

    if (masterquery.toptions.startTimeData || masterquery.toptions.endTimeData || masterquery.toptions.mode == TS::TimeSeriesGeneratorOptions::DataTimes
        || masterquery.toptions.mode == TS::TimeSeriesGeneratorOptions::GraphTimes)
    {
      if (masterquery.toptions.startTimeData)
      {
        gridQuery.mFlags = gridQuery.mFlags | QueryServer::Query::Flags::StartTimeFromData;
        grid_startTime = "19000101T000000";
      }

      if (masterquery.toptions.endTimeData)
      {
        gridQuery.mFlags = gridQuery.mFlags | QueryServer::Query::Flags::EndTimeFromData;
        grid_endTime = "21000101T000000";
      }

      if (masterquery.toptions.mode == TS::TimeSeriesGeneratorOptions::TimeSteps)
      {
        gridQuery.mTimesteps = 0;
        gridQuery.mTimestepSizeInMinutes = step / 60;
      }

      if (latestTime != startTime)
      {
        //grid_startTime = localTimeToUtc(latestTime, tz);
        gridQuery.mFlags = gridQuery.mFlags | QueryServer::Query::Flags::StartTimeNotIncluded;
      }

      gridQuery.mSearchType = QueryServer::Query::SearchType::TimeRange;

      if (masterquery.toptions.timeSteps && *masterquery.toptions.timeSteps > 0)
      {
        gridQuery.mMaxParameterValues = *masterquery.toptions.timeSteps;
        if (daylightSavingActive)
          gridQuery.mMaxParameterValues++;

        if (grid_startTime == grid_endTime)
          grid_endTime = "21000101T000000";
      }
    }
    else
    {
      auto s = toTimeStamp(grid_startTime);
      auto e = toTimeStamp(grid_endTime);

      if (masterquery.toptions.timeSteps)
      {
        if (daylightSavingActive)
          steps = steps + 3600 / step;

        e = s + boost::posix_time::seconds(steps * step);
      }

      if (masterquery.toptions.timeList.size() > 0)
      {
        step = 60;
        if (masterquery.toptions.timeSteps && *masterquery.toptions.timeSteps > 0)
          e = s + boost::posix_time::hours(10 * 365 * 24);
      }

      uint stepCount = 0;
      while (s <= e)
      {
        std::string str = Fmi::to_iso_string(s);
        boost::local_time::local_date_time localTime(s, tz);
        std::string ss = Fmi::to_iso_string(localTime.local_time());

        bool additionOk = true;

        if (latestTime != startTime && str <= latestTimeUTC)
          additionOk = false;

        if (masterquery.toptions.timeList.size() > 0)
        {
          uint idx = Fmi::stoi(ss.substr(9, 4));
          if (masterquery.toptions.timeList.find(idx) == masterquery.toptions.timeList.end())
            additionOk = false;
        }

        if (additionOk)
        {
          stepCount++;
          gridQuery.mForecastTimeList.insert(toTimeT(s));

          if (masterquery.toptions.timeSteps && stepCount == steps)
            s = e;
        }
        s = s + boost::posix_time::seconds(step);
      }
    }

    gridQuery.mStartTime = utcTimeToTimeT(grid_startTime);
    gridQuery.mEndTime = utcTimeToTimeT(grid_endTime);
    // std::cout << grid_startTime << "   -   " << grid_endTime << "\n";

    // bool sameQueryAnalysisTime = false;
    bool sameParamAnalysisTime = false;

    T::Attribute* attr = masterquery.attributeList.getAttribute("query.analysisTime");
    if (attr != nullptr)
    {
      if (strcasecmp(attr->mValue.c_str(), "same") == 0)
      {
        gridQuery.mFlags = gridQuery.mFlags | QueryServer::Query::Flags::SameAnalysisTime;
        // sameQueryAnalysisTime = true;
      }
    }

    attr = masterquery.attributeList.getAttribute("param.analysisTime");
    if (attr != nullptr)
    {
      if (strcasecmp(attr->mValue.c_str(), "same") == 0)
      {
        sameParamAnalysisTime = true;
      }
    }

    attr = masterquery.attributeList.getAttribute("analysisTime.status");
    if (attr != nullptr)
    {
      if (strcasecmp(attr->mValue.c_str(), "any") == 0)
      {
        gridQuery.mFlags = gridQuery.mFlags | QueryServer::Query::Flags::AcceptNotReadyGenerations;
      }
    }

    gridQuery.mRadius = loc->radius;

    int levelId = origLevelId;
    int geometryId = -1;

    // Producers
    for (auto it = areaproducers.begin(); it != areaproducers.end(); ++it)
    {
      std::string producerName = *it;
      producerName = itsGridEngine->getProducerAlias(producerName, levelId);

      Engine::Grid::ParameterDetails_vec parameters;
      itsGridEngine->getParameterDetails(producerName, parameters);

      size_t len = parameters.size();
      for (size_t t = 0; t < len; t++)
      {
        if (masterquery.leveltype.empty() && levelId < 0 && !parameters[t].mLevelId.empty())
          levelId = toInt32(parameters[t].mLevelId);

        if (!parameters[t].mGeometryId.empty())
          geometryId = toInt32(parameters[t].mGeometryId);

        if (!parameters[t].mProducerName.empty())
          gridQuery.mProducerNameList.emplace_back(parameters[t].mProducerName);
      }
    }

    // Generations: Enabling all generations
    gridQuery.mGenerationFlags = 0;

    if (masterquery.origintime)
    {
      if (masterquery.origintime == boost::posix_time::ptime(boost::date_time::pos_infin))
      {
        // Generation: latest, newest
        gridQuery.mFlags = gridQuery.mFlags | QueryServer::Query::Flags::LatestGeneration;
      }
      else if (masterquery.origintime == boost::posix_time::ptime(boost::date_time::neg_infin))
      {
        // Generation: oldest
        gridQuery.mFlags = gridQuery.mFlags | QueryServer::Query::Flags::OldestGeneration;
      }
      else
      {
        gridQuery.mAnalysisTime = Fmi::to_iso_string(*masterquery.origintime);
      }
    }

    // Location

    gridQuery.mAreaCoordinates = polygonPath;

    // parameters
    uint idx = 0;
    uint gIdx = 0;

    // BOOST_FOREACH (const Spine::ParameterAndFunctions& paramfunc,
    // masterquery.poptions.parameterFunctions())
    for (auto paramfunc = masterquery.poptions.parameterFunctions().begin(); paramfunc != masterquery.poptions.parameterFunctions().end(); ++paramfunc)
    {
      Spine::Parameter param = paramfunc->parameter;

      QueryServer::QueryParameter qParam;

      qParam.mId = gIdx;

      if (info)
        qParam.mFlags = QueryServer::QueryParameter::Flags::NoReturnValues;

      if (loc->type == Spine::Location::Place || loc->type == Spine::Location::CoordinatePoint)
      {
        if (loc->radius == 0)
          qParam.mLocationType = QueryServer::QueryParameter::LocationType::Point;
        else
          qParam.mLocationType = QueryServer::QueryParameter::LocationType::Circle;
      }

      if (loc->type == Spine::Location::Area || loc->type == Spine::Location::BoundingBox)
        qParam.mLocationType = QueryServer::QueryParameter::LocationType::Polygon;

      if (loc->type == Spine::Location::Path)
        qParam.mLocationType = QueryServer::QueryParameter::LocationType::Path;

      if (sameParamAnalysisTime)
        qParam.mFlags = qParam.mFlags | QueryServer::QueryParameter::Flags::SameAnalysisTime;

      if (gIdx < masterquery.precisions.size())
        qParam.mPrecision = masterquery.precisions[gIdx];

      if (levelId > 0)
        qParam.mParameterLevelId = levelId;

      if (geometryId > 0)
        qParam.mGeometryId = geometryId;

      std::string pname = param.originalName();
      itsGridEngine->getParameterAlias(pname, pname);

      std::vector < std::string > partList;

      splitString(pname, ':', partList);
      if (partList.size() > 3 && (partList[0] == "ISOBANDS" || partList[0] == "ISOLINES"))
      {
        if (partList[1] == "1")
          qParam.mFlags |= QueryServer::QueryParameter::Flags::InvisibleParameter;

        std::vector < std::string > list;
        splitString(partList[2], ';', list);
        size_t len = list.size();
        if (len > 0)
        {
          if ((len % 2) != 0)
          {
            list.emplace_back("FFFFFFFF");
            len++;
          }

          if (partList[0] == "ISOBANDS")
          {
            for (size_t a = 0; a < (len - 2); a = a + 2)
            {
              qParam.mContourLowValues.emplace_back(toDouble(list[a].c_str()));
              qParam.mContourHighValues.emplace_back(toDouble(list[a + 2].c_str()));
              qParam.mContourColors.emplace_back(strtoll(list[a + 1].c_str(), nullptr, 16));
            }
            qParam.mType = QueryServer::QueryParameter::Type::Isoband;
          }
          else
          {
            for (size_t a = 0; a < len; a = a + 2)
            {
              qParam.mContourLowValues.emplace_back(toDouble(list[a].c_str()));
              qParam.mContourColors.emplace_back(strtoll(list[a + 1].c_str(), nullptr, 16));
            }
            qParam.mType = QueryServer::QueryParameter::Type::Isoline;
          }
        }
        qParam.mLocationType = QueryServer::QueryParameter::LocationType::Geometry;

        const char* p = pname.c_str() + partList[0].size() + partList[1].size() + partList[2].size() + 3;
        qParam.mParam = p;
      }
      else
      {
        qParam.mParam = pname;
      }

      auto pos = qParam.mParam.find(".raw");
      if (pos != std::string::npos)
      {
        qParam.mAreaInterpolationMethod = T::AreaInterpolationMethod::Linear;
        qParam.mParam.erase(pos, 4);
      }

      qParam.mOrigParam = param.originalName();
      qParam.mSymbolicName = qParam.mParam;
      qParam.mParameterKey = qParam.mParam;

      int c = 0;
      while (buildIdParameters[c] != nullptr)
      {
        if (strncasecmp(buildIdParameters[c], qParam.mSymbolicName.c_str(), strlen(buildIdParameters[c])) == 0)
          qParam.mParameterKeyType = T::ParamKeyTypeValue::BUILD_IN;
        c++;
      }

      if (additionalParameters.isAdditionalParameter(qParam.mSymbolicName.c_str()))
        qParam.mParameterKeyType = T::ParamKeyTypeValue::BUILD_IN;

      if (Fmi::stoi_opt(qParam.mParam))
        qParam.mParameterKeyType = T::ParamKeyTypeValue::NEWBASE_ID;

      if (masterquery.maxAggregationIntervals.find(param.name()) != masterquery.maxAggregationIntervals.end())
      {
        unsigned int aggregationIntervalBehind = masterquery.maxAggregationIntervals.at(param.name()).behind;
        unsigned int aggregationIntervalAhead = masterquery.maxAggregationIntervals.at(param.name()).ahead;

        if (aggregationIntervalBehind > 0 || aggregationIntervalAhead > 0)
        {
          qParam.mTimestepsBefore = aggregationIntervalBehind / 60;
          qParam.mTimestepsAfter = aggregationIntervalAhead / 60;
          qParam.mTimestepSizeInMinutes = 60;
        }
      }

      if (qParam.mParameterLevelId <= 0 && !masterquery.leveltype.empty())
      {
        if (masterquery.leveltype == "pressure")
          qParam.mParameterLevelId = 2;
        else if (strcasecmp(masterquery.leveltype.c_str(), "model") == 0)
          qParam.mParameterLevelId = 3;
        else if (isdigit(*(masterquery.leveltype.c_str())))
          qParam.mParameterLevelId = toInt32(masterquery.leveltype);
      }

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

        if (qParam.mParameterLevel < 0)
          qParam.mParameterLevel = origLevel;

        switch (mode)
        {
          case 0:
            if (qParam.mParameterLevelId == 2)
            {
              // Grib uses Pa and querydata hPa, so we have to convert the value.
              qParam.mParameterLevel = C_INT(qParam.mParameterLevel * 100);
              qParam.mFlags = qParam.mFlags | QueryServer::QueryParameter::Flags::PressureLevelInterpolation;
            }
            break;

          case 1:
            qParam.mFlags = qParam.mFlags | QueryServer::QueryParameter::Flags::PressureLevelInterpolation;
            qParam.mParameterLevelId = 2;
            // Grib uses Pa and querydata hPa, so we have to convert the value.
            qParam.mParameterLevel = C_INT(qParam.mParameterLevel * 100);
            break;

          case 2:
            qParam.mParameterLevelId = 0;
            qParam.mFlags = qParam.mFlags | QueryServer::QueryParameter::Flags::HeightLevelInterpolation;
            break;
        }
      }

      size_t len = parameters.size();
      if (len > 1)
        qParam.mAlternativeParamId = idx + 1;

      gridQuery.mQueryParameterList.emplace_back(qParam);
      idx++;
      gIdx++;

      if (len > 1)
      {
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

int GridInterface::getParameterIndex(QueryServer::Query& gridQuery, const std::string& param)
{
  FUNCTION_TRACE
  try
  {
    size_t pLen = gridQuery.mQueryParameterList.size();
    for (size_t p = 0; p < pLen; p++)
    {
      if (strcasecmp(gridQuery.mQueryParameterList[p].mParam.c_str(), param.c_str()) == 0 || strcasecmp(gridQuery.mQueryParameterList[p].mSymbolicName.c_str(), param.c_str()) == 0)
        return p;
    }
    return -1;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void GridInterface::processGridQuery(
    const State& state,
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
    std::shared_ptr < ContentServer::ServiceInterface > contentServer = itsGridEngine->getContentServer_sptr();

    TS::Value missing_value = TS::None();

    auto latestTimestep = masterquery.latestTimestep;

    std::string timezoneName = loc->timezone;
    boost::local_time::time_zone_ptr localtz = itsTimezones.time_zone_from_string(loc->timezone);
    boost::local_time::time_zone_ptr tz = localtz;

    if (masterquery.timezone != "localtime")
    {
      timezoneName = masterquery.timezone;
      tz = itsTimezones.time_zone_from_string(timezoneName);
    }

    AdditionalParameters additionalParameters(itsTimezones, masterquery.outlocale, *masterquery.timeformatter, masterquery.valueformatter);

    const TS::OptionParsers::ParameterFunctionList& paramFuncs = masterquery.poptions.parameterFunctions();

    // std::cout << "LEVELTYPE : " << masterquery.leveltype << "\n";

    int qLevelId = -1;
    std::string geometryIdStr;

    if (strcasecmp(masterquery.leveltype.c_str(), "pressure") == 0)
      qLevelId = 2;
    else if (strcasecmp(masterquery.leveltype.c_str(), "model") == 0)
      qLevelId = 3;

    if (areaproducers.size() == 1 && masterquery.pressures.empty() && masterquery.heights.empty())
    {
      std::string producerName = *areaproducers.begin();
      if (masterquery.leveltype.empty())
      {
        // The level type is empty. Maybe it is included into produces alias definition.

        Engine::Grid::ParameterDetails_vec parameters;
        itsGridEngine->getParameterDetails(producerName, parameters);

        size_t len = parameters.size();
        for (size_t t = 0; t < len; t++)
        {
          // std::cout << "** PRODUCER " << *pname << "\n";
          if (masterquery.leveltype.empty() && qLevelId < 0)
          {
            if (parameters[t].mLevelId > "")
            {
              // It seems that the preferred level id is included into the producer alias
              // definition.

              qLevelId = toInt32(parameters[t].mLevelId);
            }

            if (parameters[t].mGeometryId > "")
            {
              geometryIdStr = parameters[t].mGeometryId;
            }
          }
        }
      }
    }

    for (uint mode = 0; mode < 3; mode++)
    {
      std::set<double> tmpLevels;
      std::vector<double> levels;

      int aLevelId = qLevelId;

      switch (mode)
      {
        case 0:
          if (masterquery.levels.empty())
          {
            if (areaproducers.size() == 1 && masterquery.pressures.empty() && masterquery.heights.empty())
            {
              std::string producerName = *areaproducers.begin();

              if (qLevelId == 2)
              {
                itsGridEngine->getProducerParameterLevelList(producerName, 2, 0.01, tmpLevels);
                for (auto lev = tmpLevels.rbegin(); lev != tmpLevels.rend(); ++lev)
                  levels.emplace_back(*lev);
              }
              else if (qLevelId == 3)
              {
                itsGridEngine->getProducerParameterLevelList(producerName, 3, 1, tmpLevels);
                for (auto lev = tmpLevels.begin(); lev != tmpLevels.end(); ++lev)
                  levels.emplace_back(*lev);
              }
              else if (masterquery.leveltype.empty())
              {
                std::set < T::ParamLevelId > levelIdList;
                itsGridEngine->getProducerParameterLevelIdList(producerName, levelIdList);
                if (levelIdList.size() == 1)
                {
                  // The producer supports only one level type

                  auto levelId = *levelIdList.begin();
                  switch (levelId)
                  {
                    case 2:  // Pressure level
                      itsGridEngine->getProducerParameterLevelList(producerName, 2, 0.01, tmpLevels);
                      for (auto lev = tmpLevels.rbegin(); lev != tmpLevels.rend(); ++lev)
                        levels.emplace_back(*lev);
                      break;

                    case 3:  // model
                      itsGridEngine->getProducerParameterLevelList(producerName, 3, 1, tmpLevels);
                      for (auto lev = tmpLevels.begin(); lev != tmpLevels.end(); ++lev)
                        levels.emplace_back(*lev);
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
            if (qLevelId == 2)
            {
              for (auto level = masterquery.levels.rbegin(); level != masterquery.levels.rend(); ++level)
                levels.emplace_back((double) (*level));
            }
            else
            {
              for (auto level = masterquery.levels.begin(); level != masterquery.levels.end(); ++level)
                levels.emplace_back((double) (*level));
            }
          }
          break;

        case 1:
          aLevelId = 2;
          for (auto level = masterquery.pressures.begin(); level != masterquery.pressures.end(); ++level)
            levels.emplace_back((double) (*level));
          break;

        case 2:
          aLevelId = 3;
          for (auto level : masterquery.heights)
            levels.push_back(static_cast<double>(level));
          break;
      }

      for (const auto level : levels)
      {
        masterquery.latestTimestep = latestTimestep;

        std::shared_ptr < QueryServer::Query > originalGridQuery(new QueryServer::Query());
        if (geometryIdStr > "")
          splitString(geometryIdStr, ',', originalGridQuery->mGeometryIdList);

        prepareGridQuery(*originalGridQuery, additionalParameters, masterquery, mode, aLevelId, level, areaproducers, tloc, loc, geometryIdList, polygonPath);

        std::vector<TS::TimeSeriesData> tsdatavector;
        outputData.emplace_back(make_pair(get_location_id(tloc.loc), tsdatavector));

        if (queryStreamer != nullptr)
        {
          originalGridQuery->mFlags |= QueryServer::Query::Flags::GeometryHitNotRequired;
        }

        // gridQuery->print(std::cout,0,0);

        // Executing the query. The reqsult query object is returned as a shared pointer to a
        // Query-object. This object can point to the original query-object as well as a cached
        // query-object.

        std::shared_ptr < QueryServer::Query > gridQuery = itsGridEngine->executeQuery(originalGridQuery);

        if (queryStreamer != nullptr)
        {
          queryStreamer->init(0, itsGridEngine->getQueryServer_sptr());
          insertFileQueries(*gridQuery, queryStreamer);
        }

        //        gridQuery->print(std::cout, 0, 0);

        T::Coordinate_vec coordinates;
        std::set < boost::local_time::local_date_time > aggregationTimes;

        int pLen = C_INT(gridQuery->mQueryParameterList.size());
        for (int p = 0; p < pLen; p++)
        {
          uint xLen = gridQuery->mQueryParameterList[p].mValueList.size();
          for (uint x = 0; x < xLen; x++)
          {
            if ((gridQuery->mQueryParameterList[p].mValueList[x]->mFlags & QueryServer::ParameterValues::Flags::AggregationValue) != 0)
            {
              // This value is added for aggregation. We should remove it later.

              auto dt = boost::posix_time::from_time_t(gridQuery->mQueryParameterList[p].mValueList[x]->mForecastTimeUTC);
              boost::local_time::local_date_time queryTime(dt, tz);
              if (aggregationTimes.find(queryTime) == aggregationTimes.end())
              {
                aggregationTimes.insert(queryTime);
              }
            }

            if (coordinates.empty())
            {
              uint len = gridQuery->mQueryParameterList[p].mValueList[x]->mValueList.getLength();
              if (len > 0)
              {
                for (uint v = 0; v < len; v++)
                {
                  T::GridValue val;
                  if (gridQuery->mQueryParameterList[p].mValueList[x]->mValueList.getGridValueByIndex(v, val))
                  {
                    coordinates.emplace_back(T::Coordinate(val.mX, val.mY));
                  }
                }
              }
            }
          }
        }

        unsigned long long outputTime = getTime();
        std::vector < std::string > filenameList;

        // Going through all parameters

        std::map<ulonglong,uint> pidList;
        // int pidList[1000] = { 0 };

        int pIdx = 0;
        for (int pp = 0; pp < pLen; pp++)
        {
          //pidList[pIdx] = pp;
          int pid = pp;
          int ai = gridQuery->mQueryParameterList[pp].mAlternativeParamId;

          if ((gridQuery->mQueryParameterList[pp].mFlags & QueryServer::QueryParameter::Flags::AlternativeParameter) == 0)
          {
            std::vector<TS::TimeSeriesData> aggregatedData;

            TS::TimeSeriesPtr tsForParameter(new TS::TimeSeries(state.getLocalTimePool()));
            TS::TimeSeriesPtr tsForNonGridParam(new TS::TimeSeries(state.getLocalTimePool()));
            TS::TimeSeriesGroupPtr tsForGroup(new TS::TimeSeriesGroup());

            // Counting the number of values that the parameter can have in single timestep.

            uint vLen = 0;
            uint xLen = 0;
            if (C_INT(gridQuery->mQueryParameterList.size()) > pp && gridQuery->mQueryParameterList[pp].mValueList.size() > 0)
            {
              // ### Going through all timesteps.

              uint timestepCount = gridQuery->mQueryParameterList[pp].mValueList.size();
              for (uint x = 0; x < timestepCount; x++)
              {
                if (gridQuery->mQueryParameterList[pp].mValueList[x]->mValueData.size() > 0)
                  xLen = 1;

                uint vv = gridQuery->mQueryParameterList[pp].mValueList[x]->mValueList.getLength();
                if (vv > vLen)
                {
                  vLen = vv;
                }
              }
            }

            if (vLen == 0 && ai > 0)
            {
              pid = ai;
              pidList[pIdx] = ai;
              vLen = 0;
              xLen = 0;
              if (C_INT(gridQuery->mQueryParameterList.size()) > pid && gridQuery->mQueryParameterList[pid].mValueList.size() > 0)
              {
                // ### Going through all timesteps.

                uint timestepCount = gridQuery->mQueryParameterList[pid].mValueList.size();
                for (uint x = 0; x < timestepCount; x++)
                {
                  if (gridQuery->mQueryParameterList[pid].mValueList[x]->mValueData.size() > 0)
                    xLen = 1;

                  uint vv = gridQuery->mQueryParameterList[pid].mValueList[x]->mValueList.getLength();
                  if (vv > vLen)
                  {
                    vLen = vv;
                  }
                }
              }
            }

            uint rLen = gridQuery->mQueryParameterList[pid].mValueList.size();
            uint tLen = gridQuery->mForecastTimeList.size();

            if (vLen > 0 && rLen <= tLen)
            {
              // The query has returned at least some values for the parameter.

              for (uint v = 0; v < vLen; v++)
              {
                TS::TimeSeries ts(state.getLocalTimePool());

                for (uint t = 0; t < tLen; t++)
                {
                  auto dt = boost::posix_time::from_time_t(gridQuery->mQueryParameterList[pid].mValueList[t]->mForecastTimeUTC);
                  boost::local_time::local_date_time queryTime(dt, tz);

                  T::GridValue val;

                  bool res = gridQuery->mQueryParameterList[pid].mValueList[t]->mValueList.getGridValueByIndex(v, val);
                  if (res && (val.mValue != ParamValueMissing || val.mValueString.length() > 0))
                  {
                    pidList.insert(std::pair<ulonglong,uint>(((ulonglong)pIdx << 32) + t,pp));
                  }
                  else
                  if (ai > 0)
                  {
                    res = gridQuery->mQueryParameterList[ai].mValueList[t]->mValueList.getGridValueByIndex(v, val);
                    if (res)
                      pidList.insert(std::pair<ulonglong,uint>(((ulonglong)pIdx << 32) + t,ai));
                  }


                  if (res && (val.mValue != ParamValueMissing || val.mValueString.length() > 0))
                  {
                    if (val.mValueString.length() > 0)
                    {
                      // The parameter value is a string
                      TS::TimedValue tsValue(queryTime, TS::Value(val.mValueString));
                      if (vLen == 1)
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
                      TS::TimedValue tsValue(queryTime, TS::Value(val.mValue));
                      if (vLen == 1)
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
                    if (vLen == 1)
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

                if (vLen > 1)
                {
                  T::GridValue val;
                  if (gridQuery->mQueryParameterList[pid].mValueList[0]->mValueList.getGridValueByIndex(v, val))
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
            else
            {
              // The query has returned no values for the parameter. This usually means that the
              // parameter is not a data parameter. It is most likely "a special parameter" that is
              // based on the given query location, time, level, etc.

              std::string paramValue;
              size_t tLen = gridQuery->mForecastTimeList.size();
              size_t t = 0;

              for (auto ft = gridQuery->mForecastTimeList.begin(); ft != gridQuery->mForecastTimeList.end(); ++ft)
              {
                auto dt = boost::posix_time::from_time_t(*ft);
                boost::local_time::local_date_time queryTime(dt, tz);

                if (xLen == 1)
                {
                  size_t size = gridQuery->mQueryParameterList[pid].mValueList[t]->mValueData.size();
                  uint step = 1;
                  if (size > 0)
                    step = 256 / size;

                  int width = Fmi::stoi(gridQuery->mAttributeList.getAttributeValue("grid.width"));
                  int height = Fmi::stoi(gridQuery->mAttributeList.getAttributeValue("grid.height"));
                  bool rotate = (bool) Fmi::stoi(gridQuery->mAttributeList.getAttributeValue("grid.original.reverseYDirection"));

                  if (width > 0 && height > 0)
                  {
                    double mp = 1;
                    int imageWidth = width * mp;
                    int imageHeight = height * mp;

                    ImagePaint imagePaint(imageWidth, imageHeight, 0xFFFFFFFF, 0x000000, 0xFFFF00, false, !rotate);

                    for (size_t s = 0; s < size; s++)
                    {
                      uint col = 0x000000;
                      // printf("--- colors %d = %lu
                      // %lu\n",p,s,gridQuery->mQueryParameterList[pid].mContourColors.size());
                      if (s < gridQuery->mQueryParameterList[pid].mContourColors.size())
                      {
                        col = gridQuery->mQueryParameterList[pid].mContourColors[s];
                      }
                      else
                      {
                        uint r = s * step;
                        col = (r << 16) + (r << 8) + r;
                      }

                      imagePaint.setDrawColor(col);
                      imagePaint.setFillColor(col);
                      imagePaint.paintWkb(mp, mp, 0, 0, gridQuery->mQueryParameterList[pid].mValueList[t]->mValueData[s]);
                    }

                    char filename[100];
                    auto cpid = pidList.find(((ulonglong)pp << 32) + t);
                    if (cpid != pidList.end())
                    {
                      sprintf(filename, "/tmp/timeseries_contours_%llu_%u_%lu.png", outputTime, cpid->second, t);
                      imagePaint.savePngImage(filename);
                      std::string image = fileToBase64(filename);
                      filenameList.emplace_back(std::string(filename));

                      if ((gridQuery->mQueryParameterList[pid].mFlags & QueryServer::QueryParameter::Flags::InvisibleParameter) == 0)
                      {
                        std::string s1 = R"(<img border="5" src="data:image/png;base64,)";

                        TS::TimedValue tsValue(queryTime, s1 + image + "\"/>");
                        tsForNonGridParam->emplace_back(tsValue);
                      }
                    }
                  }
                  else
                  {
                    if ((gridQuery->mQueryParameterList[pid].mFlags & QueryServer::QueryParameter::Flags::InvisibleParameter) == 0)
                    {
                      TS::TimedValue tsValue(queryTime, "Contour");
                      tsForNonGridParam->emplace_back(tsValue);
                    }
                  }
                }
                else if ((gridQuery->mQueryParameterList[pid].mFlags & QueryServer::QueryParameter::Flags::InvisibleParameter) != 0)
                {
                }
                else if (strcasecmp(gridQuery->mQueryParameterList[pid].mParam.c_str(), "y") == 0)
                {
                  uint len = coordinates.size();
                  std::ostringstream output;
                  if (coordinates.size() > 1)
                    output << "[";

                  if (masterquery.crs.empty() || masterquery.crs == "EPSG:4326")
                  {
                    for (uint i = 0; i < len; i++)
                    {
                      output << masterquery.valueformatter.format(coordinates[i].y(), gridQuery->mQueryParameterList[pid].mPrecision);
                      if ((i + 1) < len)
                        output << " ";
                    }
                  }
                  else
                  {
                    Fmi::CoordinateTransformation transformation("WGS84", masterquery.crs);

                    for (uint i = 0; i < len; i++)
                    {
                      double lon = coordinates[i].x();
                      double lat = coordinates[i].y();
                      transformation.transform(lon, lat);

                      output << masterquery.valueformatter.format(lat, gridQuery->mQueryParameterList[pid].mPrecision);
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
                  uint len = coordinates.size();
                  std::ostringstream output;
                  if (coordinates.size() > 1)
                    output << "[";

                  if (masterquery.crs.empty() || masterquery.crs == "EPSG:4326")
                  {
                    for (uint i = 0; i < len; i++)
                    {
                      output << masterquery.valueformatter.format(coordinates[i].x(), gridQuery->mQueryParameterList[pid].mPrecision);
                      if ((i + 1) < len)
                        output << " ";
                    }
                  }
                  else
                  {
                    Fmi::CoordinateTransformation transformation("WGS84", masterquery.crs);

                    for (uint i = 0; i < len; i++)
                    {
                      double lon = coordinates[i].x();
                      double lat = coordinates[i].y();
                      transformation.transform(lon, lat);

                      output << masterquery.valueformatter.format(lon, gridQuery->mQueryParameterList[pid].mPrecision);
                      if ((i + 1) < len)
                        output << " ";
                    }
                  }

                  if (coordinates.size() > 1)
                    output << "]";

                  TS::TimedValue tsValue(queryTime, TS::Value(output.str()));
                  tsForNonGridParam->emplace_back(tsValue);
                }
                else if (coordinates.size() > 1 && strcasecmp(gridQuery->mQueryParameterList[pid].mParam.c_str(), "lat") == 0)
                {
                  uint len = coordinates.size();
                  std::ostringstream output;
                  output << "[";
                  for (uint i = 0; i < len; i++)
                  {
                    output << masterquery.valueformatter.format(coordinates[i].y(), gridQuery->mQueryParameterList[pid].mPrecision);
                    if ((i + 1) < len)
                      output << " ";
                  }
                  output << "]";
                  TS::TimedValue tsValue(queryTime, TS::Value(output.str()));
                  tsForNonGridParam->emplace_back(tsValue);
                }
                else if (coordinates.size() > 1 && strcasecmp(gridQuery->mQueryParameterList[pid].mParam.c_str(), "lon") == 0)
                {
                  uint len = coordinates.size();
                  std::ostringstream output;
                  output << "[";
                  for (uint i = 0; i < len; i++)
                  {
                    output << masterquery.valueformatter.format(coordinates[i].x(), gridQuery->mQueryParameterList[pid].mPrecision);
                    if ((i + 1) < len)
                      output << " ";
                  }
                  output << "]";
                  TS::TimedValue tsValue(queryTime, TS::Value(output.str()));
                  tsForNonGridParam->emplace_back(tsValue);
                }
                else if (coordinates.size() > 1 && strcasecmp(gridQuery->mQueryParameterList[pid].mParam.c_str(), "latlon") == 0)
                {
                  uint len = coordinates.size();
                  std::ostringstream output;
                  output << "[";
                  for (uint i = 0; i < len; i++)
                  {
                    output << masterquery.valueformatter.format(coordinates[i].x(), gridQuery->mQueryParameterList[pid].mPrecision) << ",";
                    output << masterquery.valueformatter.format(coordinates[i].y(), gridQuery->mQueryParameterList[pid].mPrecision);
                    if ((i + 1) < len)
                      output << " ";
                  }
                  output << "]";
                  TS::TimedValue tsValue(queryTime, TS::Value(output.str()));
                  tsForNonGridParam->emplace_back(tsValue);
                }
                else if (coordinates.size() > 1 && strcasecmp(gridQuery->mQueryParameterList[pid].mParam.c_str(), "lonlat") == 0)
                {
                  uint len = coordinates.size();
                  std::ostringstream output;
                  output << "[";
                  for (uint i = 0; i < len; i++)
                  {
                    output << masterquery.valueformatter.format(coordinates[i].x(), gridQuery->mQueryParameterList[pid].mPrecision) << ",";
                    output << masterquery.valueformatter.format(coordinates[i].y(), gridQuery->mQueryParameterList[pid].mPrecision);
                    if ((i + 1) < len)
                      output << " ";
                  }
                  output << "]";
                  TS::TimedValue tsValue(queryTime, TS::Value(output.str()));
                  tsForNonGridParam->emplace_back(tsValue);
                }
                else if (additionalParameters.getParameterValueByLocation(gridQuery->mQueryParameterList[pid].mParam, tloc.tag, loc, country,
                    gridQuery->mQueryParameterList[pid].mPrecision, paramValue))
                {
                  TS::TimedValue tsValue(queryTime, paramValue);
                  tsForNonGridParam->emplace_back(tsValue);
                }
                else if (additionalParameters.getParameterValueByLocationAndTime(gridQuery->mQueryParameterList[pid].mParam, tloc.tag, loc, queryTime, tz,
                    gridQuery->mQueryParameterList[pid].mPrecision, paramValue))
                {
                  TS::TimedValue tsValue(queryTime, paramValue);
                  tsForNonGridParam->emplace_back(tsValue);
                }
                else if (gridQuery->mQueryParameterList[pid].mParam.substr(0, 3) == "@L-")
                {
                  auto opt_idx = Fmi::stoi_opt(gridQuery->mQueryParameterList[pid].mParam.substr(3));
                  if (opt_idx && *opt_idx >= 0 && *opt_idx < pLen)
                  {
                    auto cpid = pidList.find(((ulonglong)(*opt_idx) << 32) + t);
                    if (cpid != pidList.end())
                    {
                      TS::TimedValue tsValue(queryTime, C_INT(gridQuery->mQueryParameterList[cpid->second].mValueList[t]->mParameterLevel));
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
                    TS::TimedValue tsValue(queryTime, level);
                    tsForNonGridParam->emplace_back(tsValue);
                  }
                }
                else if (gridQuery->mQueryParameterList[pid].mParam == "level")
                {
                  int idx = 0;
                  int levelValue = level;
                  while (idx < pLen && levelValue <= 0)
                  {
                    if (gridQuery->mQueryParameterList[idx].mValueList[t]->mParameterLevel > 0)
                    {
                      levelValue = gridQuery->mQueryParameterList[idx].mValueList[t]->mParameterLevel;
                      if (levelValue < 0)
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
                else if (gridQuery->mQueryParameterList[pid].mParam.substr(0, 4) == "@LT-")
                {
                  auto opt_idx = Fmi::stoi_opt(gridQuery->mQueryParameterList[pid].mParam.substr(4));
                  if (opt_idx && *opt_idx >= 0 && *opt_idx < pLen)
                  {
                    auto cpid = pidList.find(((ulonglong)(*opt_idx) << 32) + t);
                    if (cpid != pidList.end())
                    {
                      TS::TimedValue tsValue(queryTime, static_cast<int>(gridQuery->mQueryParameterList[cpid->second].mValueList[t]->mParameterLevelId));
                      tsForNonGridParam->emplace_back(tsValue);
                    }
                    else
                    {
                      TS::TimedValue tsValue(queryTime, missing_value);
                      tsForNonGridParam->emplace_back(tsValue);
                    }
                  }
                }
                else if (gridQuery->mQueryParameterList[pid].mParam == "model" || gridQuery->mQueryParameterList[pid].mParam == "producer")
                {
                  int idx = 0;
                  while (idx < pLen)
                  {
                    if (gridQuery->mQueryParameterList[idx].mValueList[t]->mProducerId > 0)
                    {
                      T::ProducerInfo producer;
                      if (itsGridEngine->getProducerInfoById(gridQuery->mQueryParameterList[idx].mValueList[t]->mProducerId, producer))
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
                      std::vector < std::string > pnameList;
                      itsGridEngine->getProducerNameList(gridQuery->mProducerNameList[0], pnameList);
                      if (pnameList.size() > 0)
                        producer = pnameList[0];
                    }

                    TS::TimedValue tsValue(queryTime, producer);
                    tsForNonGridParam->emplace_back(tsValue);
                  }
                }
                else if (gridQuery->mQueryParameterList[pid].mParam.substr(0, 3) == "@P-")
                {
                  auto opt_idx = Fmi::stoi_opt(gridQuery->mQueryParameterList[pid].mParam.substr(3));
                  if (opt_idx && *opt_idx >= 0 && *opt_idx < pLen)
                  {
                    auto cpid = pidList.find(((ulonglong)(*opt_idx) << 32) + t);
                    if (cpid != pidList.end())
                    {
                      T::ProducerInfo producer;
                      if (itsGridEngine->getProducerInfoById(gridQuery->mQueryParameterList[cpid->second].mValueList[t]->mProducerId, producer))
                      {
                        TS::TimedValue tsValue(queryTime, producer.mName);
                        tsForNonGridParam->emplace_back(tsValue);
                      }
                      else
                      {
                        TS::TimedValue tsValue(queryTime, Fmi::to_string(gridQuery->mQueryParameterList[cpid->second].mValueList[t]->mProducerId));
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
                else if (gridQuery->mQueryParameterList[pid].mParam.substr(0, 3) == "@G-")
                {
                  auto opt_idx = Fmi::stoi_opt(gridQuery->mQueryParameterList[pid].mParam.substr(3));
                  if (opt_idx && *opt_idx >= 0 && *opt_idx < pLen)
                  {
                    auto cpid = pidList.find(((ulonglong)(*opt_idx) << 32) + t);
                    if (cpid != pidList.end())
                    {
                      T::GenerationInfo info;
                      bool res = itsGridEngine->getGenerationInfoById(gridQuery->mQueryParameterList[cpid->second].mValueList[t]->mGenerationId, info);
                      if (res)
                      {
                        TS::TimedValue tsValue(queryTime, info.mName);
                        tsForNonGridParam->emplace_back(tsValue);
                      }
                      else
                      {
                        TS::TimedValue tsValue(queryTime, Fmi::to_string(gridQuery->mQueryParameterList[cpid->second].mValueList[t]->mGenerationId));
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
                else if (gridQuery->mQueryParameterList[pid].mParam.substr(0, 4) == "@AT-")
                {
                  auto opt_idx = Fmi::stoi_opt(gridQuery->mQueryParameterList[pid].mParam.substr(4));
                  if (opt_idx && *opt_idx >= 0 && *opt_idx < pLen)
                  {
                    auto cpid = pidList.find(((ulonglong)(*opt_idx) << 32) + t);
                    if (cpid != pidList.end())
                    {
                      T::GenerationInfo info;
                      bool res = itsGridEngine->getGenerationInfoById(gridQuery->mQueryParameterList[cpid->second].mValueList[t]->mGenerationId, info);
                      if (res)
                      {
                        TS::TimedValue tsValue(queryTime, info.mAnalysisTime);
                        tsForNonGridParam->emplace_back(tsValue);
                      }
                      else
                      {
                        TS::TimedValue tsValue(queryTime, std::string("Unknown"));
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
                else if (gridQuery->mQueryParameterList[pid].mParam.substr(0, 4) == "@FT-")
                {
                  auto opt_idx = Fmi::stoi_opt(gridQuery->mQueryParameterList[pid].mParam.substr(4));
                  if (*opt_idx && *opt_idx >= 0 && *opt_idx < pLen)
                  {
                    auto cpid = pidList.find(((ulonglong)(*opt_idx) << 32) + t);
                    if (cpid != pidList.end())
                    {
                      TS::TimedValue tsValue(queryTime, static_cast<int>(gridQuery->mQueryParameterList[cpid->second].mValueList[t]->mForecastType));
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
                    TS::TimedValue tsValue(queryTime, level);
                    tsForNonGridParam->emplace_back(tsValue);
                  }
                }
                else if (gridQuery->mQueryParameterList[pid].mParam.substr(0, 4) == "@FN-")
                {
                  auto opt_idx = Fmi::stoi_opt(gridQuery->mQueryParameterList[pid].mParam.substr(4));
                  if (opt_idx && *opt_idx >= 0 && *opt_idx < pLen)
                  {
                    auto cpid = pidList.find(((ulonglong)(*opt_idx) << 32) + t);
                    if (cpid != pidList.end())
                    {
                      TS::TimedValue tsValue(queryTime, static_cast<int>(gridQuery->mQueryParameterList[cpid->second].mValueList[t]->mForecastNumber));
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
                    TS::TimedValue tsValue(queryTime, level);
                    tsForNonGridParam->emplace_back(tsValue);
                  }
                }
                else if (gridQuery->mQueryParameterList[pid].mParam == "origintime")
                {
                  int idx = 0;
                  while (idx < pLen)
                  {
                    if (gridQuery->mQueryParameterList[idx].mValueList[t]->mGenerationId > 0)
                    {
                      T::GenerationInfo info;
                      bool res = itsGridEngine->getGenerationInfoById(gridQuery->mQueryParameterList[idx].mValueList[t]->mGenerationId, info);
                      if (res)
                      {
                        // boost::posix_time::ptime origTime =
                        // toTimeStamp(info->mAnalysisTime);
                        boost::local_time::local_date_time origTime(toTimeStamp(info.mAnalysisTime), tz);

                        // boost::local_time::local_date_time origTime(oTime);
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
                else if (gridQuery->mQueryParameterList[pid].mParam == "modtime" || gridQuery->mQueryParameterList[pid].mParam == "mtime")
                {
                  int idx = 0;
                  while (idx < pLen)
                  {
                    if (gridQuery->mQueryParameterList[idx].mValueList[t]->mModificationTime > 0)
                    {
                      auto dt = boost::posix_time::from_time_t(gridQuery->mQueryParameterList[idx].mValueList[t]->mModificationTime);
                      boost::local_time::local_date_time modTime(dt, tz);
                      // boost::local_time::local_date_time
                      // modTime(toTimeStamp(gridQuery->mQueryParameterList[idx].mValueList[t]->mModificationTime),
                      // tz);
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
                else if (gridQuery->mQueryParameterList[pid].mParam.substr(0, 3) == "@I-")
                {
                  auto opt_idx = Fmi::stoi_opt(gridQuery->mQueryParameterList[pid].mParam.substr(3));
                  if (opt_idx && *opt_idx >= 0 && *opt_idx < pLen)
                  {
                    auto cpid = pidList.find(((ulonglong)(*opt_idx) << 32) + t);
                    if (cpid != pidList.end())
                    {
                      uint i = cpid->second;
                      std::string producerName;
                      T::ProducerInfo producer;
                      if (itsGridEngine->getProducerInfoById(gridQuery->mQueryParameterList[i].mValueList[t]->mProducerId, producer))
                        producerName = producer.mName;

                      char tmp[1000];

                      // gridQuery->mQueryParameterList[idx].mValueList[t]->mGenerationId;
                      // gridQuery->mQueryParameterList[idx].mValueList[t]->mGeometryId;

                      sprintf(tmp, "%s:%s:%u:%d:%d:%d:%d", gridQuery->mQueryParameterList[i].mValueList[t]->mParameterKey.c_str(), producerName.c_str(),
                          gridQuery->mQueryParameterList[i].mValueList[t]->mGeometryId, C_INT(gridQuery->mQueryParameterList[i].mValueList[t]->mParameterLevelId),
                          C_INT(gridQuery->mQueryParameterList[i].mValueList[t]->mParameterLevel), C_INT(gridQuery->mQueryParameterList[i].mValueList[t]->mForecastType),
                          C_INT(gridQuery->mQueryParameterList[i].mValueList[t]->mForecastNumber));

                      TS::TimedValue tsValue(queryTime, std::string(tmp));
                      tsForNonGridParam->emplace_back(tsValue);
                    }
                    else
                    {
                      TS::TimedValue tsValue(queryTime, missing_value);
                      tsForNonGridParam->emplace_back(tsValue);
                    }
                  }
                }
                else if ((gridQuery->mQueryParameterList[pid].mFlags & QueryServer::QueryParameter::Flags::NoReturnValues) != 0
                    && (gridQuery->mQueryParameterList[pid].mValueList[t]->mFlags & QueryServer::ParameterValues::Flags::DataAvailable) != 0)
                {
                  std::string producerName;
                  T::ProducerInfo producer;
                  if (itsGridEngine->getProducerInfoById(gridQuery->mQueryParameterList[pid].mValueList[t]->mProducerId, producer))
                    producerName = producer.mName;

                  // gridQuery->mQueryParameterList[pid].mValueList[t]->print(std::cout,0,0);

                  char tmp[1000];
                  sprintf(tmp, "%s:%s:%u:%d:%d:%d:%d %s flags:%d", gridQuery->mQueryParameterList[pid].mValueList[t]->mParameterKey.c_str(), producerName.c_str(),
                      gridQuery->mQueryParameterList[pid].mValueList[t]->mGeometryId, C_INT(gridQuery->mQueryParameterList[pid].mValueList[t]->mParameterLevelId),
                      C_INT(gridQuery->mQueryParameterList[pid].mValueList[t]->mParameterLevel), C_INT(gridQuery->mQueryParameterList[pid].mValueList[t]->mForecastType),
                      C_INT(gridQuery->mQueryParameterList[pid].mValueList[t]->mForecastNumber), gridQuery->mQueryParameterList[pid].mValueList[t]->mAnalysisTime.c_str(),
                      C_INT(gridQuery->mQueryParameterList[pid].mValueList[t]->mFlags));

                  TS::TimedValue tsValue(queryTime, std::string(tmp));
                  // tsForNonGridParam->emplace_back(tsValue);
                  tsForParameter->emplace_back(tsValue);
                }
                else if (gridQuery->mQueryParameterList[pid].mParam.substr(0, 4) == "@GM-")
                {
                  auto opt_idx = Fmi::stoi_opt(gridQuery->mQueryParameterList[pid].mParam.substr(4));
                  if (opt_idx && *opt_idx >= 0 && *opt_idx < pLen)
                  {
                    auto cpid = pidList.find(((ulonglong)(*opt_idx) << 32) + t);
                    if (cpid != pidList.end())
                    {
                      uint i = cpid->second;
                      TS::TimedValue tsValue(queryTime, Fmi::to_string(gridQuery->mQueryParameterList[i].mValueList[t]->mGeometryId));
                      tsForNonGridParam->emplace_back(tsValue);
                    }
                  }
                }
                else if (gridQuery->mQueryParameterList[pid].mParam.substr(0, 7) == "@MERGE-")
                {
                  std::vector < std::string > partList;
                  std::vector < std::string > fileList;

                  splitString(gridQuery->mQueryParameterList[pid].mParam, '-', partList);

                  uint partCount = partList.size();

                  char filename[100];
                  for (uint pl = 1; pl < partCount; pl++)
                  {
                    int idx = toInt32(partList[pl]);
                    int i = pidList[idx];
                    sprintf(filename, "/tmp/timeseries_contours_%llu_%d_%lu.png", outputTime, i, t);
                    if (getFileSize(filename) > 0)
                      fileList.emplace_back(std::string(filename));
                  }

                  if (fileList.size() > 0)
                  {
                    sprintf(filename, "/tmp/timeseries_contours_%llu_%d_%lu.png", outputTime, pid, t);
                    mergePngFiles(filename, fileList);

                    std::string image = fileToBase64(filename);
                    filenameList.emplace_back(std::string(filename));

                    std::string s1 = R"(<img border="5" src="data:image/png;base64,)";

                    TS::TimedValue tsValue(queryTime, s1 + image + "\"/>");
                    tsForNonGridParam->emplace_back(tsValue);
                  }
                  else
                  {
                    TS::TimedValue tsValue(queryTime, "Contour");
                    tsForNonGridParam->emplace_back(tsValue);
                  }
                }
                else if (rLen > tLen)
                {
                  // It seems that there are more values available for each time step than expected.
                  // This usually happens if the requested parameter definition does not contain
                  // enough information in order to identify an unique parameter. For example, if
                  // the parameter definition does not contain level information then the result is
                  // that we get values from several levels.

                  char tmp[10000];
                  std::set < std::string > pList;

                  for (uint r = 0; r < rLen; r++)
                  {
                    if (gridQuery->mQueryParameterList[pid].mValueList[r]->mForecastTimeUTC == *ft)
                    {
                      std::string producerName;
                      T::ProducerInfo producer;
                      if (itsGridEngine->getProducerInfoById(gridQuery->mQueryParameterList[pid].mValueList[r]->mProducerId, producer))
                        producerName = producer.mName;

                      sprintf(tmp, "%s:%d:%d:%d:%d:%s", gridQuery->mQueryParameterList[pid].mValueList[r]->mParameterKey.c_str(),
                          C_INT(gridQuery->mQueryParameterList[pid].mValueList[r]->mParameterLevelId), C_INT(gridQuery->mQueryParameterList[pid].mValueList[r]->mParameterLevel),
                          C_INT(gridQuery->mQueryParameterList[pid].mValueList[r]->mForecastType), C_INT(gridQuery->mQueryParameterList[pid].mValueList[r]->mForecastNumber),
                          producerName.c_str());

                      if (pList.find(std::string(tmp)) == pList.end())
                        pList.insert(std::string(tmp));
                    }
                  }
                  char* pp = tmp;
                  pp += sprintf(pp, "### MULTI-MATCH ### ");
                  for (auto p = pList.begin(); p != pList.end(); ++p)
                    pp += sprintf(pp, "%s ", p->c_str());

                  TS::TimedValue tsValue(queryTime, std::string(tmp));
                  tsForNonGridParam->emplace_back(tsValue);
                }
                else
                {
                  TS::TimedValue tsValue(queryTime, missing_value);
                  tsForParameter->emplace_back(tsValue);
                }
                t++;
              }

              if (tsForNonGridParam->size() > 0)
              {
                aggregatedData.emplace_back(erase_redundant_timesteps(TS::aggregate(tsForNonGridParam, paramFuncs[pIdx].functions), aggregationTimes));
              }
            }

            if (tsForParameter->size() > 0)
            {
              aggregatedData.emplace_back(erase_redundant_timesteps(TS::aggregate(tsForParameter, paramFuncs[pIdx].functions), aggregationTimes));
            }

            if (tsForGroup->size() > 0)
            {
              aggregatedData.emplace_back(erase_redundant_timesteps(TS::aggregate(tsForGroup, paramFuncs[pIdx].functions), aggregationTimes));
            }

            UtilityFunctions::store_data(aggregatedData, masterquery, outputData);
            pIdx++;
          }
        }

        for (const auto& filename : filenameList)
          remove(filename.c_str());
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void GridInterface::erase_redundant_timesteps(TS::TimeSeries& ts, std::set<boost::local_time::local_date_time>& aggregationTimes)
{
  FUNCTION_TRACE
  try
  {
    TS::TimeSeries no_redundant(ts.getLocalTimePool());
    no_redundant.reserve(ts.size());
    std::set < boost::local_time::local_date_time > newTimes;

    for (const auto& tv : ts)
    {
      if (aggregationTimes.find(tv.time) == aggregationTimes.end() && newTimes.find(tv.time) == newTimes.end())
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

TS::TimeSeriesPtr GridInterface::erase_redundant_timesteps(TS::TimeSeriesPtr ts, std::set<boost::local_time::local_date_time>& aggregationTimes)
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

TS::TimeSeriesVectorPtr GridInterface::erase_redundant_timesteps(TS::TimeSeriesVectorPtr tsv, std::set<boost::local_time::local_date_time>& aggregationTimes)
{
  FUNCTION_TRACE
  try
  {
    for (auto& ts : *tsv)
      erase_redundant_timesteps(ts, aggregationTimes);

    return tsv;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

TS::TimeSeriesGroupPtr GridInterface::erase_redundant_timesteps(TS::TimeSeriesGroupPtr tsg, std::set<boost::local_time::local_date_time>& aggregationTimes)
{
  FUNCTION_TRACE
  try
  {
    for (size_t i = 0; i < tsg->size(); i++)
      erase_redundant_timesteps(tsg->at(i).timeseries, aggregationTimes);

    return tsg;
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
