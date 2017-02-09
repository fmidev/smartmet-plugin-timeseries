// ======================================================================
/*!
 * \brief Utility functions for parsing the request
 */
// ======================================================================

#include "Query.h"
#include "Config.h"
#include "Hash.h"
#include "State.h"

#include <spine/Exception.h>
#include <spine/Convenience.h>
#include <spine/ParameterFactory.h>
#include <engines/geonames/Engine.h>
#include <engines/observation/Engine.h>

#include <macgyver/String.h>
#include <macgyver/TimeParser.h>
#include <newbase/NFmiPoint.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/foreach.hpp>

using namespace std;

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
static const char* default_timezone = "localtime";

// ----------------------------------------------------------------------
/*!
 * \brief The constructor parses the query string
 */
// ----------------------------------------------------------------------

Query::Query(const State& state, const SmartMet::Spine::HTTP::Request& req, Config& config)
    : loptions(new SmartMet::Engine::Geonames::LocationOptions(
          state.getGeoEngine().parseLocations(req))),
      poptions(),
      toptions(SmartMet::Spine::OptionParsers::parseTimes(req)),
      valueformatter(req),
      timeAggregationRequested(false)
{
  try
  {
#ifdef MYDEBUG
    std::cout << "Time options: " << std::endl << toptions << std::endl;
#endif

    latestTimestep = toptions.startTime;

    starttimeOptionGiven = (!!req.getParameter("starttime") || !!req.getParameter("now"));
    std::string endtime = SmartMet::Spine::optional_string(req.getParameter("endtime"), "");
    latestObservation = (endtime == "now");
    endtimeOptionGiven = (endtime != "now");

    allplaces = (SmartMet::Spine::optional_string(req.getParameter("places"), "") == "all");

    timezone = SmartMet::Spine::optional_string(req.getParameter("tz"), default_timezone);

    step = SmartMet::Spine::optional_double(req.getParameter("step"), 1.0);
    leveltype = SmartMet::Spine::optional_string(req.getParameter("leveltype"), "");
    format = SmartMet::Spine::optional_string(req.getParameter("format"), "ascii");

    // Either create the requested locale or use the default one constructed
    // by the Config parser. TODO: If constructing from strings is slow, we should cache locales
    // instead.

    auto opt_locale = req.getParameter("locale");
    if (!opt_locale)
      outlocale = config.defaultLocale();
    else
      outlocale = locale(opt_locale->c_str());

    timeformat = SmartMet::Spine::optional_string(req.getParameter("timeformat"),
                                                  config.defaultTimeFormat());

    language = SmartMet::Spine::optional_string(req.getParameter("lang"), config.defaultLanguage());
    maxdistanceOptionGiven = !!req.getParameter("maxdistance");
    maxdistance = SmartMet::Spine::optional_double(req.getParameter("maxdistance"),
                                                   config.defaultMaxDistance());

    keyword = SmartMet::Spine::optional_string(req.getParameter("keyword"), "");

    findnearestvalidpoint =
        SmartMet::Spine::optional_bool(req.getParameter("findnearestvalid"), false);

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

    Query::parse_producers(req);

    Query::parse_parameters(req, state.getObsEngine());

    Query::parse_levels(req);

    // wxml requires 8 parameters

    timestring = SmartMet::Spine::optional_string(req.getParameter("timestring"), "");
    if (format == "wxml")
    {
      timeformat = "xml";
      poptions.add(SmartMet::Spine::ParameterFactory::instance().parse("origintime"));
      poptions.add(SmartMet::Spine::ParameterFactory::instance().parse("xmltime"));
      poptions.add(SmartMet::Spine::ParameterFactory::instance().parse("weekday"));
      poptions.add(SmartMet::Spine::ParameterFactory::instance().parse("timestring"));
      poptions.add(SmartMet::Spine::ParameterFactory::instance().parse("name"));
      poptions.add(SmartMet::Spine::ParameterFactory::instance().parse("geoid"));
      poptions.add(SmartMet::Spine::ParameterFactory::instance().parse("longitude"));
      poptions.add(SmartMet::Spine::ParameterFactory::instance().parse("latitude"));
    }

    // This must be done after params is no longer being modified
    // by the above wxml code!

    Query::parse_precision(req, config);

    timeformatter.reset(Fmi::TimeFormatter::create(timeformat));

    // observation params
    numberofstations = SmartMet::Spine::optional_int(req.getParameter("numberofstations"), 1);

    auto name = req.getParameter("fmisid");
    if (name)
    {
      const string fmisidreq = *name;
      vector<string> parts;
      boost::algorithm::split(parts, fmisidreq, boost::algorithm::is_any_of(","));

      BOOST_FOREACH (const string& sfmisid, parts)
      {
        int f = Fmi::stoi(sfmisid);
        this->fmisids.push_back(f);
      }
    }

    name = req.getParameter("wmo");
    if (name)
    {
      const string wmoreq = *name;
      vector<string> parts;
      boost::algorithm::split(parts, wmoreq, boost::algorithm::is_any_of(","));

      BOOST_FOREACH (const string& swmo, parts)
      {
        int w = Fmi::stoi(swmo);
        this->wmos.push_back(w);
      }
    }

    name = req.getParameter("lpnn");
    if (name)
    {
      const string lpnnreq = *name;
      vector<string> parts;
      boost::algorithm::split(parts, lpnnreq, boost::algorithm::is_any_of(","));

      BOOST_FOREACH (const string& slpnn, parts)
      {
        int l = Fmi::stoi(slpnn);
        this->lpnns.push_back(l);
      }
    }

    name = req.getParameter("geoid");
    if (name)
    {
      const string geoidreq = *name;
      vector<string> parts;
      boost::algorithm::split(parts, geoidreq, boost::algorithm::is_any_of(","));

      BOOST_FOREACH (const string& sgeoid, parts)
      {
        int g = Fmi::stoi(sgeoid);
        this->geoids.push_back(g);
      }
    }

    name = req.getParameter("geoids");
    if (name)
    {
      const string geoidreq = *name;
      vector<string> parts;
      boost::algorithm::split(parts, geoidreq, boost::algorithm::is_any_of(","));

      BOOST_FOREACH (const string& sgeoid, parts)
      {
        int g = Fmi::stoi(sgeoid);
        this->geoids.push_back(g);
      }
    }

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
        throw SmartMet::Spine::Exception(BCP, "Invalid bounding box '" + bbox + "'!");
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

    if (!!req.getParameter("weekday"))
    {
      const string sweekdays = *req.getParameter("weekday");
      vector<string> parts;
      boost::algorithm::split(parts, sweekdays, boost::algorithm::is_any_of(","));

      BOOST_FOREACH (const string& wday, parts)
      {
        int h = Fmi::stoi(wday);
        this->weekdays.push_back(h);
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse producer/model options
 */
// ----------------------------------------------------------------------

void Query::parse_producers(const SmartMet::Spine::HTTP::Request& theReq)
{
  try
  {
    string opt = SmartMet::Spine::optional_string(theReq.getParameter("model"), "");

    string opt2 = SmartMet::Spine::optional_string(theReq.getParameter("producer"), "");

    if (opt == "default" || opt2 == "default")
    {
      // Use default if it's forced by any option
      return;
    }

    // observation uses stationtype-parameter
    if (opt.empty() && opt2.empty())
      opt2 = SmartMet::Spine::optional_string(theReq.getParameter("stationtype"), "");

    std::list<std::string> firstProducers, secondProducers, resultProducers;

    // Handle time separation:: either 'model' or 'producer' keyword used
    if (!opt.empty())
      boost::algorithm::split(resultProducers, opt, boost::algorithm::is_any_of(";"));
    else if (!opt2.empty())
      boost::algorithm::split(resultProducers, opt2, boost::algorithm::is_any_of(";"));

    // Now split into location parts

    BOOST_FOREACH (const auto& tproducers, resultProducers)
    {
      AreaProducers producers;
      boost::algorithm::split(producers, tproducers, boost::algorithm::is_any_of(","));
      timeproducers.push_back(producers);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief parse Level query
 *
 * Empty result implies all levels are wanted
 */
// ----------------------------------------------------------------------

void Query::parse_levels(const SmartMet::Spine::HTTP::Request& theReq)
{
  try
  {
    // Get the option string

    string opt = SmartMet::Spine::optional_string(theReq.getParameter("level"), "");
    if (!opt.empty())
    {
      levels.insert(Fmi::stoi(opt));
    }

    // Allow also "levels"
    opt = SmartMet::Spine::optional_string(theReq.getParameter("levels"), "");
    if (!opt.empty())
    {
      vector<string> parts;
      boost::algorithm::split(parts, opt, boost::algorithm::is_any_of(","));
      BOOST_FOREACH (const string& tmp, parts)
        levels.insert(Fmi::stoi(tmp));
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse precision options
 */
// ----------------------------------------------------------------------

void Query::parse_precision(const SmartMet::Spine::HTTP::Request& req, const Config& config)
{
  try
  {
    string precname =
        SmartMet::Spine::optional_string(req.getParameter("precision"), config.defaultPrecision());

    const Precision& prec = config.getPrecision(precname);

    precisions.reserve(poptions.size());

    BOOST_FOREACH (const SmartMet::Spine::OptionParsers::ParameterList::value_type& p,
                   poptions.parameters())
    {
      Precision::Map::const_iterator it = prec.parameter_precisions.find(p.name());
      if (it == prec.parameter_precisions.end())
        precisions.push_back(prec.default_precision);  // unknown gets default
      else
        precisions.push_back(it->second);  // known gets configured value
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Query::parse_parameters(const SmartMet::Spine::HTTP::Request& theReq,
                             const SmartMet::Engine::Observation::Engine* theObsEngine)
{
  try
  {
    // Get the option string
    string opt = SmartMet::Spine::required_string(theReq.getParameter("param"),
                                                  "The 'param' option is required!");

    // Protect against empty selection
    if (opt.empty())
      throw SmartMet::Spine::Exception(BCP, "The 'param' option is empty!");

    // Split
    typedef list<string> Names;
    Names names;
    boost::algorithm::split(names, opt, boost::algorithm::is_any_of(","));

    // Determine whether any producer implies observations are needed

    bool obsProducersExist = false;

    if (theObsEngine != nullptr)
    {
      std::set<std::string> obsEngineStationTypes = theObsEngine->getValidStationTypes();
      BOOST_FOREACH (const auto& areaproducers, timeproducers)
      {
        if (obsProducersExist)
          break;

        BOOST_FOREACH (const auto& producer, areaproducers)
        {
          if (obsEngineStationTypes.find(producer) != obsEngineStationTypes.end())
          {
            obsProducersExist = true;
            break;
          }
        }
      }
    }

    // Validate and convert
    BOOST_FOREACH (const string& paramname, names)
    {
      SmartMet::Spine::ParameterAndFunctions paramfuncs =
          SmartMet::Spine::ParameterFactory::instance().parseNameAndFunctions(paramname, true);

      poptions.add(paramfuncs.parameter, paramfuncs.functions);
    }

    std::string aggregationIntervalStringBehind =
        SmartMet::Spine::optional_string(theReq.getParameter("interval"), "0m");
    std::string aggregationIntervalStringAhead = ("0m");

    // check if second aggregation interval is defined
    if (aggregationIntervalStringBehind.find(":") != string::npos)
    {
      aggregationIntervalStringAhead =
          aggregationIntervalStringBehind.substr(aggregationIntervalStringBehind.find(":") + 1);
      aggregationIntervalStringBehind =
          aggregationIntervalStringBehind.substr(0, aggregationIntervalStringBehind.find(":"));
    }

    int agg_interval_behind(
        SmartMet::Spine::duration_string_to_minutes(aggregationIntervalStringBehind));
    int agg_interval_ahead(
        SmartMet::Spine::duration_string_to_minutes(aggregationIntervalStringAhead));

    if (agg_interval_behind < 0 || agg_interval_ahead < 0)
      throw SmartMet::Spine::Exception(BCP, "The 'interval' option must be positive!");

    // set aggregation interval if it has not been set in parameter parser
    BOOST_FOREACH (const SmartMet::Spine::ParameterAndFunctions& paramfuncs,
                   poptions.parameterFunctions())
    {
      if (paramfuncs.functions.innerFunction.getAggregationIntervalBehind() ==
          std::numeric_limits<unsigned int>::max())
        const_cast<SmartMet::Spine::ParameterFunction&>(paramfuncs.functions.innerFunction)
            .setAggregationIntervalBehind(agg_interval_behind);
      if (paramfuncs.functions.innerFunction.getAggregationIntervalAhead() ==
          std::numeric_limits<unsigned int>::max())
        const_cast<SmartMet::Spine::ParameterFunction&>(paramfuncs.functions.innerFunction)
            .setAggregationIntervalAhead(agg_interval_ahead);

      if (paramfuncs.functions.outerFunction.getAggregationIntervalBehind() ==
          std::numeric_limits<unsigned int>::max())
        const_cast<SmartMet::Spine::ParameterFunction&>(paramfuncs.functions.outerFunction)
            .setAggregationIntervalBehind(agg_interval_behind);
      if (paramfuncs.functions.outerFunction.getAggregationIntervalAhead() ==
          std::numeric_limits<unsigned int>::max())
        const_cast<SmartMet::Spine::ParameterFunction&>(paramfuncs.functions.outerFunction)
            .setAggregationIntervalAhead(agg_interval_ahead);
    }

    // store maximum aggregation intervals per parameter for later use
    BOOST_FOREACH (const SmartMet::Spine::ParameterAndFunctions& paramfuncs,
                   poptions.parameterFunctions())
    {
      std::string paramname(paramfuncs.parameter.name());

      if (maxAggregationIntervals.find(paramname) == maxAggregationIntervals.end())
        maxAggregationIntervals.insert(make_pair(paramname, AggregationInterval(0, 0)));

      if (paramfuncs.functions.innerFunction.type() == SmartMet::Spine::FunctionType::TimeFunction)
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
      else if (paramfuncs.functions.outerFunction.type() ==
               SmartMet::Spine::FunctionType::TimeFunction)
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
