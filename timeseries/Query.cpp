// ======================================================================
/*!
 * \brief Utility functions for parsing the request
 */
// ======================================================================

#include "Query.h"
#include "Config.h"
#include "Hash.h"
#include "State.h"
#include <engines/geonames/Engine.h>
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/Engine.h>
#endif
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/foreach.hpp>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/ShowFunction.h>
#include <macgyver/DistanceParser.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeParser.h>
#include <macgyver/AnsiEscapeCodes.h>
#include <newbase/NFmiPoint.h>
#include <spine/Convenience.h>
#include <spine/ParameterFactory.h>
#include <gis/CoordinateTransformation.h>
#include <algorithm>

using namespace std;

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
const char* default_timezone = "localtime";

namespace
{
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

void report_unsupported_option(const std::string& name, const boost::optional<std::string>& value)
{
  if(value)
	std::cerr << (Spine::log_time_str() + ANSI_FG_RED + " [timeseries] Deprecated option '" + name + ANSI_FG_DEFAULT) << std::endl;
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief The constructor parses the query string
 */
// ----------------------------------------------------------------------

Query::Query(const State& state, const Spine::HTTP::Request& req, Config& config)
    : valueformatter(req), timeAggregationRequested(false)
{
  try
  {
	report_unsupported_option("adjustfield", req.getParameter("adjustfield"));
	report_unsupported_option("width", req.getParameter("width"));
	report_unsupported_option("fill", req.getParameter("fill"));
	report_unsupported_option("showpos", req.getParameter("showpos"));
	report_unsupported_option("uppercase", req.getParameter("uppercase"));

    time_t tt = time(nullptr);
    if ((config.itsLastAliasCheck + 10) < tt)
    {
      config.itsAliasFileCollection.checkUpdates(false);
      config.itsLastAliasCheck = tt;
    }

    itsAliasFileCollectionPtr = &config.itsAliasFileCollection;

    forecastSource = Spine::optional_string(req.getParameter("source"), "");

    // ### Attribute list ( attr=name1:value1,name2:value2,name3:$(alias1) )

    string attr = Spine::optional_string(req.getParameter("attr"), "");
    if (!attr.empty())
    {
      bool ind = true;
      uint loopCount = 0;
      while (ind)
      {
        loopCount++;
        if (loopCount > 10)
          throw Fmi::Exception(BCP, "The alias definitions seem to contain an eternal loop!");

        ind = false;
        std::string alias;
        if (itsAliasFileCollectionPtr->replaceAlias(attr, alias))
        {
          attr = alias;
          ind = true;
        }
      }

      std::vector<std::string> partList;
      splitString(attr, ';', partList);
      for (const auto& part : partList)
      {
        std::vector<std::string> list;
        splitString(part, ':', list);
        if (list.size() == 2)
        {
          std::string name = list[0];
          std::string value = list[1];

          attributeList.addAttribute(name, value);
        }
      }
    }

    // attributeList.print(std::cout,0,0);

    T::Attribute* v1 = attributeList.getAttributeByNameEnd("Grib1.IndicatorSection.EditionNumber");
    T::Attribute* v2 = attributeList.getAttributeByNameEnd("Grib2.IndicatorSection.EditionNumber");

    T::Attribute* lat = attributeList.getAttributeByNameEnd("LatitudeOfFirstGridPoint");
    T::Attribute* lon = attributeList.getAttributeByNameEnd("LongitudeOfFirstGridPoint");

    if (v1 != nullptr && lat != nullptr && lon != nullptr)
    {
      // Using coordinate that is inside the GRIB1 grid

      double latitude = toDouble(lat->mValue.c_str()) / 1000;
      double longitude = toDouble(lon->mValue.c_str()) / 1000;

      std::string val = std::to_string(latitude) + "," + std::to_string(longitude);
      Spine::HTTP::Request tmpReq;
      tmpReq.addParameter("latlon", val);
      loptions.reset(
          new Engine::Geonames::LocationOptions(state.getGeoEngine().parseLocations(tmpReq)));
    }
    else if (v2 != nullptr && lat != nullptr && lon != nullptr)
    {
      // Using coordinate that is inside the GRIB2 grid

      double latitude = toDouble(lat->mValue.c_str()) / 1000000;
      double longitude = toDouble(lon->mValue.c_str()) / 1000000;

      std::string val = std::to_string(latitude) + "," + std::to_string(longitude);
      Spine::HTTP::Request tmpReq;
      tmpReq.addParameter("latlon", val);
      loptions.reset(
          new Engine::Geonames::LocationOptions(state.getGeoEngine().parseLocations(tmpReq)));
    }
    else
    {
      loptions.reset(
          new Engine::Geonames::LocationOptions(state.getGeoEngine().parseLocations(req)));

	  auto lon_coord = Spine::optional_double(req.getParameter("x"), kFloatMissing);
	  auto lat_coord = Spine::optional_double(req.getParameter("y"), kFloatMissing);
	  auto source_crs = Spine::optional_string(req.getParameter("crs"), "");
	  
	  if(lon_coord != kFloatMissing && lat_coord != kFloatMissing && !source_crs.empty())
		{	
		  // Transform lon_coord, lat_coord to lonlat parameter
		  if(source_crs != "ESPG:4326")
			{
			  Fmi::CoordinateTransformation transformation(source_crs, "WGS84");
			  transformation.transform(lon_coord, lat_coord);
			}
		  auto tmpReq = req;
		  tmpReq.addParameter("lonlat", (Fmi::to_string(lon_coord) + "," + Fmi::to_string(lat_coord)));
		  loptions.reset(new Engine::Geonames::LocationOptions(state.getGeoEngine().parseLocations(tmpReq)));
		}
    }

    // Store WKT-geometries
    wktGeometries = state.getGeoEngine().getWktGeometries(*loptions, language);

    toptions = Spine::OptionParsers::parseTimes(req);

#ifdef MYDEBUG
    std::cout << "Time options: " << std::endl << toptions << std::endl;
#endif

    latestTimestep = toptions.startTime;

    starttimeOptionGiven = (!!req.getParameter("starttime") || !!req.getParameter("now"));
    std::string endtime = Spine::optional_string(req.getParameter("endtime"), "");
    endtimeOptionGiven = (endtime != "now");

#ifndef WITHOUT_OBSERVATION
    latestObservation = (endtime == "now");
    allplaces = (Spine::optional_string(req.getParameter("places"), "") == "all");
#endif

    debug = Spine::optional_bool(req.getParameter("debug"), false);

    timezone = Spine::optional_string(req.getParameter("tz"), default_timezone);

    step = Spine::optional_double(req.getParameter("step"), 1.0);
    leveltype = Spine::optional_string(req.getParameter("leveltype"), "");
    format = Spine::optional_string(req.getParameter("format"), "ascii");
    areasource = Spine::optional_string(req.getParameter("areasource"), "");
    crs = Spine::optional_string(req.getParameter("crs"), "");

    // Either create the requested locale or use the default one constructed
    // by the Config parser. TODO: If constructing from strings is slow, we should cache locales
    // instead.

    auto opt_locale = req.getParameter("locale");
    if (!opt_locale)
      outlocale = config.defaultLocale();
    else
      outlocale = locale(opt_locale->c_str());

    timeformat = Spine::optional_string(req.getParameter("timeformat"), config.defaultTimeFormat());

    language = Spine::optional_string(req.getParameter("lang"), config.defaultLanguage());
    maxdistanceOptionGiven = !!req.getParameter("maxdistance");
    maxdistance =
        Spine::optional_string(req.getParameter("maxdistance"), config.defaultMaxDistance());

    keyword = Spine::optional_string(req.getParameter("keyword"), "");
    auto searchName = req.getParameterList("inkeyword");
    if (!searchName.empty())
    {
      for (const std::string& keyword : searchName)
      {
        Locus::QueryOptions opts;
        opts.SetLanguage(language);
        Spine::LocationList places = state.getGeoEngine().keywordSearch(opts, keyword);
        if (places.empty())
          throw Fmi::Exception(BCP, "No locations for keyword " + std::string(keyword) + " found");
        inKeywordLocations.insert(inKeywordLocations.end(), places.begin(), places.end());
      }
    }

    findnearestvalidpoint = Spine::optional_bool(req.getParameter("findnearestvalid"), false);

    boost::optional<std::string> tmp = req.getParameter("origintime");
    if (tmp)
    {
      if (*tmp == "latest" || *tmp == "newest")
        origintime = boost::posix_time::ptime(boost::date_time::pos_infin);
      else if (*tmp == "oldest")
        origintime = boost::posix_time::ptime(boost::date_time::neg_infin);
      else
        origintime = Fmi::TimeParser::parse(*tmp);
    }

#ifndef WITHOUT_OBSERVARION
    Query::parse_producers(req, state.getQEngine(), state.getGridEngine(), state.getObsEngine());
#else
    Query::parse_producers(req, state.getQEngine(), state.getGridEngine());
#endif

#ifndef WITHOUT_OBSERVATION
    Query::parse_parameters(req, state.getObsEngine());
#else
    Query::parse_parameters(req);
#endif

    Query::parse_levels(req);

    // wxml requires 8 parameters

    timestring = Spine::optional_string(req.getParameter("timestring"), "");
    if (format == "wxml")
    {
      timeformat = "xml";
      poptions.add(Spine::ParameterFactory::instance().parse("origintime"));
      poptions.add(Spine::ParameterFactory::instance().parse("xmltime"));
      poptions.add(Spine::ParameterFactory::instance().parse("weekday"));
      poptions.add(Spine::ParameterFactory::instance().parse("timestring"));
      poptions.add(Spine::ParameterFactory::instance().parse("name"));
      poptions.add(Spine::ParameterFactory::instance().parse("geoid"));
      poptions.add(Spine::ParameterFactory::instance().parse("longitude"));
      poptions.add(Spine::ParameterFactory::instance().parse("latitude"));
    }

    // This must be done after params is no longer being modified
    // by the above wxml code!

    Query::parse_precision(req, config);

    timeformatter.reset(Fmi::TimeFormatter::create(timeformat));

#ifndef WITHOUT_OBSERVATION
    // observation params
    numberofstations = Spine::optional_int(req.getParameter("numberofstations"), 1);
#endif

#ifndef WITHOUT_OBSERVATION
    auto name = req.getParameter("fmisid");
    if (name)
    {
      const string fmisidreq = *name;
      vector<string> parts;
      boost::algorithm::split(parts, fmisidreq, boost::algorithm::is_any_of(","));

      for (const string& sfmisid : parts)
      {
        int f = Fmi::stoi(sfmisid);
        this->fmisids.push_back(f);
      }
    }

    // sid is an alias for fmisid
    name = req.getParameter("sid");
    if (name)
    {
      const string sidreq = *name;
      vector<string> parts;
      boost::algorithm::split(parts, sidreq, boost::algorithm::is_any_of(","));

      for (const string& ssid : parts)
      {
        int f = Fmi::stoi(ssid);
        this->fmisids.push_back(f);
      }
    }
#endif

#ifndef WITHOUT_OBSERVATION
    name = req.getParameter("wmo");
    if (name)
    {
      const string wmoreq = *name;
      vector<string> parts;
      boost::algorithm::split(parts, wmoreq, boost::algorithm::is_any_of(","));

      for (const string& swmo : parts)
      {
        int w = Fmi::stoi(swmo);
        this->wmos.push_back(w);
      }
    }
#endif

#ifndef WITHOUT_OBSERVATION
    name = req.getParameter("lpnn");
    if (name)
    {
      const string lpnnreq = *name;
      vector<string> parts;
      boost::algorithm::split(parts, lpnnreq, boost::algorithm::is_any_of(","));

      for (const string& slpnn : parts)
      {
        int l = Fmi::stoi(slpnn);
        this->lpnns.push_back(l);
      }
    }
#endif

#ifndef WITHOUT_OBSERVATION
    if (!!req.getParameter("bbox"))
    {
      string bbox = *req.getParameter("bbox");
      vector<string> parts;
      boost::algorithm::split(parts, bbox, boost::algorithm::is_any_of(","));
      std::string lat2(parts[3]);
      if (lat2.find(':') != string::npos)
        lat2 = lat2.substr(0, lat2.find(':'));

      // Bounding box must contain exactly 4 elements
      if (parts.size() != 4)
      {
        throw Fmi::Exception(BCP, "Invalid bounding box '" + bbox + "'!");
      }

      if (!parts[0].empty())
      {
        boundingBox["minx"] = Fmi::stod(parts[0]);
      }
      if (!parts[1].empty())
      {
        boundingBox["miny"] = Fmi::stod(parts[1]);
      }
      if (!parts[2].empty())
      {
        boundingBox["maxx"] = Fmi::stod(parts[2]);
      }
      if (!parts[3].empty())
      {
        boundingBox["maxy"] = Fmi::stod(lat2);
      }
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

    if (!!req.getParameter("weekday"))
    {
      const string sweekdays = *req.getParameter("weekday");
      vector<string> parts;
      boost::algorithm::split(parts, sweekdays, boost::algorithm::is_any_of(","));

      for (const string& wday : parts)
      {
        int h = Fmi::stoi(wday);
        this->weekdays.push_back(h);
      }
    }

    startrow = Spine::optional_size(req.getParameter("startrow"), 0);
    maxresults = Spine::optional_size(req.getParameter("maxresults"), 0);
    groupareas = Spine::optional_bool(req.getParameter("groupareas"), true);
  }
  catch (...)
  {
    // The stack traces are useless when the user has made a typo
    throw Fmi::Exception::Trace(BCP, "TimeSeries plugin failed to parse query string options!")
        .disableStackTrace();
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse producer/model options
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION
void Query::parse_producers(const Spine::HTTP::Request& theReq,
                            const Engine::Querydata::Engine& theQEngine,
                            const Engine::Grid::Engine* theGridEngine,
                            const Engine::Observation::Engine* theObsEngine)
#else
void Query::parse_producers(const Spine::HTTP::Request& theReq,
                            const Engine::Querydata::Engine& theQEngine,
                            const Engine::Grid::Engine& theGridEngine)
#endif
{
  try
  {
    string opt = Spine::optional_string(theReq.getParameter("model"), "");

    string opt2 = Spine::optional_string(theReq.getParameter("producer"), "");

    if (opt == "default" || opt2 == "default")
    {
      // Use default if it's forced by any option
      return;
    }

    // observation uses stationtype-parameter
    if (opt.empty() && opt2.empty())
      opt2 = Spine::optional_string(theReq.getParameter("stationtype"), "");

    std::list<std::string> resultProducers;

    // Handle time separation:: either 'model' or 'producer' keyword used
    if (!opt.empty())
      boost::algorithm::split(resultProducers, opt, boost::algorithm::is_any_of(";"));
    else if (!opt2.empty())
      boost::algorithm::split(resultProducers, opt2, boost::algorithm::is_any_of(";"));

    for (auto& p : resultProducers)
    {
      boost::algorithm::to_lower(p);
      if (p == "itmf")
      {
        p = Engine::Observation::FMI_IOT_PRODUCER;
        iot_producer_specifier = "itmf";
      }
    }

    // Verify the producer names are valid

#ifndef WITHOUT_OBSERVATION
    std::set<std::string> observations;
    if (theObsEngine != nullptr)
      observations = theObsEngine->getValidStationTypes();
#endif

    for (const auto& p : resultProducers)
    {
      bool ok = theQEngine.hasProducer(p);
#ifndef WITHOUT_OBSERVATION
      if (!ok)
        ok = (observations.find(p) != observations.end());
#endif
      if (!ok && theGridEngine)
        ok = theGridEngine->isGridProducer(p);

      if (!ok)
        throw Fmi::Exception(BCP, "Unknown producer name '" + p + "'");
    }

    // Now split into location parts

    for (const auto& tproducers : resultProducers)
    {
      AreaProducers producers;
      boost::algorithm::split(producers, tproducers, boost::algorithm::is_any_of(","));

      // FMI producer is deprecated, use OPENDATA instead
      // std::replace(producers.begin(), producers.end(), std::string("fmi"),
      // std::string("opendata"));

      timeproducers.push_back(producers);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief parse Level query
 *
 * Empty result implies all levels are wanted
 */
// ----------------------------------------------------------------------

void Query::parse_levels(const Spine::HTTP::Request& theReq)
{
  try
  {
    // Get the option string

    string opt = Spine::optional_string(theReq.getParameter("level"), "");
    if (!opt.empty())
    {
      levels.insert(Fmi::stoi(opt));
    }

    // Allow also "levels"
    opt = Spine::optional_string(theReq.getParameter("levels"), "");
    if (!opt.empty())
    {
      vector<string> parts;
      boost::algorithm::split(parts, opt, boost::algorithm::is_any_of(","));
      for (const string& tmp : parts)
        levels.insert(Fmi::stoi(tmp));
    }

    // Get pressures and heights to extract data

    opt = Spine::optional_string(theReq.getParameter("pressure"), "");
    if (!opt.empty())
    {
      pressures.insert(Fmi::stod(opt));
    }

    opt = Spine::optional_string(theReq.getParameter("pressures"), "");
    if (!opt.empty())
    {
      vector<string> parts;
      boost::algorithm::split(parts, opt, boost::algorithm::is_any_of(","));
      for (const string& tmp : parts)
        pressures.insert(Fmi::stod(tmp));
    }

    opt = Spine::optional_string(theReq.getParameter("height"), "");
    if (!opt.empty())
    {
      heights.insert(Fmi::stod(opt));
    }

    opt = Spine::optional_string(theReq.getParameter("heights"), "");
    if (!opt.empty())
    {
      vector<string> parts;
      boost::algorithm::split(parts, opt, boost::algorithm::is_any_of(","));
      for (const string& tmp : parts)
        heights.insert(Fmi::stod(tmp));
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse precision options
 */
// ----------------------------------------------------------------------

void Query::parse_precision(const Spine::HTTP::Request& req, const Config& config)
{
  try
  {
    string precname =
        Spine::optional_string(req.getParameter("precision"), config.defaultPrecision());

    const Precision& prec = config.getPrecision(precname);

    precisions.reserve(poptions.size());

    for (const Spine::OptionParsers::ParameterList::value_type& p : poptions.parameters())
    {
      Precision::Map::const_iterator it = prec.parameter_precisions.find(p.name());
      if (it == prec.parameter_precisions.end())
      {
        precisions.push_back(prec.default_precision);  // unknown gets default
      }
      else
      {
        precisions.push_back(it->second);  // known gets configured value
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

#ifndef WITHOUT_OBSERVATION
void Query::parse_parameters(const Spine::HTTP::Request& theReq,
                             const Engine::Observation::Engine* theObsEngine)
#else
void Query::parse_parameters(const Spine::HTTP::Request& theReq)
#endif
{
  try
  {
    // Get the option string
    string opt =
        Spine::required_string(theReq.getParameter("param"), "The 'param' option is required!");

    // Protect against empty selection
    if (opt.empty())
      throw Fmi::Exception(BCP, "The 'param' option is empty!");

    // Split
    using Names = list<string>;
    Names tmpNames;
    boost::algorithm::split(tmpNames, opt, boost::algorithm::is_any_of(","));

    Names names;
    bool ind = true;
    uint loopCount = 0;
    while (ind)
    {
      loopCount++;
      if (loopCount > 10)
        throw Fmi::Exception(BCP, "The alias definitions seem to contain an eternal loop!");

      ind = false;
      names.clear();

      for (const auto& tmpName : tmpNames)
      {
        std::string alias;
        if (itsAliasFileCollectionPtr->getAlias(tmpName, alias))
        {
          Names tmp;
          boost::algorithm::split(tmp, alias, boost::algorithm::is_any_of(","));
          for (auto tt = tmp.begin(); tt != tmp.end(); ++tt)
          {
            names.push_back(*tt);
          }
          ind = true;
        }
        else if (itsAliasFileCollectionPtr->replaceAlias(tmpName, alias))
        {
          Names tmp;
          boost::algorithm::split(tmp, alias, boost::algorithm::is_any_of(","));
          for (const auto& tt : tmp)
            names.push_back(tt);

          ind = true;
        }
        else
        {
          names.push_back(tmpName);
        }
      }

      if (ind)
        tmpNames = names;
    }

#ifndef WITHOUT_OBSERVATION

    // Determine whether any producer implies observations are needed

    bool obsProducersExist = false;

    if (theObsEngine != nullptr)
    {
      std::set<std::string> obsEngineStationTypes = theObsEngine->getValidStationTypes();
      for (const auto& areaproducers : timeproducers)
      {
        if (obsProducersExist)
          break;

        for (const auto& producer : areaproducers)
        {
          if (obsEngineStationTypes.find(producer) != obsEngineStationTypes.end())
          {
            obsProducersExist = true;
            break;
          }
        }
      }
    }
#endif

    // Validate and convert
    for (const string& paramname : names)
    {
      try
      {
        Spine::ParameterAndFunctions paramfuncs =
            Spine::ParameterFactory::instance().parseNameAndFunctions(paramname, true);

        poptions.add(paramfuncs.parameter, paramfuncs.functions);
      }
      catch (...)
      {
        Fmi::Exception exception(BCP, "Parameter parsing failed for '" + paramname + "'!", NULL);
        throw exception;
      }
    }
    poptions.expandParameter("data_source");

    std::string aggregationIntervalStringBehind =
        Spine::optional_string(theReq.getParameter("interval"), "0m");
    std::string aggregationIntervalStringAhead = ("0m");

    // check if second aggregation interval is defined
    if (aggregationIntervalStringBehind.find(':') != string::npos)
    {
      aggregationIntervalStringAhead =
          aggregationIntervalStringBehind.substr(aggregationIntervalStringBehind.find(':') + 1);
      aggregationIntervalStringBehind =
          aggregationIntervalStringBehind.substr(0, aggregationIntervalStringBehind.find(':'));
    }

    int agg_interval_behind(Spine::duration_string_to_minutes(aggregationIntervalStringBehind));
    int agg_interval_ahead(Spine::duration_string_to_minutes(aggregationIntervalStringAhead));

    if (agg_interval_behind < 0 || agg_interval_ahead < 0)
      throw Fmi::Exception(BCP, "The 'interval' option must be positive!");

    // set aggregation interval if it has not been set in parameter parser
    for (const Spine::ParameterAndFunctions& paramfuncs : poptions.parameterFunctions())
    {
      if (paramfuncs.functions.innerFunction.getAggregationIntervalBehind() ==
          std::numeric_limits<unsigned int>::max())
        const_cast<TS::DataFunction&>(paramfuncs.functions.innerFunction)
            .setAggregationIntervalBehind(agg_interval_behind);
      if (paramfuncs.functions.innerFunction.getAggregationIntervalAhead() ==
          std::numeric_limits<unsigned int>::max())
        const_cast<TS::DataFunction&>(paramfuncs.functions.innerFunction)
            .setAggregationIntervalAhead(agg_interval_ahead);

      if (paramfuncs.functions.outerFunction.getAggregationIntervalBehind() ==
          std::numeric_limits<unsigned int>::max())
        const_cast<TS::DataFunction&>(paramfuncs.functions.outerFunction)
            .setAggregationIntervalBehind(agg_interval_behind);
      if (paramfuncs.functions.outerFunction.getAggregationIntervalAhead() ==
          std::numeric_limits<unsigned int>::max())
        const_cast<TS::DataFunction&>(paramfuncs.functions.outerFunction)
            .setAggregationIntervalAhead(agg_interval_ahead);
    }

    // store maximum aggregation intervals per parameter for later use
    for (const Spine::ParameterAndFunctions& paramfuncs : poptions.parameterFunctions())
    {
      std::string paramname(paramfuncs.parameter.name());

      if (maxAggregationIntervals.find(paramname) == maxAggregationIntervals.end())
        maxAggregationIntervals.insert(make_pair(paramname, AggregationInterval(0, 0)));

      if (paramfuncs.functions.innerFunction.type() == TS::FunctionType::TimeFunction)
      {
        if (maxAggregationIntervals[paramname].behind <
            paramfuncs.functions.innerFunction.getAggregationIntervalBehind())
          maxAggregationIntervals[paramname].behind =
              paramfuncs.functions.innerFunction.getAggregationIntervalBehind();
        if (maxAggregationIntervals[paramname].ahead <
            paramfuncs.functions.innerFunction.getAggregationIntervalAhead())
          maxAggregationIntervals[paramname].ahead =
              paramfuncs.functions.innerFunction.getAggregationIntervalAhead();
      }
      else if (paramfuncs.functions.outerFunction.type() == TS::FunctionType::TimeFunction)
      {
        if (maxAggregationIntervals[paramname].behind <
            paramfuncs.functions.outerFunction.getAggregationIntervalBehind())
          maxAggregationIntervals[paramname].behind =
              paramfuncs.functions.outerFunction.getAggregationIntervalBehind();
        if (maxAggregationIntervals[paramname].ahead <
            paramfuncs.functions.outerFunction.getAggregationIntervalAhead())
          maxAggregationIntervals[paramname].ahead =
              paramfuncs.functions.outerFunction.getAggregationIntervalAhead();
      }

      if (maxAggregationIntervals[paramname].behind > 0 ||
          maxAggregationIntervals[paramname].ahead > 0)
        timeAggregationRequested = true;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

double Query::maxdistance_kilometers() const
{
  return Fmi::DistanceParser::parse_kilometer(maxdistance);
}

double Query::maxdistance_meters() const
{
  return Fmi::DistanceParser::parse_meter(maxdistance);
}

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
