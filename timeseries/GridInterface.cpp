// ======================================================================
/*!
 * \brief SmartMet TimeSeries plugin implementation
 */
// ======================================================================
#include "GridInterface.h"
#include "DataFunctions.h"
#include "LocationTools.h"

#include <spine/TimeSeriesGeneratorCache.h>

#include <engines/grid/Engine.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/ImageFunctions.h>
#include <grid-files/common/ImagePaint.h>
#include <grid-files/identification/GridDef.h>

#include <grid-files/common/ShowFunction.h>
#include <macgyver/Astronomy.h>
#include <macgyver/CharsetTools.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeFormatter.h>
#include <macgyver/TimeParser.h>
#include <macgyver/TimeZoneFactory.h>

#define FUNCTION_TRACE FUNCTION_TRACE_OFF

using namespace std;
using namespace boost::posix_time;
using namespace boost::gregorian;
using namespace boost::local_time;
using boost::numeric_cast;
using boost::numeric::bad_numeric_cast;
using boost::numeric::negative_overflow;
using boost::numeric::positive_overflow;

namespace ts = SmartMet::Spine::TimeSeries;

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
GridInterface::GridInterface(Engine::Grid::Engine* engine, const Fmi::TimeZones& timezones)
    : itsGridEngine(engine), itsTimezones(timezones)
{
  FUNCTION_TRACE
  try
  {
    itsProducerList_updateTime = 0;
    itsProducerInfoList_updateTime = 0;
    itsGenerationInfoList_updateTime = 0;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", nullptr);
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
    SmartMet::Spine::Exception exception(BCP, "Destructor failed", nullptr);
    exception.printError();
  }
}

bool GridInterface::isGridProducer(const std::string& producer)
{
  FUNCTION_TRACE
  try
  {
    if ((time(nullptr) - itsProducerList_updateTime) > 60)
    {
      itsProducerList_updateTime = time(nullptr);
      itsGridEngine->getProducerList(itsProducerList);
    }

    std::vector<std::string> nameList;
    itsGridEngine->getProducerNameList(producer, nameList);
    for (auto it = nameList.begin(); it != nameList.end(); ++it)
    {
      for (auto itm = itsProducerList.begin(); itm != itsProducerList.end(); ++itm)
      {
        if (strcasecmp(it->c_str(), itm->c_str()) == 0)
          return true;
      }
    }

    return false;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", nullptr);
  }
}

bool GridInterface::containsGridProducer(const Query& masterquery)
{
  try
  {
    for (auto areaproducers = masterquery.timeproducers.begin();
         areaproducers != masterquery.timeproducers.end();
         ++areaproducers)
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
    throw Spine::Exception(BCP, "Operation failed!", nullptr);
  }
}

bool GridInterface::containsParameterWithGridProducer(const Query& masterquery)
{
  try
  {
    const char removeChar[] = {'(', ')', '{', '}', '[', ']', ';', ' ', '\0'};

    // BOOST_FOREACH (const Spine::ParameterAndFunctions&
    // paramfunc,masterquery.poptions.parameterFunctions())
    for (auto paramfunc = masterquery.poptions.parameterFunctions().begin();
         paramfunc != masterquery.poptions.parameterFunctions().end();
         ++paramfunc)
    {
      Spine::Parameter param = paramfunc->parameter;

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

      std::vector<std::string> partList;

      splitString(param.name(), ':', partList);
      for (auto it = partList.begin(); it != partList.end(); ++it)
      {
        if (isGridProducer(*it))
        {
          return true;
        }
      }
    }
    return false;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", nullptr);
  }
}

T::ParamLevelId GridInterface::getLevelId(const char* producerName, const Query& masterquery)
{
  try
  {
    T::ParamLevelId cnt[20] = {0};
    T::ProducerInfo* producerInfo = itsProducerInfoList.getProducerInfoByName(producerName);
    if (producerInfo != nullptr)
    {
      for (auto level = masterquery.levels.rbegin(); level != masterquery.levels.rend(); ++level)
      {
        T::ParamLevelId levelId =
            itsGridEngine->getFmiParameterLevelId(producerInfo->mProducerId, *level);
        if (levelId == 0)
        {
          // Did not find a level id. Maybe the level value is given in hPa. Let's try with Pa.
          levelId =
              itsGridEngine->getFmiParameterLevelId(producerInfo->mProducerId, C_INT(*level * 100));
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
    throw Spine::Exception(BCP, "Operation failed!", nullptr);
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

    std::shared_ptr<SmartMet::ContentServer::ServiceInterface> contentServer =
        itsGridEngine->getContentServer_sptr();
    for (auto it = areaproducers.begin(); it != areaproducers.end(); ++it)
    {
      std::vector<std::string> pnameList;
      itsGridEngine->getProducerNameList(*it, pnameList);

      for (auto pname = pnameList.begin(); pname != pnameList.end(); ++pname)
      {
        T::ProducerInfo* producerInfo = itsProducerInfoList.getProducerInfoByName(*pname);
        if (producerInfo != nullptr)
        {
          std::set<std::string> contentTimeList;
          contentServer->getContentTimeListByProducerId(
              0, producerInfo->mProducerId, contentTimeList);
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
    throw Spine::Exception(BCP, "Operation failed!", nullptr);
  }
}

void GridInterface::insertFileQueries(QueryServer::Query& query,
                                      QueryServer::QueryStreamer_sptr queryStreamer)
{
  try
  {
    // Adding queries into the QueryStreamer. These queries fetch actual GRIB1/GRIB2 files accoding
    // to the file attribute definitions (query.mAttributeList). The queryStreamer sends these
    // queries to the queryServer and returns the results (= GRIB files) to the HTTP client.

    for (auto param = query.mQueryParameterList.begin(); param != query.mQueryParameterList.end();
         ++param)
    {
      for (auto val = param->mValueList.begin(); val != param->mValueList.end(); ++val)
      {
        QueryServer::Query newQuery;

        newQuery.mSearchType = QueryServer::Query::SearchType::TimeSteps;
        // newQuery.mProducerNameList;
        newQuery.mForecastTimeList.insert(val->mForecastTime);
        newQuery.mAttributeList = query.mAttributeList;
        // newQuery.mCoordinateType;
        // newQuery.mAreaCoordinates;
        // newQuery.mRadius;
        newQuery.mTimezone = query.mTimezone;
        newQuery.mStartTime = val->mForecastTime;
        newQuery.mEndTime = val->mForecastTime;
        newQuery.mTimesteps = 1;
        newQuery.mTimestepSizeInMinutes = 60;
        newQuery.mAnalysisTime = val->mAnalysisTime;
        if (val->mGeometryId > 0)
          newQuery.mGeometryIdList.insert(val->mGeometryId);

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
        newParam.mParameterKeyType = val->mParameterKeyType;
        newParam.mParameterKey = val->mParameterKey;
        newParam.mProducerName = param->mProducerName;
        newParam.mGeometryId = val->mGeometryId;
        newParam.mParameterLevelIdType = val->mParameterLevelIdType;
        newParam.mParameterLevelId = val->mParameterLevelId;
        newParam.mParameterLevel = val->mParameterLevel;
        newParam.mForecastType = val->mForecastType;
        newParam.mForecastNumber = val->mForecastNumber;
        newParam.mAreaInterpolationMethod = param->mAreaInterpolationMethod;
        newParam.mTimeInterpolationMethod = param->mTimeInterpolationMethod;
        newParam.mLevelInterpolationMethod = param->mLevelInterpolationMethod;
        // newParam.mContourLowValues = param->mContourLowValues;
        // newParam.mContourHighValues = param->mContourHighValues;
        // newParam.mContourColors = param->mContourColors;
        newParam.mProducerId = val->mProducerId;
        newParam.mGenerationFlags = val->mGenerationFlags;
        newParam.mPrecision = param->mPrecision;
        newParam.mTemporary = param->mTemporary;
        newParam.mFunction = param->mFunction;
        newParam.mFunctionParams = param->mFunctionParams;
        newParam.mTimestepsBefore = param->mTimestepsBefore;
        newParam.mTimestepsAfter = param->mTimestepsAfter;
        newParam.mTimestepSizeInMinutes = param->mTimestepSizeInMinutes;
        newParam.mFlags = param->mFlags;

        newQuery.mQueryParameterList.push_back(newParam);

        queryStreamer->addQuery(newQuery);
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", nullptr);
  }
}

void GridInterface::prepareGridQuery(QueryServer::Query& gridQuery,
                                     AdditionalParameters& additionalParameters,
                                     const Query& masterquery,
                                     uint mode,
                                     int origLevelId,
                                     double origLevel,
                                     const AreaProducers& areaproducers,
                                     const Spine::TaggedLocation& tloc,
                                     const Spine::LocationPtr loc,
                                     std::vector<std::vector<T::Coordinate>>& polygonPath)
{
  FUNCTION_TRACE
  try
  {
    const char* buildIdParameters[] = {
        "origintime", "modtime", "mtime", "level", "lat", "lon", "latlon", "lonlat", "@", nullptr};
    int level = C_INT(origLevel);

    bool info = false;
    if (strcasecmp(masterquery.format.c_str(), "INFO") == 0)
      info = true;

    std::shared_ptr<SmartMet::ContentServer::ServiceInterface> contentServer =
        itsGridEngine->getContentServer_sptr();

    gridQuery.mLanguage = masterquery.language;

    std::string startTime = Fmi::to_iso_string(masterquery.toptions.startTime);
    std::string endTime = Fmi::to_iso_string(masterquery.toptions.endTime);

    bool startTimeUTC = masterquery.toptions.startTimeUTC;
    bool endTimeUTC = masterquery.toptions.endTimeUTC;

    gridQuery.mAttributeList = masterquery.attributeList;

    T::Attribute* geomId = gridQuery.mAttributeList.getAttribute("grid.geometryId");
    T::Attribute* coordinateType = gridQuery.mAttributeList.getAttribute("contour.coordinateType");

    if (geomId != nullptr && coordinateType == nullptr)
    {
      gridQuery.mAttributeList.addAttribute(
          "contour.coordinateType", std::to_string(T::CoordinateTypeValue::GRID_COORDINATES));
    }

    gridQuery.mStartTime = startTime;
    gridQuery.mEndTime = endTime;

    if (!std::isnan(loc->dem))
      gridQuery.mAttributeList.setAttribute("dem", std::to_string(loc->dem));

    gridQuery.mAttributeList.setAttribute("coverType", std::to_string(loc->covertype));

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
      daylightSavingTime = Fmi::to_iso_string(tz->dst_local_end_time(atoi(year.c_str())));

    // std::cout << "DAYLIGHT : " << daylightSavingTime << "\n";

    if (masterquery.toptions.mode == Spine::TimeSeriesGeneratorOptions::TimeSteps ||
        masterquery.toptions.mode == Spine::TimeSeriesGeneratorOptions::FixedTimes)
    {
      std::string startT = startTime;
      if (startTimeUTC)
      {
        boost::local_time::local_date_time localTime(boost::posix_time::from_iso_string(startTime),
                                                     tz);
        startT = Fmi::to_iso_string(localTime.local_time());
      }

      startTimeUTC = false;
      gridQuery.mStartTime = startTime.substr(0, 9) + "000000";

      while (gridQuery.mStartTime < startT)
      {
        auto ptime = boost::posix_time::from_iso_string(gridQuery.mStartTime);
        ptime = ptime + boost::posix_time::seconds(step);
        gridQuery.mStartTime = Fmi::to_iso_string(ptime);
      }

      gridQuery.mEndTime = endTime.substr(0, 11) + "0000";
    }

    if (gridQuery.mStartTime <= daylightSavingTime && gridQuery.mEndTime >= daylightSavingTime)
    {
      daylightSavingActive = true;
      if (masterquery.toptions.timeSteps && *masterquery.toptions.timeSteps != 0)
      {
        // Adding one hour to the end time because of the daylight saving.

        auto ptime = boost::posix_time::from_iso_string(gridQuery.mEndTime);
        ptime = ptime + boost::posix_time::minutes(60);
        gridQuery.mEndTime = Fmi::to_iso_string(ptime);
      }
    }

    // Converting the start time and the end time to the UTC time.

    if (!startTimeUTC)
    {
      gridQuery.mStartTime = localTimeToUtc(gridQuery.mStartTime, tz);
    }

    if (!endTimeUTC)
    {
      gridQuery.mEndTime = localTimeToUtc(gridQuery.mEndTime, tz);
    }

    std::string latestTime = Fmi::to_iso_string(masterquery.latestTimestep);

    if (masterquery.toptions.startTimeData || masterquery.toptions.endTimeData ||
        masterquery.toptions.mode == Spine::TimeSeriesGeneratorOptions::DataTimes ||
        masterquery.toptions.mode == Spine::TimeSeriesGeneratorOptions::GraphTimes)
    {
      if (masterquery.toptions.startTimeData)
      {
        gridQuery.mFlags = gridQuery.mFlags | QueryServer::Query::Flags::StartTimeFromData;
        gridQuery.mStartTime = "19000101T000000";
      }

      if (masterquery.toptions.endTimeData)
      {
        gridQuery.mFlags = gridQuery.mFlags | QueryServer::Query::Flags::EndTimeFromData;
        gridQuery.mEndTime = "30000101T000000";
      }

      if (masterquery.toptions.mode == Spine::TimeSeriesGeneratorOptions::TimeSteps)
      {
        gridQuery.mTimesteps = 0;
        gridQuery.mTimestepSizeInMinutes = step / 60;
      }

      if (latestTime != startTime)
      {
        gridQuery.mStartTime = localTimeToUtc(latestTime, tz);
        gridQuery.mFlags = gridQuery.mFlags | QueryServer::Query::Flags::StartTimeNotIncluded;
      }

      gridQuery.mSearchType = QueryServer::Query::SearchType::TimeRange;

      if (masterquery.toptions.timeSteps && *masterquery.toptions.timeSteps > 0)
      {
        gridQuery.mMaxParameterValues = *masterquery.toptions.timeSteps;
        if (daylightSavingActive)
          gridQuery.mMaxParameterValues++;

        if (gridQuery.mStartTime == gridQuery.mEndTime)
          gridQuery.mEndTime = "30000101T000000";
      }
    }
    else
    {
      auto s = boost::posix_time::from_iso_string(gridQuery.mStartTime);
      auto e = boost::posix_time::from_iso_string(gridQuery.mEndTime);

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

        if (latestTime != startTime && ss < latestTime)
          additionOk = false;

        if (masterquery.toptions.timeList.size() > 0)
        {
          uint idx = atoi(ss.substr(9, 4).c_str());
          if (masterquery.toptions.timeList.find(idx) == masterquery.toptions.timeList.end())
            additionOk = false;
        }

        if (additionOk)
        {
          stepCount++;
          gridQuery.mForecastTimeList.insert(str);

          if (masterquery.toptions.timeSteps && stepCount == steps)
            s = e;
        }
        s = s + boost::posix_time::seconds(step);
      }
    }

    gridQuery.mRadius = loc->radius;

    int geometryId = -1;
    int levelId = origLevelId;

    // Producers
    for (auto it = areaproducers.begin(); it != areaproducers.end(); ++it)
    {
      // std::cout << "PRODUCER " << *it << "\n";

      std::string producerName = *it;
      producerName = itsGridEngine->getProducerAlias(producerName, levelId);

      Engine::Grid::ParameterDetails_vec parameters;
      itsGridEngine->getParameterDetails(producerName, parameters);

      size_t len = parameters.size();
      for (size_t t = 0; t < len; t++)
      {
        // std::cout << "** PRODUCER " << *pname << "\n";
        if (masterquery.leveltype.empty() && levelId < 0 && parameters[t].mLevelId > "")
          levelId = toInt32(parameters[t].mLevelId);

        if (parameters[t].mGeometryId > "")
          geometryId = toInt32(parameters[t].mGeometryId);

        gridQuery.mProducerNameList.push_back(parameters[t].mProducerName);
      }
    }

    // Generations: Enabling all generations
    gridQuery.mGenerationFlags = 0;

    if (masterquery.origintime)
    {
      if (masterquery.origintime == boost::posix_time::ptime(boost::date_time::pos_infin))
      {
        // Generation: latest, newest
        gridQuery.mGenerationFlags = 1;
      }
      else if (masterquery.origintime == boost::posix_time::ptime(boost::date_time::neg_infin))
      {
        // Generation: oldest
        gridQuery.mGenerationFlags = 1;
        gridQuery.mFlags = gridQuery.mFlags | QueryServer::Query::Flags::ReverseGenerationFlags;
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

    // BOOST_FOREACH (const Spine::ParameterAndFunctions& paramfunc,
    // masterquery.poptions.parameterFunctions())
    for (auto paramfunc = masterquery.poptions.parameterFunctions().begin();
         paramfunc != masterquery.poptions.parameterFunctions().end();
         ++paramfunc)
    {
      Spine::Parameter param = paramfunc->parameter;

      QueryServer::QueryParameter qParam;

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

      // std::cout << "PARAM [" << param.name() << "][" << param.alias() << "]\n";
      std::string tmp = param.name();

      std::vector<std::string> partList;

      splitString(tmp, ':', partList);
      if (partList.size() > 2 && (partList[0] == "ISOBANDS" || partList[0] == "ISOLINES"))
      {
        std::vector<std::string> list;
        splitString(partList[1], ';', list);
        size_t len = list.size();
        if (len > 0)
        {
          if ((len % 2) != 0)
          {
            list.push_back("FFFFFFFF");
            len++;
          }

          if (partList[0] == "ISOBANDS")
          {
            for (size_t a = 0; a < (len - 2); a = a + 2)
            {
              qParam.mContourLowValues.push_back(toDouble(list[a].c_str()));
              qParam.mContourHighValues.push_back(toDouble(list[a + 2].c_str()));
              qParam.mContourColors.push_back(strtoll(list[a + 1].c_str(), nullptr, 16));
            }
            qParam.mType = QueryServer::QueryParameter::Type::Isoband;
          }
          else
          {
            for (size_t a = 0; a < len; a = a + 2)
            {
              qParam.mContourLowValues.push_back(toDouble(list[a].c_str()));
              qParam.mContourColors.push_back(strtoll(list[a + 1].c_str(), nullptr, 16));
            }
            qParam.mType = QueryServer::QueryParameter::Type::Isoline;
          }
        }
        qParam.mLocationType = QueryServer::QueryParameter::LocationType::Geometry;

        const char* p = tmp.c_str() + partList[0].size() + partList[1].size() + 2;
        qParam.mParam = p;
      }
      else
      {
        qParam.mParam = param.name();
      }

      qParam.mOrigParam = param.name();

      if (qParam.mParam.find('@') != std::string::npos)
      {
        qParam.mParam = param.alias();
      }

      qParam.mSymbolicName = qParam.mParam;
      qParam.mParameterKey = qParam.mParam;

      int qGeometryId = geometryId;
      int qLevelId = levelId;
      int qLevel = level;
      int qForecastType = -1;
      int qForecastNumber = -1;

      for (auto it = areaproducers.begin(); it != areaproducers.end(); ++it)
      {
        std::string producerName = *it;
        producerName = itsGridEngine->getProducerAlias(producerName, levelId);

        Engine::Grid::ParameterDetails_vec parameters;
        std::string key = producerName + ";" + qParam.mParam;

        itsGridEngine->getParameterDetails(producerName, qParam.mParam, parameters);

        size_t len = parameters.size();
        if (len > 0 && strcasecmp(parameters[0].mProducerName.c_str(), key.c_str()) != 0)
        {
          for (size_t t = 0; t < len; t++)
          {
            if (parameters[t].mLevel > "")
              qLevel = toInt32(parameters[t].mLevel);

            if (parameters[t].mLevelId > "")
              qLevelId = toInt32(parameters[t].mLevelId);

            if (parameters[t].mForecastType > "")
              qForecastType = toInt32(parameters[t].mForecastType);

            if (parameters[t].mForecastNumber > "")
              qForecastNumber = toInt32(parameters[t].mForecastNumber);

            if (parameters[t].mGeometryId > "")
            {
              qParam.mProducerName = parameters[t].mProducerName;
              qGeometryId = toInt32(parameters[t].mGeometryId);
            }
          }
        }
      }

      if (qLevel < 0)
        qLevel = origLevel;

      // qParam.mParameterLevel = qLevel;
      qParam.mGeometryId = qGeometryId;

      if (qLevelId >= 0)
      {
        qParam.mParameterLevelId = qLevelId;
      }
      else if (!masterquery.leveltype.empty())
      {
        if (masterquery.leveltype == "pressure")
          qParam.mParameterLevelId = 2;
        else if (strcasecmp(masterquery.leveltype.c_str(), "model") == 0)
          qParam.mParameterLevelId = 3;
        else if (isdigit(*(masterquery.leveltype.c_str())))
          qParam.mParameterLevelId = toInt32(masterquery.leveltype);
      }

      if (qForecastType != -1)
        qParam.mForecastType = qForecastType;

      if (qForecastNumber != -1)
        qParam.mForecastNumber = qForecastNumber;

      switch (mode)
      {
        case 0:
          qParam.mParameterLevel = C_INT(qLevel);
          if (qParam.mParameterLevelId == 2)
          {
            // Grib uses Pa and querydata hPa, so we have to convert the value.
            qParam.mParameterLevel = C_INT(qLevel * 100);
            qParam.mFlags = qParam.mFlags | QueryServer::QueryParameter::Flags::PressureLevelInterpolation;
          }
          break;

        case 1:
          qParam.mFlags = qParam.mFlags | QueryServer::QueryParameter::Flags::PressureLevelInterpolation;
          qParam.mParameterLevelId = 2;
          // Grib uses Pa and querydata hPa, so we have to convert the value.
          qParam.mParameterLevel = C_INT(qLevel * 100);
          break;

        case 2:
          qParam.mParameterLevelId = 0;
          qParam.mParameterLevel = C_INT(qLevel);
          qParam.mFlags = qParam.mFlags | QueryServer::QueryParameter::Flags::HeightLevelInterpolation;
          break;
      }

      qParam.mPrecision = masterquery.precisions[idx];

      if (masterquery.maxAggregationIntervals.find(param.name()) !=
          masterquery.maxAggregationIntervals.end())
      {
        unsigned int aggregationIntervalBehind =
            masterquery.maxAggregationIntervals.at(param.name()).behind;
        unsigned int aggregationIntervalAhead =
            masterquery.maxAggregationIntervals.at(param.name()).ahead;

        if (aggregationIntervalBehind > 0 || aggregationIntervalAhead > 0)
        {
          qParam.mTimestepsBefore = aggregationIntervalBehind / 60;
          qParam.mTimestepsAfter = aggregationIntervalAhead / 60;
          qParam.mTimestepSizeInMinutes = 60;
        }
      }

      int c = 0;
      while (buildIdParameters[c] != nullptr)
      {
        if (strncasecmp(buildIdParameters[c],
                        qParam.mSymbolicName.c_str(),
                        strlen(buildIdParameters[c])) == 0)
          qParam.mParameterKeyType = T::ParamKeyTypeValue::BUILD_IN;
        c++;
      }

      if (additionalParameters.isAdditionalParameter(qParam.mSymbolicName.c_str()))
        qParam.mParameterKeyType = T::ParamKeyTypeValue::BUILD_IN;

      if (atoi(qParam.mParam.c_str()) > 0)
        qParam.mParameterKeyType = T::ParamKeyTypeValue::NEWBASE_ID;

      gridQuery.mQueryParameterList.push_back(qParam);
      idx++;
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", nullptr);
  }
}

int GridInterface::getParameterIndex(QueryServer::Query& gridQuery, std::string param)
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
    throw Spine::Exception(BCP, "Operation failed!", nullptr);
  }
}

void GridInterface::processGridQuery(const State& state,
                                     Query& masterquery,
                                     OutputData& outputData,
                                     QueryServer::QueryStreamer_sptr queryStreamer,
                                     const AreaProducers& areaproducers,
                                     const ProducerDataPeriod& producerDataPeriod,
                                     const Spine::TaggedLocation& tloc,
                                     const Spine::LocationPtr loc,
                                     std::string country,
                                     std::vector<std::vector<T::Coordinate>>& polygonPath)
{
  FUNCTION_TRACE
  try
  {
    std::shared_ptr<SmartMet::ContentServer::ServiceInterface> contentServer =
        itsGridEngine->getContentServer_sptr();

    Spine::TimeSeries::Value missing_value = Spine::TimeSeries::None();

    if (itsProducerInfoList.getLength() == 0)
    {
      contentServer->getProducerInfoList(0, itsProducerInfoList);
      itsProducerInfoList_updateTime = time(nullptr);
    }

    if (itsGenerationInfoList.getLength() == 0)
    {
      contentServer->getGenerationInfoList(0, itsGenerationInfoList);
      itsGenerationInfoList_updateTime = time(nullptr);
    }

    auto latestTimestep = masterquery.latestTimestep;

    std::string timezoneName = loc->timezone;
    boost::local_time::time_zone_ptr localtz = itsTimezones.time_zone_from_string(loc->timezone);
    boost::local_time::time_zone_ptr tz = localtz;

    if (masterquery.timezone != "localtime")
    {
      timezoneName = masterquery.timezone;
      tz = itsTimezones.time_zone_from_string(timezoneName);
    }

    AdditionalParameters additionalParameters(itsTimezones,
                                              masterquery.outlocale,
                                              *masterquery.timeformatter,
                                              masterquery.valueformatter);

    const SmartMet::Spine::OptionParsers::ParameterFunctionList& paramFuncs =
        masterquery.poptions.parameterFunctions();

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
            if (areaproducers.size() == 1 && masterquery.pressures.empty() &&
                masterquery.heights.empty())
            {
              std::string producerName = *areaproducers.begin();

              if (qLevelId == 2)
              {
                itsGridEngine->getProducerParameterLevelList(producerName, 2, 0.01, tmpLevels);
                for (auto lev = tmpLevels.rbegin(); lev != tmpLevels.rend(); ++lev)
                  levels.push_back(*lev);
              }
              else if (qLevelId == 3)
              {
                itsGridEngine->getProducerParameterLevelList(producerName, 3, 1, tmpLevels);
                for (auto lev = tmpLevels.begin(); lev != tmpLevels.end(); ++lev)
                  levels.push_back(*lev);
              }
              else if (masterquery.leveltype.empty())
              {
                std::set<T::ParamLevelId> levelIdList;
                itsGridEngine->getProducerParameterLevelIdList(producerName, levelIdList);
                if (levelIdList.size() == 1)
                {
                  // The producer supports only one level type

                  auto levelId = *levelIdList.begin();
                  switch (levelId)
                  {
                    case 2:  // Pressure level
                      itsGridEngine->getProducerParameterLevelList(
                          producerName, 2, 0.01, tmpLevels);
                      for (auto lev = tmpLevels.rbegin(); lev != tmpLevels.rend(); ++lev)
                        levels.push_back(*lev);
                      break;

                    case 3:  // model
                      itsGridEngine->getProducerParameterLevelList(producerName, 3, 1, tmpLevels);
                      for (auto lev = tmpLevels.begin(); lev != tmpLevels.end(); ++lev)
                        levels.push_back(*lev);
                      break;
                  }
                }
              }
            }

            if (tmpLevels.empty() && masterquery.pressures.empty() && masterquery.heights.empty())
              levels.push_back(-1);
          }
          else
          {
            if (qLevelId == 2)
            {
              for (auto level = masterquery.levels.rbegin(); level != masterquery.levels.rend();
                   ++level)
                levels.push_back((double)(*level));
            }
            else
            {
              for (auto level = masterquery.levels.begin(); level != masterquery.levels.end();
                   ++level)
                levels.push_back((double)(*level));
            }
          }
          break;

        case 1:
          aLevelId = 2;
          for (auto level = masterquery.pressures.begin(); level != masterquery.pressures.end();
               ++level)
            levels.push_back((double)(*level));
          break;

        case 2:
          aLevelId = 3;
          for (auto level = masterquery.heights.begin(); level != masterquery.heights.end();
               ++level)
            levels.push_back((double)(*level));
          break;
      }

      for (auto level = levels.begin(); level != levels.end(); ++level)
      {
        //        std::cout << "  ------- LEVEL " << *level << "\n";
        masterquery.latestTimestep = latestTimestep;

        QueryServer::Query gridQuery;
        if (geometryIdStr > "")
          splitString(geometryIdStr, ',', gridQuery.mGeometryIdList);

        prepareGridQuery(gridQuery,
                         additionalParameters,
                         masterquery,
                         mode,
                         aLevelId,
                         *level,
                         areaproducers,
                         tloc,
                         loc,
                         polygonPath);

        std::vector<TimeSeriesData> tsdatavector;
        outputData.push_back(make_pair(get_location_id(tloc.loc), tsdatavector));

        if (queryStreamer != nullptr)
        {
          gridQuery.mFlags |= QueryServer::Query::Flags::GeometryHitNotRequired;
        }

        // gridQuery.print(std::cout,0,0);
        int result = itsGridEngine->executeQuery(gridQuery);

        if (result != 0)
        {
          Spine::Exception exception(BCP, "The query server returns an error message!");
          exception.addParameter("Result", std::to_string(result));
          exception.addParameter("Message", QueryServer::getResultString(result));

          switch (result)
          {
            case QueryServer::Result::NO_PRODUCERS_FOUND:
              exception.addDetail(
                  "The reason for this situation is usually that the given producer is unknown");
              exception.addDetail(
                  "or there are no producer list available in the grid engine's configuration "
                  "file.");
              break;
          }
          throw exception;
        }

        if (queryStreamer != nullptr)
        {
          queryStreamer->init(0, itsGridEngine->getQueryServer_sptr());
          insertFileQueries(gridQuery, queryStreamer);
        }

        //        gridQuery.print(std::cout, 0, 0);

        T::Coordinate_vec coordinates;
        std::set<boost::local_time::local_date_time> aggregationTimes;

        int pLen = C_INT(gridQuery.mQueryParameterList.size());
        for (int p = 0; p < pLen; p++)
        {
          uint xLen = gridQuery.mQueryParameterList[p].mValueList.size();
          for (uint x = 0; x < xLen; x++)
          {
            if ((gridQuery.mQueryParameterList[p].mValueList[x].mFlags &
                 QueryServer::ParameterValues::Flags::AggregationValue) != 0)
            {
              // This value is added for aggregation. We should remove it later.

              boost::local_time::local_date_time queryTime(
                  boost::posix_time::from_iso_string(
                      gridQuery.mQueryParameterList[p].mValueList[x].mForecastTime),
                  tz);
              if (aggregationTimes.find(queryTime) == aggregationTimes.end())
              {
                aggregationTimes.insert(queryTime);
              }
            }

            if (coordinates.size() == 0)
            {
              uint len = gridQuery.mQueryParameterList[p].mValueList[x].mValueList.getLength();
              if (len > 0)
              {
                for (uint v = 0; v < len; v++)
                {
                  T::GridValue val;
                  if (gridQuery.mQueryParameterList[p].mValueList[x].mValueList.getGridValueByIndex(
                          v, val))
                  {
                    coordinates.push_back(T::Coordinate(val.mX, val.mY));
                  }
                }
              }
            }
          }
        }

        unsigned long long outputTime = getTime();
        std::vector<std::string> filenameList;

        // Going through all parameters

        for (int p = 0; p < pLen; p++)
        {
          std::vector<TimeSeriesData> aggregatedData;

          Spine::TimeSeries::TimeSeriesPtr tsForParameter(new Spine::TimeSeries::TimeSeries());
          Spine::TimeSeries::TimeSeriesPtr tsForNonGridParam(new Spine::TimeSeries::TimeSeries());
          Spine::TimeSeries::TimeSeriesGroupPtr tsForGroup(
              new Spine::TimeSeries::TimeSeriesGroup());

          // Counting the number of values that the parameter can have in single timestep.

          uint vLen = 0;
          uint xLen = 0;
          if (C_INT(gridQuery.mQueryParameterList.size()) > p &&
              gridQuery.mQueryParameterList[p].mValueList.size() > 0)
          {
            // ### Going through all timesteps.

            uint timestepCount = gridQuery.mQueryParameterList[p].mValueList.size();
            for (uint x = 0; x < timestepCount; x++)
            {
              if (gridQuery.mQueryParameterList[p].mValueList[x].mValueData.size() > 0)
                xLen = 1;

              uint vv = gridQuery.mQueryParameterList[p].mValueList[x].mValueList.getLength();
              if (vv > vLen)
              {
                vLen = vv;
              }
            }
          }

          uint rLen = gridQuery.mQueryParameterList[p].mValueList.size();
          uint tLen = gridQuery.mForecastTimeList.size();

          if (vLen > 0 && rLen <= tLen)
          {
            // The query has returned at least some values for the parameter.

            for (uint v = 0; v < vLen; v++)
            {
              Spine::TimeSeries::TimeSeries ts;

              for (uint t = 0; t < tLen; t++)
              {
                boost::local_time::local_date_time queryTime(
                    boost::posix_time::from_iso_string(
                        gridQuery.mQueryParameterList[p].mValueList[t].mForecastTime),
                    tz);

                T::GridValue val;
                if (gridQuery.mQueryParameterList[p].mValueList[t].mValueList.getGridValueByIndex(
                        v, val) &&
                    (val.mValue != ParamValueMissing || val.mValueString.length() > 0))
                {
                  if (val.mValueString.length() > 0)
                  {
                    // The parameter value is a string
                    Spine::TimeSeries::TimedValue tsValue(
                        queryTime, Spine::TimeSeries::Value(val.mValueString));
                    if (vLen == 1)
                    {
                      // The parameter has only single value in the timestep
                      tsForParameter->push_back(tsValue);
                    }
                    else
                    {
                      // The parameter has multiple values in the same timestep
                      ts.push_back(tsValue);
                    }
                  }
                  else
                  {
                    // The parameter value is numeric
                    Spine::TimeSeries::TimedValue tsValue(queryTime,
                                                          Spine::TimeSeries::Value(val.mValue));
                    if (vLen == 1)
                    {
                      // The parameter has only single value in the timestep
                      tsForParameter->push_back(tsValue);
                    }
                    else
                    {
                      // The parameter has multiple values in the same timestep
                      ts.push_back(tsValue);
                    }
                  }
                }
                else
                {
                  // The parameter value is missing

                  Spine::TimeSeries::TimedValue tsValue(queryTime, missing_value);
                  if (vLen == 1)
                  {
                    // The parameter has only single value in the timestep
                    tsForParameter->push_back(tsValue);
                  }
                  else
                  {
                    // The parameter has multiple values in the same timestep
                    ts.push_back(tsValue);
                  }
                }
              }

              if (vLen > 1)
              {
                T::GridValue val;
                if (gridQuery.mQueryParameterList[p].mValueList[0].mValueList.getGridValueByIndex(
                        v, val))
                {
                  Spine::TimeSeries::LonLatTimeSeries llSeries(
                      Spine::TimeSeries::LonLat(val.mX, val.mY), ts);
                  tsForGroup->push_back(llSeries);
                }
                else
                {
                  Spine::TimeSeries::LonLatTimeSeries llSeries(Spine::TimeSeries::LonLat(0, 0), ts);
                  tsForGroup->push_back(llSeries);
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
            size_t tLen = gridQuery.mForecastTimeList.size();
            size_t t = 0;

            for (auto ft = gridQuery.mForecastTimeList.begin();
                 ft != gridQuery.mForecastTimeList.end();
                 ++ft)
            {
              boost::local_time::local_date_time queryTime(boost::posix_time::from_iso_string(*ft),
                                                           tz);

              if (xLen == 1)
              {
                size_t size = gridQuery.mQueryParameterList[p].mValueList[t].mValueData.size();
                uint step = 1;
                if (size > 0)
                  step = 256 / size;

                int width = atoi(gridQuery.mAttributeList.getAttributeValue("grid.width"));
                int height = atoi(gridQuery.mAttributeList.getAttributeValue("grid.height"));
                bool rotate = (bool)atoi(
                    gridQuery.mAttributeList.getAttributeValue("grid.reverseYDirection"));

                if (width > 0 && height > 0)
                {
                  double mp = 1;
                  int imageWidth = width * mp;
                  int imageHeight = height * mp;

                  ImagePaint imagePaint(imageWidth, imageHeight, 0xFFFFFFFF, false, !rotate);

                  for (size_t s = 0; s < size; s++)
                  {
                    uint col = 0x000000;
                    // printf("--- colors %d = %lu
                    // %lu\n",p,s,gridQuery.mQueryParameterList[p].mContourColors.size());
                    if (s < gridQuery.mQueryParameterList[p].mContourColors.size())
                    {
                      col = gridQuery.mQueryParameterList[p].mContourColors[s];
                    }
                    else
                    {
                      uint r = s * step;
                      col = (r << 16) + (r << 8) + r;
                    }

                    imagePaint.paintWkb(
                        mp,
                        mp,
                        0,
                        0,
                        gridQuery.mQueryParameterList[p].mValueList[t].mValueData[s],
                        col);
                  }

                  char filename[100];
                  sprintf(filename, "/tmp/timeseries_contours_%llu_%u_%lu.png", outputTime, p, t);
                  imagePaint.savePngImage(filename);
                  std::string image = fileToBase64(filename);
                  filenameList.push_back(std::string(filename));

                  std::string s1 = "<img border=\"5\" src=\"data:image/png;base64,";

                  Spine::TimeSeries::TimedValue tsValue(queryTime, s1 + image + "\"/>");
                  tsForNonGridParam->push_back(tsValue);
                }
                else
                {
                  Spine::TimeSeries::TimedValue tsValue(queryTime, "Contour");
                  tsForNonGridParam->push_back(tsValue);
                }
              }
              else if (coordinates.size() > 1 &&
                       strcasecmp(gridQuery.mQueryParameterList[p].mParam.c_str(), "lat") == 0)
              {
                uint len = coordinates.size();
                std::ostringstream output;
                output << "[";
                for (uint i = 0; i < len; i++)
                {
                  output << masterquery.valueformatter.format(
                      coordinates[i].y(), gridQuery.mQueryParameterList[p].mPrecision);
                  if ((i + 1) < len)
                    output << " ";
                }
                output << "]";
                Spine::TimeSeries::TimedValue tsValue(queryTime,
                                                      Spine::TimeSeries::Value(output.str()));
                tsForNonGridParam->push_back(tsValue);
              }
              else if (coordinates.size() > 1 &&
                       strcasecmp(gridQuery.mQueryParameterList[p].mParam.c_str(), "lon") == 0)
              {
                uint len = coordinates.size();
                std::ostringstream output;
                output << "[";
                for (uint i = 0; i < len; i++)
                {
                  output << masterquery.valueformatter.format(
                      coordinates[i].x(), gridQuery.mQueryParameterList[p].mPrecision);
                  if ((i + 1) < len)
                    output << " ";
                }
                output << "]";
                Spine::TimeSeries::TimedValue tsValue(queryTime,
                                                      Spine::TimeSeries::Value(output.str()));
                tsForNonGridParam->push_back(tsValue);
              }
              else if (coordinates.size() > 1 &&
                       strcasecmp(gridQuery.mQueryParameterList[p].mParam.c_str(), "latlon") == 0)
              {
                uint len = coordinates.size();
                std::ostringstream output;
                output << "[";
                for (uint i = 0; i < len; i++)
                {
                  output << masterquery.valueformatter.format(
                                coordinates[i].x(), gridQuery.mQueryParameterList[p].mPrecision)
                         << ",";
                  output << masterquery.valueformatter.format(
                      coordinates[i].y(), gridQuery.mQueryParameterList[p].mPrecision);
                  if ((i + 1) < len)
                    output << " ";
                }
                output << "]";
                Spine::TimeSeries::TimedValue tsValue(queryTime,
                                                      Spine::TimeSeries::Value(output.str()));
                tsForNonGridParam->push_back(tsValue);
              }
              else if (coordinates.size() > 1 &&
                       strcasecmp(gridQuery.mQueryParameterList[p].mParam.c_str(), "lonlat") == 0)
              {
                uint len = coordinates.size();
                std::ostringstream output;
                output << "[";
                for (uint i = 0; i < len; i++)
                {
                  output << masterquery.valueformatter.format(
                                coordinates[i].x(), gridQuery.mQueryParameterList[p].mPrecision)
                         << ",";
                  output << masterquery.valueformatter.format(
                      coordinates[i].y(), gridQuery.mQueryParameterList[p].mPrecision);
                  if ((i + 1) < len)
                    output << " ";
                }
                output << "]";
                Spine::TimeSeries::TimedValue tsValue(queryTime,
                                                      Spine::TimeSeries::Value(output.str()));
                tsForNonGridParam->push_back(tsValue);
              }
              else if (additionalParameters.getParameterValueByLocation(
                           gridQuery.mQueryParameterList[p].mParam,
                           tloc.tag,
                           loc,
                           country,
                           gridQuery.mQueryParameterList[p].mPrecision,
                           paramValue))
              {
                Spine::TimeSeries::TimedValue tsValue(queryTime, paramValue);
                tsForNonGridParam->push_back(tsValue);
              }
              else if (additionalParameters.getParameterValueByLocationAndTime(
                           gridQuery.mQueryParameterList[p].mParam,
                           tloc.tag,
                           loc,
                           queryTime,
                           tz,
                           gridQuery.mQueryParameterList[p].mPrecision,
                           paramValue))
              {
                Spine::TimeSeries::TimedValue tsValue(queryTime, paramValue);
                tsForNonGridParam->push_back(tsValue);
              }
              else if (gridQuery.mQueryParameterList[p].mParam.substr(0, 3) == "@L-")
              {
                int idx = atoi(gridQuery.mQueryParameterList[p].mParam.substr(3).c_str());
                if (idx >= 0 && idx < pLen)
                {
                  Spine::TimeSeries::TimedValue tsValue(
                      queryTime,
                      static_cast<int>(
                          gridQuery.mQueryParameterList[idx].mValueList[t].mParameterLevel));
                  tsForNonGridParam->push_back(tsValue);
                }
                else
                {
                  Spine::TimeSeries::TimedValue tsValue(queryTime, *level);
                  tsForNonGridParam->push_back(tsValue);
                }
              }
              else if (gridQuery.mQueryParameterList[p].mParam == "level")
              {
                int idx = 0;

                int levelValue = *level;
                while (idx < pLen && levelValue == 0)
                {
                  if (gridQuery.mQueryParameterList[idx].mValueList[t].mParameterLevel > 0)
                  {
                    levelValue = gridQuery.mQueryParameterList[idx].mValueList[t].mParameterLevel;
                    if (gridQuery.mQueryParameterList[idx].mValueList[t].mParameterLevelIdType ==
                            T::ParamLevelIdTypeValue::FMI &&
                        gridQuery.mQueryParameterList[idx].mValueList[t].mParameterLevelId == 2)
                      levelValue = levelValue / 100;
                  }

                  idx++;
                }

                Spine::TimeSeries::TimedValue tsValue(queryTime, levelValue);
                tsForNonGridParam->push_back(tsValue);
              }
              else if (gridQuery.mQueryParameterList[p].mParam.substr(0, 4) == "@LT-")
              {
                int idx = atoi(gridQuery.mQueryParameterList[p].mParam.substr(4).c_str());
                if (idx >= 0 && idx < pLen)
                {
                  Spine::TimeSeries::TimedValue tsValue(
                      queryTime,
                      static_cast<int>(
                          gridQuery.mQueryParameterList[idx].mValueList[t].mParameterLevelId));
                  tsForNonGridParam->push_back(tsValue);
                }
              }
              else if (gridQuery.mQueryParameterList[p].mParam == "model" ||
                       gridQuery.mQueryParameterList[p].mParam == "producer")
              {
                int idx = 0;
                while (idx < pLen)
                {
                  if (gridQuery.mQueryParameterList[idx].mValueList[t].mProducerId > 0)
                  {
                    T::ProducerInfo* info = itsProducerInfoList.getProducerInfoById(
                        gridQuery.mQueryParameterList[idx].mValueList[t].mProducerId);
                    if (info == nullptr)
                    {
                      contentServer->getProducerInfoList(0, itsProducerInfoList);
                      itsProducerInfoList_updateTime = time(nullptr);
                      info = itsProducerInfoList.getProducerInfoById(
                          gridQuery.mQueryParameterList[idx].mValueList[t].mProducerId);
                    }

                    if (info != nullptr)
                    {
                      Spine::TimeSeries::TimedValue tsValue(queryTime, info->mName);
                      tsForNonGridParam->push_back(tsValue);
                      idx = pLen + 10;
                    }
                  }
                  idx++;
                }

                if (idx == pLen)
                {
                  std::string producer = "Unknown";
                  if (gridQuery.mProducerNameList.size() == 1)
                  {
                    std::vector<std::string> pnameList;
                    itsGridEngine->getProducerNameList(gridQuery.mProducerNameList[0], pnameList);
                    if (pnameList.size() > 0)
                      producer = pnameList[0];
                  }

                  Spine::TimeSeries::TimedValue tsValue(queryTime, producer);
                  tsForNonGridParam->push_back(tsValue);
                }
              }
              else if (gridQuery.mQueryParameterList[p].mParam.substr(0, 3) == "@P-")
              {
                int idx = atoi(gridQuery.mQueryParameterList[p].mParam.substr(3).c_str());
                if (idx >= 0 && idx < pLen)
                {
                  T::ProducerInfo* info = itsProducerInfoList.getProducerInfoById(
                      gridQuery.mQueryParameterList[idx].mValueList[t].mProducerId);
                  if (info == nullptr &&
                      gridQuery.mQueryParameterList[idx].mValueList[t].mProducerId != 0 &&
                      (itsProducerInfoList_updateTime + 60) < time(nullptr))
                  {
                    contentServer->getProducerInfoList(0, itsProducerInfoList);
                    itsProducerInfoList_updateTime = time(nullptr);
                    info = itsProducerInfoList.getProducerInfoById(
                        gridQuery.mQueryParameterList[idx].mValueList[t].mProducerId);
                  }

                  if (info != nullptr)
                  {
                    Spine::TimeSeries::TimedValue tsValue(queryTime, info->mName);
                    tsForNonGridParam->push_back(tsValue);
                  }
                  else
                  {
                    Spine::TimeSeries::TimedValue tsValue(
                        queryTime,
                        std::to_string(
                            gridQuery.mQueryParameterList[idx].mValueList[t].mProducerId));
                    tsForNonGridParam->push_back(tsValue);
                  }
                }
              }
              else if (gridQuery.mQueryParameterList[p].mParam.substr(0, 3) == "@G-")
              {
                int idx = atoi(gridQuery.mQueryParameterList[p].mParam.substr(3).c_str());
                if (idx >= 0 && idx < pLen)
                {
                  T::GenerationInfo* info = itsGenerationInfoList.getGenerationInfoById(
                      gridQuery.mQueryParameterList[idx].mValueList[t].mGenerationId);
                  if (info == nullptr &&
                      gridQuery.mQueryParameterList[idx].mValueList[t].mGenerationId != 0 &&
                      (itsGenerationInfoList_updateTime + 60) < time(nullptr))
                  {
                    contentServer->getGenerationInfoList(0, itsGenerationInfoList);
                    itsGenerationInfoList_updateTime = time(nullptr);
                    info = itsGenerationInfoList.getGenerationInfoById(
                        gridQuery.mQueryParameterList[idx].mValueList[t].mGenerationId);
                  }
                  if (info != nullptr)
                  {
                    Spine::TimeSeries::TimedValue tsValue(queryTime, info->mName);
                    tsForNonGridParam->push_back(tsValue);
                  }
                  else
                  {
                    Spine::TimeSeries::TimedValue tsValue(
                        queryTime,
                        std::to_string(
                            gridQuery.mQueryParameterList[idx].mValueList[t].mGenerationId));
                    tsForNonGridParam->push_back(tsValue);
                  }
                }
              }
              else if (gridQuery.mQueryParameterList[p].mParam.substr(0, 4) == "@AT-")
              {
                int idx = atoi(gridQuery.mQueryParameterList[p].mParam.substr(4).c_str());
                if (idx >= 0 && idx < pLen)
                {
                  T::GenerationInfo* info = itsGenerationInfoList.getGenerationInfoById(
                      gridQuery.mQueryParameterList[idx].mValueList[t].mGenerationId);
                  if (info == nullptr &&
                      gridQuery.mQueryParameterList[idx].mValueList[t].mGenerationId != 0 &&
                      (itsGenerationInfoList_updateTime + 60) < time(nullptr))
                  {
                    contentServer->getGenerationInfoList(0, itsGenerationInfoList);
                    itsGenerationInfoList_updateTime = time(nullptr);
                    info = itsGenerationInfoList.getGenerationInfoById(
                        gridQuery.mQueryParameterList[idx].mValueList[t].mGenerationId);
                  }
                  if (info != nullptr)
                  {
                    Spine::TimeSeries::TimedValue tsValue(queryTime, info->mAnalysisTime);
                    tsForNonGridParam->push_back(tsValue);
                  }
                  else
                  {
                    Spine::TimeSeries::TimedValue tsValue(queryTime, std::string("Unknown"));
                    tsForNonGridParam->push_back(tsValue);
                  }
                }
              }
              else if (gridQuery.mQueryParameterList[p].mParam.substr(0, 4) == "@FT-")
              {
                int idx = atoi(gridQuery.mQueryParameterList[p].mParam.substr(4).c_str());
                if (idx >= 0 && idx < pLen)
                {
                  Spine::TimeSeries::TimedValue tsValue(
                      queryTime,
                      static_cast<int>(
                          gridQuery.mQueryParameterList[idx].mValueList[t].mForecastType));
                  tsForNonGridParam->push_back(tsValue);
                }
                else
                {
                  Spine::TimeSeries::TimedValue tsValue(queryTime, *level);
                  tsForNonGridParam->push_back(tsValue);
                }
              }
              else if (gridQuery.mQueryParameterList[p].mParam.substr(0, 4) == "@FN-")
              {
                int idx = atoi(gridQuery.mQueryParameterList[p].mParam.substr(4).c_str());
                if (idx >= 0 && idx < pLen)
                {
                  Spine::TimeSeries::TimedValue tsValue(
                      queryTime,
                      static_cast<int>(
                          gridQuery.mQueryParameterList[idx].mValueList[t].mForecastNumber));
                  tsForNonGridParam->push_back(tsValue);
                }
                else
                {
                  Spine::TimeSeries::TimedValue tsValue(queryTime, *level);
                  tsForNonGridParam->push_back(tsValue);
                }
              }
              else if (gridQuery.mQueryParameterList[p].mParam == "origintime")
              {
                int idx = 0;
                while (idx < pLen)
                {
                  if (gridQuery.mQueryParameterList[idx].mValueList[t].mGenerationId > 0)
                  {
                    T::GenerationInfo* info = itsGenerationInfoList.getGenerationInfoById(
                        gridQuery.mQueryParameterList[idx].mValueList[t].mGenerationId);
                    if (info == nullptr && (itsGenerationInfoList_updateTime + 60) < time(nullptr))
                    {
                      contentServer->getGenerationInfoList(0, itsGenerationInfoList);
                      itsGenerationInfoList_updateTime = time(nullptr);
                      info = itsGenerationInfoList.getGenerationInfoById(
                          gridQuery.mQueryParameterList[idx].mValueList[t].mGenerationId);
                    }
                    if (info != nullptr)
                    {
                      // boost::posix_time::ptime origTime =
                      // Fmi::TimeParser::parse_iso(info->mAnalysisTime);
                      boost::local_time::local_date_time origTime(
                          boost::posix_time::from_iso_string(info->mAnalysisTime), tz);

                      // boost::local_time::local_date_time origTime(oTime);
                      Spine::TimeSeries::TimedValue tsValue(
                          queryTime, masterquery.timeformatter->format(origTime));
                      tsForNonGridParam->push_back(tsValue);
                      idx = pLen + 10;
                    }
                  }
                  idx++;
                }
                if (idx == pLen)
                {
                  Spine::TimeSeries::TimedValue tsValue(queryTime, std::string("Unknown"));
                  tsForNonGridParam->push_back(tsValue);
                }
              }
              else if (gridQuery.mQueryParameterList[p].mParam == "modtime" ||
                       gridQuery.mQueryParameterList[p].mParam == "mtime")
              {
                int idx = 0;
                while (idx < pLen)
                {
                  if (gridQuery.mQueryParameterList[idx].mValueList[t].mModificationTime > "")
                  {
                    boost::local_time::local_date_time modTime(
                        boost::posix_time::from_iso_string(
                            gridQuery.mQueryParameterList[idx].mValueList[t].mModificationTime),
                        tz);
                    Spine::TimeSeries::TimedValue tsValue(
                        queryTime, masterquery.timeformatter->format(modTime));
                    tsForNonGridParam->push_back(tsValue);
                    idx = pLen + 10;
                  }
                  idx++;
                }

                if (idx == pLen)
                {
                  Spine::TimeSeries::TimedValue tsValue(queryTime, std::string("Unknown"));
                  tsForNonGridParam->push_back(tsValue);
                }
              }
              else if (gridQuery.mQueryParameterList[p].mParam.substr(0, 3) == "@I-")
              {
                int idx = atoi(gridQuery.mQueryParameterList[p].mParam.substr(3).c_str());
                if (idx >= 0 && idx < pLen)
                {
                  std::string producerName;
                  T::ProducerInfo* producer = itsProducerInfoList.getProducerInfoById(
                      gridQuery.mQueryParameterList[idx].mValueList[t].mProducerId);
                  if (producer != nullptr)
                    producerName = producer->mName;

                  char tmp[1000];

                  //gridQuery.mQueryParameterList[idx].mValueList[t].mGenerationId;
                  //gridQuery.mQueryParameterList[idx].mValueList[t].mGeometryId;

                  sprintf(tmp,
                          "%s:%s:%u:%d:%d:%d:%d",
                          gridQuery.mQueryParameterList[idx].mValueList[t].mParameterKey.c_str(),
                          producerName.c_str(),
                          gridQuery.mQueryParameterList[idx].mValueList[t].mGeometryId,
                          C_INT(gridQuery.mQueryParameterList[idx].mValueList[t].mParameterLevelId),
                          C_INT(gridQuery.mQueryParameterList[idx].mValueList[t].mParameterLevel),
                          C_INT(gridQuery.mQueryParameterList[idx].mValueList[t].mForecastType),
                          C_INT(gridQuery.mQueryParameterList[idx].mValueList[t].mForecastNumber));

                  Spine::TimeSeries::TimedValue tsValue(queryTime, std::string(tmp));
                  tsForNonGridParam->push_back(tsValue);
                }
              }
              else if ((gridQuery.mQueryParameterList[p].mFlags & QueryServer::QueryParameter::Flags::NoReturnValues) != 0  &&
                  (gridQuery.mQueryParameterList[p].mValueList[t].mFlags & QueryServer::ParameterValues::Flags::DataAvailable) != 0)
              {
                std::string producerName;
                T::ProducerInfo* producer = itsProducerInfoList.getProducerInfoById(gridQuery.mQueryParameterList[p].mValueList[t].mProducerId);
                if (producer != nullptr)
                  producerName = producer->mName;

                // gridQuery.mQueryParameterList[p].mValueList[t].print(std::cout,0,0);

                char tmp[1000];
                sprintf(tmp,"%s:%s:%u:%d:%d:%d:%d %s flags:%d",
                          gridQuery.mQueryParameterList[p].mValueList[t].mParameterKey.c_str(),
                          producerName.c_str(),
                          gridQuery.mQueryParameterList[p].mValueList[t].mGeometryId,
                          C_INT(gridQuery.mQueryParameterList[p].mValueList[t].mParameterLevelId),
                          C_INT(gridQuery.mQueryParameterList[p].mValueList[t].mParameterLevel),
                          C_INT(gridQuery.mQueryParameterList[p].mValueList[t].mForecastType),
                          C_INT(gridQuery.mQueryParameterList[p].mValueList[t].mForecastNumber),
                          gridQuery.mQueryParameterList[p].mValueList[t].mAnalysisTime.c_str(),
                          C_INT(gridQuery.mQueryParameterList[p].mValueList[t].mFlags));

                Spine::TimeSeries::TimedValue tsValue(queryTime, std::string(tmp));
                //tsForNonGridParam->push_back(tsValue);
                tsForParameter->push_back(tsValue);

              }
              else if (gridQuery.mQueryParameterList[p].mParam.substr(0, 4) == "@GM-")
              {
                int idx = atoi(gridQuery.mQueryParameterList[p].mParam.substr(4).c_str());
                if (idx >= 0 && idx < pLen)
                {
                  Spine::TimeSeries::TimedValue tsValue(
                      queryTime,
                      std::to_string(gridQuery.mQueryParameterList[idx].mValueList[t].mGeometryId));
                  tsForNonGridParam->push_back(tsValue);
                }
              }
              else if (gridQuery.mQueryParameterList[p].mParam.substr(0, 7) == "@MERGE-")
              {
                std::vector<std::string> partList;
                std::vector<std::string> fileList;

                splitString(gridQuery.mQueryParameterList[p].mParam, '-', partList);

                uint partCount = partList.size();

                char filename[100];
                for (uint pp = 1; pp < partCount; pp++)
                {
                  sprintf(filename,
                          "/tmp/timeseries_contours_%llu_%s_%lu.png",
                          outputTime,
                          partList[pp].c_str(),
                          t);
                  if (getFileSize(filename) > 0)
                    fileList.push_back(std::string(filename));
                }

                if (fileList.size() > 0)
                {
                  sprintf(filename, "/tmp/timeseries_contours_%llu_%u_%lu.png", outputTime, p, t);
                  mergePngFiles(filename, fileList);

                  std::string image = fileToBase64(filename);
                  filenameList.push_back(std::string(filename));

                  std::string s1 = "<img border=\"5\" src=\"data:image/png;base64,";

                  Spine::TimeSeries::TimedValue tsValue(queryTime, s1 + image + "\"/>");
                  tsForNonGridParam->push_back(tsValue);
                }
                else
                {
                  Spine::TimeSeries::TimedValue tsValue(queryTime, "Contour");
                  tsForNonGridParam->push_back(tsValue);
                }
              }
              else if (rLen > tLen)
              {
                // It seems that there are more values available for each time step than expected.
                // This usually happens if the requested parameter definition does not contain
                // enough information in order to identify an unique parameter. For example, if the
                // parameter definition does not contain level information then the result is that
                // we get values from several levels.

                char tmp[10000];
                std::set<std::string> pList;

                for (uint r = 0; r < rLen; r++)
                {
                  if (gridQuery.mQueryParameterList[p].mValueList[r].mForecastTime == *ft)
                  {
                    std::string producerName;
                    T::ProducerInfo* producer = itsProducerInfoList.getProducerInfoById(
                        gridQuery.mQueryParameterList[p].mValueList[r].mProducerId);
                    if (producer != nullptr)
                      producerName = producer->mName;

                    sprintf(tmp,
                            "%s:%d:%d:%d:%d:%s",
                            gridQuery.mQueryParameterList[p].mValueList[r].mParameterKey.c_str(),
                            C_INT(gridQuery.mQueryParameterList[p].mValueList[r].mParameterLevelId),
                            C_INT(gridQuery.mQueryParameterList[p].mValueList[r].mParameterLevel),
                            C_INT(gridQuery.mQueryParameterList[p].mValueList[r].mForecastType),
                            C_INT(gridQuery.mQueryParameterList[p].mValueList[r].mForecastNumber),
                            producerName.c_str());

                    if (pList.find(std::string(tmp)) == pList.end())
                      pList.insert(std::string(tmp));
                  }
                }
                char* pp = tmp;
                pp += sprintf(pp, "### MULTI-MATCH ### ");
                for (auto p = pList.begin(); p != pList.end(); ++p)
                  pp += sprintf(pp, "%s ", p->c_str());

                Spine::TimeSeries::TimedValue tsValue(queryTime, std::string(tmp));
                tsForNonGridParam->push_back(tsValue);
              }
              else
              {
                Spine::TimeSeries::TimedValue tsValue(queryTime, missing_value);
                // Spine::TimeSeries::TimedValue tsValue(queryTime,
                // Spine::TimeSeries::Value(std::numeric_limits<double>::quiet_NaN()));
                tsForParameter->push_back(tsValue);
              }
              t++;
            }

            if (tsForNonGridParam->size() > 0)
              aggregatedData.push_back(erase_redundant_timesteps(
                  DataFunctions::aggregate(tsForNonGridParam, paramFuncs[p].functions),
                  aggregationTimes));
          }

          if (tsForParameter->size() > 0)
          {
            aggregatedData.push_back(erase_redundant_timesteps(
                DataFunctions::aggregate(tsForParameter, paramFuncs[p].functions),
                aggregationTimes));
          }

          if (tsForGroup->size() > 0)
          {
            aggregatedData.push_back(erase_redundant_timesteps(
                DataFunctions::aggregate(tsForGroup, paramFuncs[p].functions), aggregationTimes));
          }

          DataFunctions::store_data(aggregatedData, masterquery, outputData);
        }

        for (auto f = filenameList.begin(); f != filenameList.end(); ++f)
          remove(f->c_str());
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", nullptr);
  }
}

void GridInterface::erase_redundant_timesteps(
    ts::TimeSeries& ts, std::set<boost::local_time::local_date_time>& aggregationTimes)
{
  FUNCTION_TRACE
  try
  {
    ts::TimeSeries no_redundant;
    no_redundant.reserve(ts.size());
    std::set<std::string> newTimes;

    for (auto it = ts.begin(); it != ts.end(); ++it)
    {
      std::string lTime = Fmi::to_iso_string(it->time.local_time());
      if (aggregationTimes.find(it->time) == aggregationTimes.end() &&
          newTimes.find(lTime) == newTimes.end())
      {
        no_redundant.push_back(*it);
        newTimes.insert(lTime);
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
    throw Spine::Exception(BCP, "Operation failed!", nullptr);
  }
}

ts::TimeSeriesPtr GridInterface::erase_redundant_timesteps(
    ts::TimeSeriesPtr ts, std::set<boost::local_time::local_date_time>& aggregationTimes)
{
  FUNCTION_TRACE
  try
  {
    erase_redundant_timesteps(*ts, aggregationTimes);
    return ts;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", nullptr);
  }
}

ts::TimeSeriesVectorPtr GridInterface::erase_redundant_timesteps(
    ts::TimeSeriesVectorPtr tsv, std::set<boost::local_time::local_date_time>& aggregationTimes)
{
  FUNCTION_TRACE
  try
  {
    for (unsigned int i = 0; i < tsv->size(); i++)
      erase_redundant_timesteps(tsv->at(i), aggregationTimes);

    return tsv;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", nullptr);
  }
}

ts::TimeSeriesGroupPtr GridInterface::erase_redundant_timesteps(
    ts::TimeSeriesGroupPtr tsg, std::set<boost::local_time::local_date_time>& aggregationTimes)
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
    throw Spine::Exception(BCP, "Operation failed!", nullptr);
  }
}

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
