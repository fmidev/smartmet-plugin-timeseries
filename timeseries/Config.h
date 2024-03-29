// ======================================================================
/*!
 * \brief Configuration file API
 */
// ======================================================================

#pragma once

#include "Precision.h"
#include "Query.h"
#include <engines/gis/GeometryStorage.h>
#include <engines/grid/Engine.h>
#include <spine/Parameter.h>
#include <spine/TableFormatterOptions.h>
#include <timeseries/RequestLimits.h>
#include <libconfig.h++>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
using Precisions = std::map<std::string, Precision>;

class Config
{
 public:
  ~Config() = default;
  Config() = delete;
  explicit Config(const std::string& configfile);

  Config(const Config& other) = delete;
  Config& operator=(const Config& other) = delete;
  Config(Config&& other) = delete;
  Config& operator=(Config&& other) = delete;

  const Precision& getPrecision(const std::string& name) const;

  const std::string& defaultPrecision() const { return itsDefaultPrecision; }
  const std::string& defaultProducerMappingName() const { return itsDefaultProducerMappingName; }
  const std::string& defaultLanguage() const { return itsDefaultLanguage; }
  // You can copy the locale, not modify it!
  const std::locale& defaultLocale() const { return *itsDefaultLocale; }
  const std::string& defaultLocaleName() const { return itsDefaultLocaleName; }
  const std::string& defaultTimeFormat() const { return itsDefaultTimeFormat; }
  const std::string& defaultUrl() const { return itsDefaultUrl; }
  const std::string& defaultMaxDistance() const { return itsDefaultMaxDistance; }
  const std::string& defaultWxmlTimeString() const
  {
    return itsFormatterOptions.defaultWxmlTimeString();
  }

  const std::vector<uint>& defaultGridGeometries() const { return itsDefaultGridGeometries; }

  const std::string& defaultWxmlVersion() const { return itsFormatterOptions.defaultWxmlVersion(); }
  const std::string& wxmlSchema() const { return itsFormatterOptions.wxmlSchema(); }
  const Spine::TableFormatterOptions& formatterOptions() const { return itsFormatterOptions; }
  const libconfig::Config& config() const { return itsConfig; }
  Engine::Gis::PostGISIdentifierVector getPostGISIdentifiers() const;

  bool obsEngineDisabled() const { return itsObsEngineDisabled; }
  bool gridEngineDisabled() const { return itsGridEngineDisabled; }
  std::string primaryForecastSource() const { return itsPrimaryForecastSource; }
  bool obsEngineDatabaseQueryPrevented() const { return itsPreventObsEngineDatabaseQuery; }

  unsigned long long maxTimeSeriesCacheSize() const;

  unsigned int expirationTime() const { return itsExpirationTime; }
  const TS::RequestLimits& requestLimits() const { return itsRequestLimits; };

  QueryServer::AliasFileCollection itsAliasFileCollection;
  time_t itsLastAliasCheck;

 private:
  libconfig::Config itsConfig;
  std::string itsDefaultPrecision;
  std::string itsDefaultProducerMappingName;
  std::string itsDefaultLanguage;
  std::string itsDefaultLocaleName;
  std::unique_ptr<std::locale> itsDefaultLocale;
  std::string itsDefaultTimeFormat;
  std::string itsDefaultUrl;
  std::string itsDefaultMaxDistance;
  unsigned int itsExpirationTime;
  std::vector<std::string> itsParameterAliasFiles;
  std::vector<uint> itsDefaultGridGeometries;

  Spine::TableFormatterOptions itsFormatterOptions;
  Precisions itsPrecisions;

  std::map<std::string, Engine::Gis::postgis_identifier> postgis_identifiers;
  std::string itsDefaultPostGISIdentifierKey;
  bool itsObsEngineDisabled;
  bool itsGridEngineDisabled;
  std::string itsPrimaryForecastSource;
  bool itsPreventObsEngineDatabaseQuery;

  unsigned long long itsMaxTimeSeriesCacheSize;
  SmartMet::TimeSeries::RequestLimits itsRequestLimits;

  void add_default_precisions();
  void parse_config_precisions();
  void parse_config_precision(const std::string& name);
  void parse_geometries();
  void parse_grid_settings(const std::string& configfile);

};  // class Config

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
