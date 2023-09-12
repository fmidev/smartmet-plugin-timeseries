// ======================================================================
/*!
 * \brief SmartMet TimeSeries plugin implementation
 */
// ======================================================================

#include "Plugin.h"
#include "QueryProcessingHub.h"
#include "UtilityFunctions.h"
#include "State.h"
#include <spine/Convenience.h>
#include <timeseries/ParameterKeywords.h>
#include <spine/FmiApiKey.h>
#include <spine/HostInfo.h>
#include <spine/ImageFormatter.h>
#include <spine/TableFormatterFactory.h>
#include <macgyver/Hash.h>
#include <fmt/format.h>
#include <engines/gis/Engine.h>

//#define MYDEBUG ON

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
namespace
{
Spine::TableFormatter* get_formatter_and_qstreamer(const Query& q, QueryServer::QueryStreamer_sptr& queryStreamer)
{  
  try
	{
	  if (strcasecmp(q.format.c_str(), "IMAGE") == 0)
		return new Spine::ImageFormatter();
	  
	  if (strcasecmp(q.format.c_str(), "FILE") == 0)
		{
		  auto* qStreamer = new QueryServer::QueryStreamer();
		  queryStreamer.reset(qStreamer);
		  return new Spine::ImageFormatter();
		}
	  
	  if (strcasecmp(q.format.c_str(), "INFO") == 0)
		return Spine::TableFormatterFactory::create("debug");
	  
	  return Spine::TableFormatterFactory::create(q.format);
	  
	}
  catch (...)
	{
	  throw Fmi::Exception::Trace(BCP, "Operation failed!");
	}
}

Spine::TableFormatter::Names get_headers(const std::vector<Spine::Parameter>& parameters)
{  
  try
	{
	  // The names of the columns
	  Spine::TableFormatter::Names headers;
	  
	  for (const Spine::Parameter& p : parameters)
		{
		  std::string header_name = p.alias();
		  std::vector<std::string> partList;
		  splitString(header_name, ':', partList);
		  // There was a merge conflict in here at one time. GRIB branch processed
		  // these two special cases, while master added the sensor part below. This
		  // may be incorrect.
		  if (partList.size() > 2 && (partList[0] == "ISOBANDS" || partList[0] == "ISOLINES"))
			{
			  const char* param_header =
				header_name.c_str() + partList[0].size() + partList[1].size() + 2;
			  headers.push_back(param_header);
			}
		  else
			{
			  const boost::optional<int>& sensor_no = p.getSensorNumber();
			  if (sensor_no)
				{
				  header_name += ("_#" + Fmi::to_string(*sensor_no));
				}
			  if (!p.getSensorParameter().empty())
				header_name += ("_" + p.getSensorParameter());
			  headers.push_back(header_name);
			}
		}
	  
	  return headers;
	}
  catch (...)
	{
	  throw Fmi::Exception::Trace(BCP, "Operation failed!");
	}
}

std::string get_wxml_type(const std::string& producer_option, const std::set<std::string>& obs_station_types)
{  
  try
	{
	  // If query is fast, we do not not have observation producers
	  // This means we put 'forecast' into wxml-tag
	  std::string wxml_type = "forecast";
	  
	  for (const auto& obsProducer : obs_station_types)
		{
		  if (boost::algorithm::contains(producer_option, obsProducer))
			{
			  // Observation mentioned, use 'observation' wxml type
			  wxml_type = "observation";
			  break;
			}
		}
	  
	  return wxml_type;
	}
  catch (...)
	{
	  throw Fmi::Exception::Trace(BCP, "Operation failed!");
	}
}

bool etag_only(const Spine::HTTP::Request& request, Spine::HTTP::Response& response, std::size_t product_hash)
{  
  try
	{
	  if (product_hash != Fmi::bad_hash)
		{
		  response.setHeader("ETag", fmt::format("\"{:x}-timeseries\"", product_hash));
		  
		  // If the product is cacheable and etag was requested, respond with etag only
		  
		  if (request.getHeader("X-Request-ETag"))
			{
			  response.setStatus(Spine::HTTP::Status::no_content);
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

void parse_lonlats(const boost::optional<std::string>& lonlats,
				   Spine::HTTP::Request& theRequest,
				   std::string& wkt_multipoint)
{
  try
  {
	if(!lonlats)
	  return;

	theRequest.removeParameter("lonlat");
	theRequest.removeParameter("lonlats");
	std::vector<std::string> parts;
	boost::algorithm::split(parts, *lonlats, boost::algorithm::is_any_of(","));
	if (parts.size() % 2 != 0)
	  throw Fmi::Exception(BCP, "Invalid lonlats list: " + *lonlats);
	
	for (unsigned int j = 0; j < parts.size(); j += 2)
	  {
		if (wkt_multipoint != "MULTIPOINT(")
		  wkt_multipoint += ",";
		wkt_multipoint += "(" + parts[j] + " " + parts[j + 1] + ")";
	  }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void parse_latlons(const boost::optional<std::string>& latlons,
				   Spine::HTTP::Request& theRequest,
				   std::string& wkt_multipoint)
{
  try
  {
	if(!latlons)
	  return;

	theRequest.removeParameter("latlon");
	theRequest.removeParameter("latlons");
	std::vector<std::string> parts;
	boost::algorithm::split(parts, *latlons, boost::algorithm::is_any_of(","));
	if (parts.size() % 2 != 0)
	  throw Fmi::Exception(BCP, "Invalid latlons list: " + *latlons);
	
	for (unsigned int j = 0; j < parts.size(); j += 2)
      {
        if (wkt_multipoint != "MULTIPOINT(")
          wkt_multipoint += ",";
        wkt_multipoint += "(" + parts[j + 1] + " " + parts[j] + ")";
      }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void parse_places(const boost::optional<std::string>& places,
				  Spine::HTTP::Request& theRequest,
				  std::string& wkt_multipoint,
				  const Engine::Geonames::Engine* geoEngine)
{
  try
  {
	if(!places)
	  return;

      theRequest.removeParameter("places");
      std::vector<std::string> parts;
      boost::algorithm::split(parts, *places, boost::algorithm::is_any_of(","));
      for (const auto& place : parts)
      {
        Spine::LocationPtr loc = geoEngine->nameSearch(place, "fi");
        if (loc)
        {
          if (wkt_multipoint != "MULTIPOINT(")
            wkt_multipoint += ",";
          wkt_multipoint +=
              "(" + Fmi::to_string(loc->longitude) + " " + Fmi::to_string(loc->latitude) + ")";
        }
      }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void parse_fmisids(const boost::optional<std::string>& fmisid,
				   Spine::HTTP::Request& theRequest,
				   std::vector<int>& fmisids)
{
  try
  {
	if (!fmisid)
	  return;

	theRequest.removeParameter("fmisid");
	std::vector<std::string> parts;
	boost::algorithm::split(parts, *fmisid, boost::algorithm::is_any_of(","));
	for (const auto& id : parts)
	  fmisids.push_back(Fmi::stoi(id));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void parse_lpnns(const boost::optional<std::string>& lpnn,
				 Spine::HTTP::Request& theRequest,
				 std::vector<int>& lpnns)
{
  try
  {
	if (!lpnn)
	  return;

	theRequest.removeParameter("lpnn");
	std::vector<std::string> parts;
	boost::algorithm::split(parts, *lpnn, boost::algorithm::is_any_of(","));
	for (const auto& id : parts)
	  lpnns.push_back(Fmi::stoi(id));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void parse_wmos(const boost::optional<std::string>& wmo,
				Spine::HTTP::Request& theRequest,
				std::vector<int>& wmos)
{
  try
  {
	if (!wmo)
	  return;

	theRequest.removeParameter("wmo");
	std::vector<std::string> parts;
	boost::algorithm::split(parts, *wmo, boost::algorithm::is_any_of(","));
	for (const auto& id : parts)
	  wmos.push_back(Fmi::stoi(id));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

} // anonymous


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
    Query q(state, request, itsConfig);

    // Resolve locations for FMISDs,WMOs,LPNNs (https://jira.fmi.fi/browse/BRAINSTORM-1848)
    Engine::Geonames::LocationOptions lopt =
        itsEngines.geoEngine->parseLocations(q.fmisids, q.lpnns, q.wmos, q.language);

    const Spine::TaggedLocationList& locations = lopt.locations();
    Spine::TaggedLocationList tagged_ll = q.loptions->locations();
    tagged_ll.insert(tagged_ll.end(), locations.begin(), locations.end());
    q.loptions->setLocations(tagged_ll);

    high_resolution_clock::time_point t2 = high_resolution_clock::now();

    data.setPaging(q.startrow, q.maxresults);

    std::string producer_option =
        Spine::optional_string(request.getParameter(PRODUCER_PARAM),
                               Spine::optional_string(request.getParameter(STATIONTYPE_PARAM), ""));
    boost::algorithm::to_lower(producer_option);
    // At least one of location specifiers must be set

#ifndef WITHOUT_OBSERVATION
    if (q.fmisids.empty() && q.lpnns.empty() && q.wmos.empty() && q.boundingBox.empty() &&
        !UtilityFunctions::is_flash_or_mobile_producer(producer_option) && q.loptions->locations().empty())
#else
    if (q.loptions->locations().empty())
#endif
      throw Fmi::Exception(BCP, "No location option given!").disableLogging();

    QueryServer::QueryStreamer_sptr queryStreamer;
    // The formatter knows which mimetype to send
	Spine::TableFormatter* fmt = get_formatter_and_qstreamer(q, queryStreamer);

    bool gridEnabled = false;
    if (strcasecmp(q.forecastSource.c_str(), "grid") == 0 ||
        (q.forecastSource.length() == 0 &&
         strcasecmp(itsConfig.primaryForecastSource().c_str(), "grid") == 0))
      gridEnabled = true;

    boost::shared_ptr<Spine::TableFormatter> formatter(fmt);
    std::string mime = formatter->mimetype() + "; charset=UTF-8";
    response.setHeader("Content-Type", mime);

    // Calculate the hash value for the product.

    std::size_t product_hash = Fmi::bad_hash;
	
	QueryProcessingHub qph(*this);

    try
    {
      product_hash = qph.hash_value(state, request, q);
    }
    catch (...)
    {
      if (!gridEnabled)
        throw Fmi::Exception::Trace(BCP, "Operation failed!");
    }


    high_resolution_clock::time_point t3 = high_resolution_clock::now();

    std::string timeheader = Fmi::to_string(duration_cast<microseconds>(t2 - t1).count()) + '+' +
                             Fmi::to_string(duration_cast<microseconds>(t3 - t2).count());


	if(etag_only(request, response, product_hash))
	   return;

    // If obj is not nullptr it is from cache
    auto obj = qph.processQuery(state, data, q, queryStreamer, product_hash);

    if (obj)
    {
      response.setHeader("X-Duration", timeheader);
      response.setHeader("X-TimeSeries-Cache", "yes");
      response.setContent(obj);
      return;
    }

    // Must generate the result from scratch
    response.setHeader("X-TimeSeries-Cache", "no");

    high_resolution_clock::time_point t4 = high_resolution_clock::now();
    timeheader.append("+").append(
        Fmi::to_string(std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3).count()));

    data.setMissingText(q.valueformatter.missing());

    // The names of the columns
	Spine::TableFormatter::Names headers = get_headers(q.poptions.parameters());

    // Format product
    // Deduce WXML-tag from used producer. (What happens when we combine forecasts and
    // observations??).

	std::string wxml_type = get_wxml_type(producer_option, itsObsEngineStationTypes);

    auto formatter_options = itsConfig.formatterOptions();
    formatter_options.setFormatType(wxml_type);

    auto out = formatter->format(data, headers, request, formatter_options);
    high_resolution_clock::time_point t5 = high_resolution_clock::now();
    timeheader.append("+").append(
        Fmi::to_string(std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4).count()));

    // TODO: Should use std::move when it has become available
    boost::shared_ptr<std::string> result(new std::string());
    std::swap(out, *result);

    // Too many flash data requests with empty output filling the logs...
#if 0    
    if (result->empty())
    {
      std::cerr << "Warning: Empty output for request " << request.getQueryString() << " from "
                << request.getClientIP() << std::endl;
    }
#endif

#ifdef MYDEBUG
    std::cout << "Output:" << std::endl << *result << std::endl;
#endif

    response.setHeader("X-Duration", timeheader);

    if (strcasecmp(q.format.c_str(), "FILE") == 0)
    {
      std::string filename =
          "attachement; filename=timeseries_" + std::to_string(getTime()) + ".grib";
      response.setHeader("Content-type", "application/octet-stream");
      response.setHeader("Content-Disposition", filename);
      response.setContent(queryStreamer);
    }
    else
    {
      response.setContent(*result);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Plugin::grouplocations(Spine::HTTP::Request& theRequest)
{
  try
  {
    auto lonlats = theRequest.getParameter("lonlats");
    if (!lonlats)
      lonlats = theRequest.getParameter("lonlat");
    auto latlons = theRequest.getParameter("latlons");
    if (!latlons)
      latlons = theRequest.getParameter("latlon");
    auto places = theRequest.getParameter("places");
    auto fmisid = theRequest.getParameter("fmisid");
    auto lpnn = theRequest.getParameter("lpnn");
    auto wmo = theRequest.getParameter("wmo");

    std::string wkt_multipoint = "MULTIPOINT(";
	parse_lonlats(lonlats, theRequest, wkt_multipoint);
	parse_latlons(latlons, theRequest, wkt_multipoint);
	parse_places(places, theRequest, wkt_multipoint, itsEngines.geoEngine);

    std::vector<int> fmisids;
    std::vector<int> lpnns;
    std::vector<int> wmos;

	parse_fmisids(fmisid, theRequest, fmisids);
	parse_lpnns(lpnn, theRequest, lpnns);
	parse_wmos(wmo, theRequest, wmos);

    Engine::Geonames::LocationOptions lopts =
        itsEngines.geoEngine->parseLocations(fmisids, lpnns, wmos, "fi");
    for (const auto& lopt : lopts.locations())
    {
      if (lopt.loc)
      {
        if (wkt_multipoint != "MULTIPOINT(")
          wkt_multipoint += ",";
        wkt_multipoint += "(" + Fmi::to_string(lopt.loc->longitude) + " " +
                          Fmi::to_string(lopt.loc->latitude) + ")";
      }
    }

    wkt_multipoint += ")";
    theRequest.addParameter("wkt", wkt_multipoint);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Main content handler
 */
// ----------------------------------------------------------------------

void Plugin::requestHandler(Spine::Reactor& /* theReactor */,
                            const Spine::HTTP::Request& theRequest,
                            Spine::HTTP::Response& theResponse)
{
  // We need this in the catch block
  bool isdebug = false;

  try
  {
    // Check request method (support GET, OPTIONS)
    if (checkRequest(theRequest, theResponse, false))
    {
      return;
    }

    if (Spine::optional_bool(theRequest.getParameter("grouplocations"), false))
      grouplocations(const_cast<Spine::HTTP::Request&>(theRequest));

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
      theResponse.setHeader("Cache-Control", cachecontrol);

      boost::posix_time::ptime t_expires =
          state.getTime() + boost::posix_time::seconds(expires_seconds);
      std::string expiration = tformat->format(t_expires);
      theResponse.setHeader("Expires", expiration);
    }

    std::string modification = tformat->format(state.getTime());
    theResponse.setHeader("Last-Modified", modification);
  }
  catch (...)
  {
    // Catching all exceptions

    Fmi::Exception ex(BCP, "Request processing exception!", nullptr);
    ex.addParameter("URI", theRequest.getURI());
    ex.addParameter("ClientIP", theRequest.getClientIP());
    ex.addParameter("HostName", Spine::HostInfo::getHostName(theRequest.getClientIP()));

    const bool check_token = true;
    auto apikey = Spine::FmiApiKey::getFmiApiKey(theRequest, check_token);
    ex.addParameter("Apikey", (apikey ? *apikey : std::string("-")));

    ex.printError();

    std::string firstMessage = ex.what();
    if (isdebug)
    {
      // Delivering the exception information as HTTP content
      std::string fullMessage = ex.getHtmlStackTrace();
      theResponse.setContent(fullMessage);
      theResponse.setStatus(Spine::HTTP::Status::ok);
    }
    else
    {
      if (firstMessage.find("timeout") != std::string::npos)
        theResponse.setStatus(Spine::HTTP::Status::request_timeout);
      else
        theResponse.setStatus(Spine::HTTP::Status::bad_request);
    }

    // Adding the first exception information into the response header
    boost::algorithm::replace_all(firstMessage, "\n", " ");
    if (firstMessage.size() > 300)
      firstMessage.resize(300);
    theResponse.setHeader("X-TimeSeriesPlugin-Error", firstMessage);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Plugin constructor
 */
// ----------------------------------------------------------------------

Plugin::Plugin(Spine::Reactor* theReactor, const char* theConfig)
    : itsModuleName("TimeSeries"), itsConfig(theConfig), itsReactor(theReactor)
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize in a separate thread due to PostGIS slowness
 */
// ----------------------------------------------------------------------

void Plugin::init()
{
  using namespace boost::placeholders;

  try
  {
    // Time series cache
    itsTimeSeriesCache.reset(new TS::TimeSeriesGeneratorCache);
    itsTimeSeriesCache->resize(itsConfig.maxTimeSeriesCacheSize());

    /* GeoEngine */
    auto* engine = itsReactor->getSingleton("Geonames", nullptr);
    if (!engine)
      throw Fmi::Exception(BCP, "Geonames engine unavailable");
    itsEngines.geoEngine = reinterpret_cast<Engine::Geonames::Engine*>(engine);

    /* GisEngine */
    engine = itsReactor->getSingleton("Gis", nullptr);
    if (!engine)
      throw Fmi::Exception(BCP, "Gis engine unavailable");
    itsEngines.gisEngine = reinterpret_cast<Engine::Gis::Engine*>(engine);

    // Read the geometries from PostGIS database
    itsEngines.gisEngine->populateGeometryStorage(itsConfig.getPostGISIdentifiers(), itsGeometryStorage);

    /* QEngine */
    engine = itsReactor->getSingleton("Querydata", nullptr);
    if (!engine)
      throw Fmi::Exception(BCP, "Querydata engine unavailable");
    itsEngines.qEngine = reinterpret_cast<Engine::Querydata::Engine*>(engine);

    /* GridEngine */
    if (!itsConfig.gridEngineDisabled())
    {
      engine = itsReactor->getSingleton("grid", nullptr);
      if (!engine)
        throw Fmi::Exception(BCP, "The 'grid-engine' unavailable!");

      itsEngines.gridEngine = reinterpret_cast<Engine::Grid::Engine*>(engine);
      itsEngines.gridEngine->setDem(itsEngines.geoEngine->dem());
      itsEngines.gridEngine->setLandCover(itsEngines.geoEngine->landCover());
    }

#ifndef WITHOUT_OBSERVATION
    if (!itsConfig.obsEngineDisabled())
    {
      /* ObsEngine */
      engine = itsReactor->getSingleton("Observation", nullptr);
      if (!engine)
        throw Fmi::Exception(BCP, "Observation engine unavailable");
      itsEngines.obsEngine = reinterpret_cast<Engine::Observation::Engine*>(engine);

      // fetch obsebgine station types (producers)
      itsObsEngineStationTypes = itsEngines.obsEngine->getValidStationTypes();
    }
#endif

    // Initialization done, register services. We are aware that throwing
    // from a separate thread will cause a crash, but these should never
    // fail.
    if (!itsReactor->addContentHandler(this,
                                       itsConfig.defaultUrl(),
                                       [this](Spine::Reactor& theReactor,
                                              const Spine::HTTP::Request& theRequest,
                                              Spine::HTTP::Response& theResponse) {
                                         callRequestHandler(theReactor, theRequest, theResponse);
                                       }))
      throw Fmi::Exception(BCP, "Failed to register timeseries content handler");

    // DEPRECATED:

    if (!itsReactor->addContentHandler(this,
                                       "/pointforecast",
                                       [this](Spine::Reactor& theReactor,
                                              const Spine::HTTP::Request& theRequest,
                                              Spine::HTTP::Response& theResponse) {
                                         callRequestHandler(theReactor, theRequest, theResponse);
                                       }))
      throw Fmi::Exception(BCP, "Failed to register pointforecast content handler");

    // DONE
    itsReady = true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Destructor
 */
// ----------------------------------------------------------------------

Plugin::~Plugin() = default;

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

Fmi::Cache::CacheStatistics Plugin::getCacheStats() const
{
  Fmi::Cache::CacheStatistics ret;

  ret.insert(std::make_pair("Timeseries::timeseries_generator_cache",
                            itsTimeSeriesCache->getCacheStats()));

  return ret;
}

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
