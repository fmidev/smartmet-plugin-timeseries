// ======================================================================
/*!
 * \brief Implementation of Config
 */
// ======================================================================

#include "Config.h"
#include "Precision.h"
#include <boost/foreach.hpp>
#include <macgyver/StringConversion.h>
#include <macgyver/Exception.h>
#include <stdexcept>

using namespace std;

const char* default_url = "/timeseries";
const char* default_timeformat = "iso";

double default_maxdistance = 60.0;  // km

unsigned int default_expires = 60;  // seconds

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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse a parameter function setting
 */
// ----------------------------------------------------------------------

Spine::FunctionId get_function_id(const string& configName)
{
  try
  {
    if (configName == "mean")
    {
      return Spine::FunctionId::Mean;
    }
    else if (configName == "max")
    {
      return Spine::FunctionId::Maximum;
    }
    else if (configName == "min")
    {
      return Spine::FunctionId::Minimum;
    }
    else if (configName == "median")
    {
      return Spine::FunctionId::Median;
    }
    else if (configName == "sum")
    {
      return Spine::FunctionId::Sum;
    }
    else if (configName == "sdev")
    {
      return Spine::FunctionId::StandardDeviation;
    }
    else if (configName == "trend")
    {
      return Spine::FunctionId::Trend;
    }
    else if (configName == "change")
    {
      return Spine::FunctionId::Change;
    }
    else if (configName == "count")
    {
      return Spine::FunctionId::Count;
    }
    else if (configName == "percentage")
    {
      return Spine::FunctionId::Percentage;
    }

    return Spine::FunctionId::NullFunction;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
      throw Fmi::Exception(BCP,
                             string("Precision settings for ") + name +
                                 " are missing from pointforecast configuration");

    libconfig::Setting& settings = itsConfig.lookup(optname);

    if (!settings.isGroup())
      throw Fmi::Exception(
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
          throw Fmi::Exception(
              BCP,
              "Precision settings must be nonnegative in pointforecast configuration for " +
                  string(name) + "." + paramname);

        if (paramname == "default")
          prec.default_precision = value;
        else
          prec.parameter_precisions.insert(Precision::Map::value_type(paramname, value));
      }
      catch (const libconfig::ParseException& e)
      {
        throw Fmi::Exception(BCP,
                               string("TimeSeries configuration error ' ") + e.getError() +
                                   "' with variable '" + paramname + "' on line " +
                                   Fmi::to_string(e.getLine()));
      }
      catch (const libconfig::ConfigException&)
      {
        throw Fmi::Exception(BCP,
                               string("TimeSeries configuration error with variable '") +
                                   paramname + "' on line " +
                                   Fmi::to_string(settings[i].getSourceLine()));
      }
      catch (const std::exception& e)
      {
        throw Fmi::Exception(BCP,
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
        throw Fmi::Exception(BCP,
                               "precision.enabled missing from pointforecast congiguration file");

      libconfig::Setting& enabled = itsConfig.lookup("precision.enabled");
      if (!enabled.isArray())
      {
        throw Fmi::Exception(
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
        throw Fmi::Exception(BCP, "No precisions defined in pointforecast precision: datablock!");
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Constructor
 */
// ----------------------------------------------------------------------

Config::Config(const string& configfile)
    : itsDefaultPrecision("normal"),
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
      itsMaxFilesystemCacheSize(0)  // off by default
      ,
      itsMaxTimeSeriesCacheSize(10000)
{
  try
  {
    if (configfile.empty())
      throw Fmi::Exception(BCP, "TimeSeries configuration file cannot be empty");

    boost::filesystem::path p = configfile;
    p.remove_filename();
    itsConfig.setIncludeDir(p.c_str());
    
    itsConfig.readFile(configfile.c_str());

    // Obligatory settings
    itsDefaultLocaleName = itsConfig.lookup("locale").c_str();
    itsDefaultLanguage = itsConfig.lookup("language").c_str();

    if (itsDefaultLocaleName.empty())
      throw Fmi::Exception(BCP, "Default locale name cannot be empty");

    if (itsDefaultLanguage.empty())
      throw Fmi::Exception(BCP, "Default language code cannot be empty");

    // Optional settings
    itsConfig.lookupValue("timeformat", itsDefaultTimeFormat);
    itsConfig.lookupValue("url", itsDefaultUrl);
    itsConfig.lookupValue("maxdistance", itsDefaultMaxDistance);
    itsConfig.lookupValue("expires", itsExpirationTime);
    itsConfig.lookupValue("observation_disabled", itsObsEngineDisabled);

    itsConfig.lookupValue("cache.memory_bytes", itsMaxMemoryCacheSize);
    itsConfig.lookupValue("cache.filesystem_bytes", itsMaxFilesystemCacheSize);
    itsConfig.lookupValue("cache.directory", itsFilesystemCacheDirectory);
    itsConfig.lookupValue("cache.timeseries_size", itsMaxTimeSeriesCacheSize);

    itsFormatterOptions = Spine::TableFormatterOptions(itsConfig);

    parse_config_precisions();

    // PostGIS
    if (itsConfig.exists("geometry_tables"))
    {
      Engine::Gis::postgis_identifier default_postgis_id;
      std::string default_server;
      std::string default_schema;
      std::string default_table;
      std::string default_field;
      itsConfig.lookupValue("geometry_tables.server", default_server);
      itsConfig.lookupValue("geometry_tables.schema", default_schema);
      itsConfig.lookupValue("geometry_tables.table", default_table);
      itsConfig.lookupValue("geometry_tables.field", default_field);
      if (!default_schema.empty() && !default_table.empty() && !default_field.empty())
      {
        default_postgis_id.pgname = default_server;
        default_postgis_id.schema = default_schema;
        default_postgis_id.table = default_table;
        default_postgis_id.field = default_field;
        postgis_identifiers.insert(make_pair(default_postgis_id.key(), default_postgis_id));
      }

      if (itsConfig.exists("geometry_tables.additional_tables"))
      {
        libconfig::Setting& additionalTables =
            itsConfig.lookup("geometry_tables.additional_tables");

        for (int i = 0; i < additionalTables.getLength(); i++)
        {
          libconfig::Setting& tableConfig = additionalTables[i];
          std::string server = (default_server.empty() ? "" : default_server);
          std::string schema = (default_schema.empty() ? "" : default_schema);
          std::string table = (default_table.empty() ? "" : default_table);
          std::string field = (default_field.empty() ? "" : default_field);
          tableConfig.lookupValue("server", server);
          tableConfig.lookupValue("schema", schema);
          tableConfig.lookupValue("table", table);
          tableConfig.lookupValue("field", field);

          Engine::Gis::postgis_identifier postgis_id;
          postgis_id.pgname = server;
          postgis_id.schema = schema;
          postgis_id.table = table;
          postgis_id.field = field;

          if (schema.empty() || table.empty() || field.empty())
            throw Fmi::Exception(BCP,
                                   "Configuration file error. Some of the following fields "
                                   "missing: server, schema, table, field!");

          postgis_identifiers.insert(std::make_pair(postgis_id.key(), postgis_id));
        }
      }
    }

    // We construct the default locale only once from the string,
    // creating it from scratch for every request is very expensive
    itsDefaultLocale.reset(new std::locale(itsDefaultLocaleName.c_str()));
  }
  catch (const libconfig::SettingNotFoundException& e)
  {
    throw Fmi::Exception(BCP, "Setting not found").addParameter("Setting path", e.getPath());
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
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

    throw Fmi::Exception(BCP, "Unknown precision '" + name + "'!");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Engine::Gis::PostGISIdentifierVector Config::getPostGISIdentifiers() const
{
  Engine::Gis::PostGISIdentifierVector ret;
  for (const auto & item : postgis_identifiers)
    ret.push_back(item.second);

  return ret;
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
