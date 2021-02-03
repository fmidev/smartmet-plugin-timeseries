// ======================================================================
/*!
 * \brief Configuration file API
 */
// ======================================================================

#pragma once

#include "Precision.h"
#include "Query.h"

#include <boost/utility.hpp>
#include <engines/gis/GeometryStorage.h>
#include <spine/Parameter.h>
#include <spine/TableFormatterOptions.h>
#include <libconfig.h++>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
typedef std::map<std::string, Precision> Precisions;

class Config : private boost::noncopyable
{
 public:
  Config(const std::string& configfile);
  Config() = delete;

  const Precision& getPrecision(const std::string& name) const;

  const std::string& defaultPrecision() const { return itsDefaultPrecision; }
  const std::string& defaultLanguage() const { return itsDefaultLanguage; }
  // You can copy the locale, not modify it!
  const std::locale& defaultLocale() const { return *itsDefaultLocale; }
  const std::string& defaultLocaleName() const { return itsDefaultLocaleName; }
  const std::string& defaultTimeFormat() const { return itsDefaultTimeFormat; }
  const std::string& defaultUrl() const { return itsDefaultUrl; }
  double defaultMaxDistance() const { return itsDefaultMaxDistance; }
  const std::string& defaultWxmlTimeString() const
  {
    return itsFormatterOptions.defaultWxmlTimeString();
  }

  const std::string& defaultWxmlVersion() const { return itsFormatterOptions.defaultWxmlVersion(); }
  const std::string& wxmlSchema() const { return itsFormatterOptions.wxmlSchema(); }
  const SmartMet::Spine::TableFormatterOptions& formatterOptions() const
  {
    return itsFormatterOptions;
  }
  const libconfig::Config& config() const { return itsConfig; }
  Engine::Gis::PostGISIdentifierVector getPostGISIdentifiers() const;

  bool obsEngineDisabled() const { return itsObsEngineDisabled; }
  bool obsEngineDatabaseQueryPrevented() const { return itsPreventObsEngineDatabaseQuery; }
  unsigned long long maxMemoryCacheSize() const;
  unsigned long long maxFilesystemCacheSize() const;
  const std::string& filesystemCacheDirectory() const;

  unsigned long long maxTimeSeriesCacheSize() const;

  unsigned int expirationTime() const { return itsExpirationTime; }

 private:
  libconfig::Config itsConfig;
  std::string itsDefaultPrecision;
  std::string itsDefaultLanguage;
  std::string itsDefaultLocaleName;
  std::unique_ptr<std::locale> itsDefaultLocale;
  std::string itsDefaultTimeFormat;
  std::string itsDefaultUrl;
  double itsDefaultMaxDistance;
  unsigned int itsExpirationTime;

  SmartMet::Spine::TableFormatterOptions itsFormatterOptions;
  Precisions itsPrecisions;

  std::map<std::string, Engine::Gis::postgis_identifier> postgis_identifiers;
  std::string itsDefaultPostGISIdentifierKey;
  bool itsObsEngineDisabled;
  bool itsPreventObsEngineDatabaseQuery;

  std::string itsFilesystemCacheDirectory;
  unsigned long long itsMaxMemoryCacheSize;
  unsigned long long itsMaxFilesystemCacheSize;
  unsigned long long itsMaxTimeSeriesCacheSize;

 private:
  void add_default_precisions();
  void parse_config_precisions();
  void parse_config_precision(const std::string& name);

};  // class Config

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
