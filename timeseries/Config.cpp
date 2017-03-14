// ======================================================================
/*!
 * \brief Implementation of Config
 */
// ======================================================================

#include "Config.h"
#include "Precision.h"
#include <spine/Exception.h>
#include <macgyver/StringConversion.h>
#include <stdexcept>
#include <boost/foreach.hpp>

using namespace std;

static const char* default_url = "/timeseries";
static const char* default_language = "fi";
static const char* default_locale = "fi_FI";
static const char* default_timeformat = "iso";
static const char* default_postgis_client_encoding = "latin1";

static double default_maxdistance = 60.0;  // km

static unsigned int default_expires = 60;  // seconds

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
// ----------------------------------------------------------------------
/*!
 * \brief Add default precisions if none were configured
 */
// ----------------------------------------------------------------------

void Config::add_default_precisions()
{
  try
  {
    Precision prec;
    itsPrecisions.insert(Precisions::value_type("double", prec));
    itsDefaultPrecision = "double";
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse a parameter function setting
 */
// ----------------------------------------------------------------------

SmartMet::Spine::FunctionId get_function_id(const string& configName)
{
  try
  {
    if (configName == "mean")
    {
      return SmartMet::Spine::FunctionId::Mean;
    }
    else if (configName == "max")
    {
      return SmartMet::Spine::FunctionId::Maximum;
    }
    else if (configName == "min")
    {
      return SmartMet::Spine::FunctionId::Minimum;
    }
    else if (configName == "median")
    {
      return SmartMet::Spine::FunctionId::Median;
    }
    else if (configName == "sum")
    {
      return SmartMet::Spine::FunctionId::Sum;
    }
    else if (configName == "sdev")
    {
      return SmartMet::Spine::FunctionId::StandardDeviation;
    }
    else if (configName == "trend")
    {
      return SmartMet::Spine::FunctionId::Trend;
    }
    else if (configName == "change")
    {
      return SmartMet::Spine::FunctionId::Change;
    }
    else if (configName == "count")
    {
      return SmartMet::Spine::FunctionId::Count;
    }
    else if (configName == "percentage")
    {
      return SmartMet::Spine::FunctionId::Percentage;
    }

    return SmartMet::Spine::FunctionId::NullFunction;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse a named precision setting
 */
// ----------------------------------------------------------------------

void Config::parse_config_precision(const string& name)
{
  try
  {
    string optname = "precision." + name;

    if (!itsConfig.exists(optname))
      throw SmartMet::Spine::Exception(BCP,
                                       string("Precision settings for ") + name +
                                           " are missing from pointforecast configuration");

    libconfig::Setting& settings = itsConfig.lookup(optname);

    if (!settings.isGroup())
      throw SmartMet::Spine::Exception(
          BCP,
          "Precision settings for point forecasts must be stored in groups delimited by {}: line " +
              Fmi::to_string(settings.getSourceLine()));

    Precision prec;

    for (int i = 0; i < settings.getLength(); ++i)
    {
      string paramname = settings[i].getName();

      try
      {
        int value = settings[i];
        if (value < 0)
          throw SmartMet::Spine::Exception(
              BCP,
              "Precision settings must be nonnegative in pointforecast configuration for " +
                  string(name) + "." + paramname);

        if (paramname == "default")
          prec.default_precision = value;
        else
          prec.parameter_precisions.insert(Precision::Map::value_type(paramname, value));
      }
      catch (libconfig::ParseException& e)
      {
        throw SmartMet::Spine::Exception(BCP,
                                         string("TimeSeries configuration error ' ") +
                                             e.getError() + "' with variable '" + paramname +
                                             "' on line " + Fmi::to_string(e.getLine()));
      }
      catch (libconfig::ConfigException&)
      {
        throw SmartMet::Spine::Exception(BCP,
                                         string("TimeSeries configuration error with variable '") +
                                             paramname + "' on line " +
                                             Fmi::to_string(settings[i].getSourceLine()));
      }
      catch (std::exception& e)
      {
        throw SmartMet::Spine::Exception(BCP,
                                         e.what() + string(" (line number ") +
                                             Fmi::to_string(settings[i].getSourceLine()) + ")");
      }
    }

    // This line may crash if prec.parameters is empty
    // Looks like a bug in g++

    itsPrecisions.insert(Precisions::value_type(name, prec));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse precisions listed in precision.enabled
 */
// ----------------------------------------------------------------------

void Config::parse_config_precisions()
{
  try
  {
    if (!itsConfig.exists("precision"))
      add_default_precisions();  // set sensible defaults
    else
    {
      // Require available precisions in

      if (!itsConfig.exists("precision.enabled"))
        throw SmartMet::Spine::Exception(
            BCP, "precision.enabled missing from pointforecast congiguration file");

      libconfig::Setting& enabled = itsConfig.lookup("precision.enabled");
      if (!enabled.isArray())
      {
        throw SmartMet::Spine::Exception(
            BCP,
            "precision.enabled must be an array in pointforecast configuration file line " +
                Fmi::to_string(enabled.getSourceLine()));
      }

      for (int i = 0; i < enabled.getLength(); ++i)
      {
        const char* name = enabled[i];
        if (i == 0)
          itsDefaultPrecision = name;
        parse_config_precision(name);
      }

      if (itsPrecisions.empty())
        throw SmartMet::Spine::Exception(
            BCP, "No precisions defined in pointforecast precision: datablock!");
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

string parse_config_key(const char* str1 = 0, const char* str2 = 0, const char* str3 = 0)
{
  try
  {
    string string1(str1 ? str1 : "");
    string string2(str2 ? str2 : "");
    string string3(str3 ? str3 : "");

    string retval(string1 + string2 + string3);

    return retval;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Constructor
 */
// ----------------------------------------------------------------------

Config::Config()
    : itsDefaultPrecision("normal"),
      itsDefaultLanguage(default_language),
      itsDefaultLocaleName(default_locale),
      itsDefaultLocale(std::locale(default_locale)),
      itsDefaultTimeFormat(default_timeformat),
      itsDefaultUrl(default_url),
      itsDefaultMaxDistance(default_maxdistance),
      itsExpirationTime(default_expires),
      itsFormatterOptions(),
      itsPrecisions(),
      itsObsEngineDisabled(false),
      itsFilesystemCacheDirectory("/var/smartmet/timeseriescache"),
      itsMaxMemoryCacheSize(104857600)  // 100 MB
      ,
      itsMaxFilesystemCacheSize(209715200)  // 200 MB
      ,
      itsMaxTimeSeriesCacheSize(10000)
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Constructor
 */
// ----------------------------------------------------------------------

Config::Config(const string& configfile)
    : itsDefaultPrecision("normal"),
      itsDefaultLanguage(default_language),
      itsDefaultLocaleName(default_locale),
      itsDefaultTimeFormat(default_timeformat),
      itsDefaultUrl(default_url),
      itsDefaultMaxDistance(default_maxdistance),
      itsExpirationTime(default_expires),
      itsFormatterOptions(),
      itsPrecisions(),
      itsObsEngineDisabled(false),
      itsFilesystemCacheDirectory("/var/smartmet/timeseriescache"),
      itsMaxMemoryCacheSize(104857600)  // 100 MB
      ,
      itsMaxFilesystemCacheSize(209715200)  // 200 MB
      ,
      itsMaxTimeSeriesCacheSize(10000)
{
  try
  {
    if (configfile.empty())
    {
      add_default_precisions();  // set sensible defaults
    }
    else
    {
      itsConfig.readFile(configfile.c_str());

      itsConfig.lookupValue("timeformat", itsDefaultTimeFormat);
      itsConfig.lookupValue("language", itsDefaultLanguage);
      itsConfig.lookupValue("locale", itsDefaultLocaleName);
      itsConfig.lookupValue("url", itsDefaultUrl);
      itsConfig.lookupValue("maxdistance", itsDefaultMaxDistance);
      itsConfig.lookupValue("expires", itsExpirationTime);
      itsConfig.lookupValue("observation_disabled", itsObsEngineDisabled);

      itsConfig.lookupValue("cache.memory_bytes", itsMaxMemoryCacheSize);
      itsConfig.lookupValue("cache.filesystem_bytes", itsMaxFilesystemCacheSize);
      itsConfig.lookupValue("cache.directory", itsFilesystemCacheDirectory);
      itsConfig.lookupValue("cache.timeseries_size", itsMaxTimeSeriesCacheSize);

      itsFormatterOptions = SmartMet::Spine::TableFormatterOptions(itsConfig);

      parse_config_precisions();

      // PostGIS
      if (!itsConfig.exists("postgis"))
        itsDisabled = true;
      else
        itsConfig.lookupValue("postgis.disabled", itsDisabled);

      if (!itsDisabled)
      {
        if (!itsConfig.exists("postgis.default"))
        {
          throw SmartMet::Spine::Exception(
              BCP, "PostGIS configuration error: postgis.default-section missing!");
        }

        SmartMet::Spine::postgis_identifier postgis_default_identifier;
        postgis_default_identifier.postGISClientEncoding = default_postgis_client_encoding;
        itsConfig.lookupValue("postgis.default.host", postgis_default_identifier.postGISHost);
        itsConfig.lookupValue("postgis.default.port", postgis_default_identifier.postGISPort);
        itsConfig.lookupValue("postgis.default.database",
                              postgis_default_identifier.postGISDatabase);
        itsConfig.lookupValue("postgis.default.username",
                              postgis_default_identifier.postGISUsername);
        itsConfig.lookupValue("postgis.default.password",
                              postgis_default_identifier.postGISPassword);
        itsConfig.lookupValue("postgis.default.client_encoding",
                              postgis_default_identifier.postGISClientEncoding);
        itsConfig.lookupValue("postgis.default.schema", postgis_default_identifier.postGISSchema);
        itsConfig.lookupValue("postgis.default.table", postgis_default_identifier.postGISTable);
        itsConfig.lookupValue("postgis.default.field", postgis_default_identifier.postGISField);
        std::string postgis_identifier_key(postgis_default_identifier.key());
        itsDefaultPostGISIdentifierKey = postgis_identifier_key;
        postgis_identifiers.insert(make_pair(postgis_identifier_key, postgis_default_identifier));

        if (itsConfig.exists("postgis.config_items"))
        {
          libconfig::Setting& config_items = itsConfig.lookup("postgis.config_items");

          if (!config_items.isArray())
          {
            throw SmartMet::Spine::Exception(
                BCP,
                "postgis.config_items not an array in areaforecastplugin configuration file line " +
                    Fmi::to_string(config_items.getSourceLine()));
            ;
          }

          for (int i = 0; i < config_items.getLength(); ++i)
          {
            if (!itsConfig.exists(parse_config_key("postgis.", config_items[i])))
              throw SmartMet::Spine::Exception(
                  BCP,
                  parse_config_key("postgis.", config_items[i]) +
                      " -section does not exists in configuration file");

            SmartMet::Spine::postgis_identifier postgis_id(
                postgis_identifiers[itsDefaultPostGISIdentifierKey]);

            itsConfig.lookupValue(parse_config_key("postgis.", config_items[i], ".host").c_str(),
                                  postgis_id.postGISHost);
            itsConfig.lookupValue(parse_config_key("postgis.", config_items[i], ".port").c_str(),
                                  postgis_id.postGISPort);
            itsConfig.lookupValue(
                parse_config_key("postgis.", config_items[i], ".database").c_str(),
                postgis_id.postGISDatabase);
            itsConfig.lookupValue(
                parse_config_key("postgis.", config_items[i], ".username").c_str(),
                postgis_id.postGISUsername);
            itsConfig.lookupValue(
                parse_config_key("postgis.", config_items[i], ".password").c_str(),
                postgis_id.postGISPassword);
            itsConfig.lookupValue(
                parse_config_key("postgis.", config_items[i], ".client_encoding").c_str(),
                postgis_id.postGISClientEncoding);
            itsConfig.lookupValue(parse_config_key("postgis.", config_items[i], ".schema").c_str(),
                                  postgis_id.postGISSchema);
            itsConfig.lookupValue(parse_config_key("postgis.", config_items[i], ".table").c_str(),
                                  postgis_id.postGISTable);
            itsConfig.lookupValue(parse_config_key("postgis.", config_items[i], ".field").c_str(),
                                  postgis_id.postGISField);

            std::string key(postgis_id.key());
            if (postgis_identifiers.find(key) == postgis_identifiers.end())
              postgis_identifiers.insert(make_pair(postgis_id.key(), postgis_id));
          }
        }
      }
    }

    // We construct the default locale only once from the string,
    // creating it from scratch is more expensive.

    itsDefaultLocale = std::locale(itsDefaultLocaleName.c_str());
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the precision settings for the given name
 */
// ----------------------------------------------------------------------

const Precision& Config::getPrecision(const string& name) const
{
  try
  {
    Precisions::const_iterator p = itsPrecisions.find(name);
    if (p != itsPrecisions.end())
      return p->second;

    throw SmartMet::Spine::Exception(BCP, "Unknown precision '" + name + "'!");
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

vector<std::string> Config::getPostGISIdentifierKeys()
{
  try
  {
    vector<std::string> keys;

    for (std::map<string, SmartMet::Spine::postgis_identifier>::const_iterator it =
             postgis_identifiers.begin();
         it != postgis_identifiers.end();
         it++)
    {
      keys.push_back(it->first);
    }

    return keys;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

const SmartMet::Spine::postgis_identifier& Config::getPostGISIdentifier(const std::string& key)
{
  try
  {
    if (postgis_identifiers.find(key) == postgis_identifiers.end())
      throw SmartMet::Spine::Exception(
          BCP,
          "Config::getPostGISIdentifier(const std::string& key): parameter key '" + key +
              "' not found");

    return postgis_identifiers[key];
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

const SmartMet::Spine::postgis_identifier& Config::getDefaultPostGISIdentifier()
{
  try
  {
    return postgis_identifiers[itsDefaultPostGISIdentifierKey];
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

const std::string& Config::filesystemCacheDirectory() const
{
  return itsFilesystemCacheDirectory;
}

unsigned long long Config::maxMemoryCacheSize() const
{
  return itsMaxMemoryCacheSize;
}

unsigned long long Config::maxFilesystemCacheSize() const
{
  return itsMaxFilesystemCacheSize;
}

unsigned long long Config::maxTimeSeriesCacheSize() const
{
  return itsMaxTimeSeriesCacheSize;
}

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
