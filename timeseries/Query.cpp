// ======================================================================
/*!
 * \brief Utility functions for parsing the request
 */
// ======================================================================

#include "Query.h"
#include "Config.h"
#include <engines/observation/ExternalAndMobileProducerId.h>
#include <engines/querydata/Engine.h>
#include <gis/CoordinateTransformation.h>
#include <macgyver/AnsiEscapeCodes.h>
#include <macgyver/DistanceParser.h>
#include <spine/Convenience.h>

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
void set_max_agg_interval_behind(TS::DataFunction& func, unsigned int& max_interval)
{
  try
  {
    if (func.type() == TS::FunctionType::TimeFunction)
    {
      if (max_interval < func.getAggregationIntervalBehind())
        max_interval = func.getAggregationIntervalBehind();
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void set_max_agg_interval_ahead(TS::DataFunction& func, unsigned int& max_interval)
{
  try
  {
    if (func.type() == TS::FunctionType::TimeFunction)
    {
      if (max_interval < func.getAggregationIntervalAhead())
        max_interval = func.getAggregationIntervalAhead();
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void set_agg_interval_behind(TS::DataFunction& func, unsigned int interval)
{
  try
  {
    if (func.getAggregationIntervalBehind() == std::numeric_limits<unsigned int>::max())
      func.setAggregationIntervalBehind(interval);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void set_agg_interval_ahead(TS::DataFunction& func, unsigned int interval)
{
  try
  {
    if (func.getAggregationIntervalAhead() == std::numeric_limits<unsigned int>::max())
      func.setAggregationIntervalAhead(interval);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void report_unsupported_option(const std::string& name, const std::optional<std::string>& value)
{
  if (value)
    std::cerr << (Spine::log_time_str() + ANSI_FG_RED + " [timeseries] Deprecated option '" + name +
                  ANSI_FG_DEFAULT)
              << std::endl;
}

Fmi::ValueFormatterParam valueformatter_params(const Spine::HTTP::Request& req)
{
  Fmi::ValueFormatterParam opt;
  opt.missingText = Spine::optional_string(req.getParameter("missingtext"), "nan");
  opt.floatField = Spine::optional_string(req.getParameter("floatfield"), "fixed");

  if (Spine::optional_string(req.getParameter("format"), "") == "WXML")
    opt.missingText = "NaN";
  return opt;
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief The constructor parses the query string
 */
// ----------------------------------------------------------------------

Query::Query(const State& state, const Spine::HTTP::Request& req, Config& config)
    : ObsQueryParams(req),
      valueformatter(valueformatter_params(req)),
      timeAggregationRequested(false)
{
  try
  {
    report_unsupported_option("adjustfield", req.getParameter("adjustfield"));
    report_unsupported_option("width", req.getParameter("width"));
    report_unsupported_option("fill", req.getParameter("fill"));
    report_unsupported_option("showpos", req.getParameter("showpos"));
    report_unsupported_option("uppercase", req.getParameter("uppercase"));

    language = Spine::optional_string(req.getParameter("lang"), config.defaultLanguage());

    time_t tt = time(nullptr);
    if ((config.itsLastAliasCheck + 10) < tt)
    {
      config.itsAliasFileCollection.checkUpdates(false);
      config.itsLastAliasCheck = tt;
    }

    itsAliasFileCollectionPtr = &config.itsAliasFileCollection;

    forecastSource = Spine::optional_string(req.getParameter("source"), "");

    parse_attr(req);

    if (!parse_grib_loptions(state))
    {
      loptions.reset(
          new Engine::Geonames::LocationOptions(state.getGeoEngine().parseLocations(req)));

      auto lon_coord = Spine::optional_double(req.getParameter("x"), kFloatMissing);
      auto lat_coord = Spine::optional_double(req.getParameter("y"), kFloatMissing);
      auto source_crs = Spine::optional_string(req.getParameter("crs"), "");

      if (lon_coord != kFloatMissing && lat_coord != kFloatMissing && !source_crs.empty())
      {
        // Transform lon_coord, lat_coord to lonlat parameter
        if (source_crs != "ESPG:4326")
        {
          Fmi::CoordinateTransformation transformation(source_crs, "WGS84");
          transformation.transform(lon_coord, lat_coord);
        }
        auto tmpReq = req;
        tmpReq.addParameter("lonlat",
                            (Fmi::to_string(lon_coord) + "," + Fmi::to_string(lat_coord)));
        loptions.reset(
            new Engine::Geonames::LocationOptions(state.getGeoEngine().parseLocations(tmpReq)));
      }
    }

    // Store WKT-geometries
    wktGeometries = state.getGeoEngine().getWktGeometries(*loptions, language);

    toptions = TS::parseTimes(req);

#ifdef MYDEBUG
    std::cout << "Time options: " << std::endl << toptions << std::endl;
#endif

    latestTimestep = toptions.startTime;

    starttimeOptionGiven = (!!req.getParameter("starttime") || !!req.getParameter("now"));
    std::string endtime = Spine::optional_string(req.getParameter("endtime"), "");
    endtimeOptionGiven = (endtime != "now");

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

    maxdistanceOptionGiven = !!req.getParameter("maxdistance");
    maxdistance =
        Spine::optional_string(req.getParameter("maxdistance"), config.defaultMaxDistance());

    keyword = Spine::optional_string(req.getParameter("keyword"), "");

    parse_inkeyword_locations(req, state);

    findnearestvalidpoint = Spine::optional_bool(req.getParameter("findnearestvalid"), false);

    parse_origintime(req);
    parse_producers(req, state);
    parse_parameters(req);

    Query::parse_levels(req);

    // wxml requires 8 parameters

    timestring = Spine::optional_string(req.getParameter("timestring"), "");
    if (format == "wxml")
    {
      timeformat = "xml";
      poptions.add(TS::ParameterFactory::instance().parse("origintime"));
      poptions.add(TS::ParameterFactory::instance().parse("xmltime"));
      poptions.add(TS::ParameterFactory::instance().parse("weekday"));
      poptions.add(TS::ParameterFactory::instance().parse("timestring"));
      poptions.add(TS::ParameterFactory::instance().parse("name"));
      poptions.add(TS::ParameterFactory::instance().parse("geoid"));
      poptions.add(TS::ParameterFactory::instance().parse("longitude"));
      poptions.add(TS::ParameterFactory::instance().parse("latitude"));
    }

    // This must be done after params is no longer being modified
    // by the above wxml code!

    Query::parse_precision(req, config);

    timeformatter.reset(Fmi::TimeFormatter::create(timeformat));

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
    // Stack traces in journals are useless when the user has made a typo
    throw Fmi::Exception::Trace(BCP, "TimeSeries plugin failed to parse query string options!")
        .disableLogging();
  }
}

void Query::parse_producers(const Spine::HTTP::Request& theReq, const State& theState)
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
    const auto obsProducers = getObsProducers(theState);

    for (const auto& p : resultProducers)
    {
      const auto& qEngine = theState.getQEngine();
      bool ok = qEngine.hasProducer(p);
      if (!obsProducers.empty() && !ok)
        ok = (obsProducers.find(p) != obsProducers.end());
      const auto* gridEngine = theState.getGridEngine();
      if (!ok && gridEngine)
        ok = gridEngine->isGridProducer(p);

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

    for (const TS::OptionParsers::ParameterList::value_type& p : poptions.parameters())
    {
      const auto it = prec.parameter_precisions.find(p.name());
      if (it == prec.parameter_precisions.end())
        precisions.push_back(prec.default_precision);  // unknown gets default
      else
        precisions.push_back(it->second);  // known gets configured value
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Query::parse_aggregation_intervals(const Spine::HTTP::Request& theReq)
{
  try
  {
    std::string aggregationIntervalStringBehind =
        Spine::optional_string(theReq.getParameter("interval"), "0m");
    std::string aggregationIntervalStringAhead = ("0m");

    // check if second aggregation interval is defined
    auto agg_pos = aggregationIntervalStringBehind.find(':');
    if (agg_pos != string::npos)
    {
      aggregationIntervalStringAhead = aggregationIntervalStringBehind.substr(agg_pos + 1);
      aggregationIntervalStringBehind.resize(agg_pos);
    }

    int agg_interval_behind = Spine::duration_string_to_minutes(aggregationIntervalStringBehind);
    int agg_interval_ahead = Spine::duration_string_to_minutes(aggregationIntervalStringAhead);

    if (agg_interval_behind < 0 || agg_interval_ahead < 0)
      throw Fmi::Exception(BCP, "The 'interval' option must be positive!");

    // set aggregation interval if it has not been set in parameter parser
    for (const TS::ParameterAndFunctions& paramfuncs : poptions.parameterFunctions())
    {
      set_agg_interval_behind(const_cast<TS::DataFunction&>(paramfuncs.functions.innerFunction),
                              agg_interval_behind);
      set_agg_interval_ahead(const_cast<TS::DataFunction&>(paramfuncs.functions.innerFunction),
                             agg_interval_ahead);
      set_agg_interval_behind(const_cast<TS::DataFunction&>(paramfuncs.functions.outerFunction),
                              agg_interval_behind);
      set_agg_interval_ahead(const_cast<TS::DataFunction&>(paramfuncs.functions.outerFunction),
                             agg_interval_ahead);
    }

    // store maximum aggregation intervals per parameter for later use
    for (const TS::ParameterAndFunctions& paramfuncs : poptions.parameterFunctions())
    {
      std::string paramname(paramfuncs.parameter.name());

      if (maxAggregationIntervals.find(paramname) == maxAggregationIntervals.end())
        maxAggregationIntervals.insert(make_pair(paramname, AggregationInterval(0, 0)));

      set_max_agg_interval_behind(const_cast<TS::DataFunction&>(paramfuncs.functions.innerFunction),
                                  maxAggregationIntervals[paramname].behind);
      set_max_agg_interval_ahead(const_cast<TS::DataFunction&>(paramfuncs.functions.innerFunction),
                                 maxAggregationIntervals[paramname].ahead);
      set_max_agg_interval_behind(const_cast<TS::DataFunction&>(paramfuncs.functions.outerFunction),
                                  maxAggregationIntervals[paramname].behind);
      set_max_agg_interval_ahead(const_cast<TS::DataFunction&>(paramfuncs.functions.outerFunction),
                                 maxAggregationIntervals[paramname].ahead);

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

void Query::parse_parameters(const Spine::HTTP::Request& theReq)
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
          for (const auto& tt : tmp)
            names.push_back(tt);

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

    // Validate and convert
    for (const string& paramname : names)
    {
      try
      {
        TS::ParameterAndFunctions paramfuncs =
            TS::ParameterFactory::instance().parseNameAndFunctions(paramname, true);

        poptions.add(paramfuncs.parameter, paramfuncs.functions);
      }
      catch (...)
      {
        throw Fmi::Exception(BCP, "Parameter parsing failed for '" + paramname + "'!", nullptr);
      }
    }
    poptions.expandParameter("data_source");

    parse_aggregation_intervals(theReq);

    // Make sure parameter names are unique to avoid simple ddos attacks
    std::set<std::string> unique_names;
    std::set<std::string> duplicate_names;
    for (const auto& paramname : names)
    {
      if (unique_names.insert(paramname).second == false)
        duplicate_names.insert(paramname);
    }
    if (!duplicate_names.empty())
#if 1
      std::cerr << Spine::log_time_str() << " Warning: Duplicate parameters in query " + theReq.getQueryString() + " : " +
          boost::algorithm::join(duplicate_names, ",")
                << std::endl;
#else
      throw Fmi::Exception(BCP, "Duplicate parameters in the query")
          .addParameter("Duplicates", boost::algorithm::join(duplicate_names, ","));
#endif
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Query::parse_attr(const Spine::HTTP::Request& theReq)
{
  try
  {
    string attr = Spine::optional_string(theReq.getParameter("attr"), "");
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
        const std::size_t pos = part.find(":");
        if (pos != std::string::npos)
        {
          std::string name = part.substr(0, pos);
          std::string value = part.substr(pos + 1);
          attributeList.addAttribute(name, value);
        }
      }
    }
    // ### Attribute list ( attr=name1:value1,name2:value2,name3:$(alias1) )
    // attributeList.print(std::cout,0,0);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool Query::parse_grib_loptions(const State& state)
{
  try
  {
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
      return true;
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
      return true;
    }

    return false;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Query::parse_inkeyword_locations(const Spine::HTTP::Request& theReq, const State& state)
{
  try
  {
    auto searchName = theReq.getParameterList("inkeyword");
    if (!searchName.empty())
    {
      for (const std::string& key : searchName)
      {
        Locus::QueryOptions opts;
        opts.SetLanguage(language);
        Spine::LocationList places = state.getGeoEngine().keywordSearch(opts, key);
        if (places.empty())
          throw Fmi::Exception(BCP, "No locations for keyword " + std::string(key) + " found");
        inKeywordLocations.insert(inKeywordLocations.end(), places.begin(), places.end());
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Query::parse_origintime(const Spine::HTTP::Request& theReq)
{
  try
  {
    std::optional<std::string> tmp = theReq.getParameter("origintime");
    if (tmp)
    {
      if (*tmp == "latest" || *tmp == "newest")
        origintime = Fmi::DateTime(Fmi::DateTime::POS_INFINITY);
      else if (*tmp == "oldest")
        origintime = Fmi::DateTime(Fmi::DateTime::NEG_INFINITY);
      else
        origintime = Fmi::TimeParser::parse(*tmp);
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
