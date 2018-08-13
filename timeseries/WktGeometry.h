// ======================================================================
/*!
 * \brief Class for geometries created from wkt-string (passed in URL)
 *
 * The following 2D geometries are supported: Point, LineString, Polygon,
 * MultiPoint, MultiLineString and MultiPolygon.
 *
 * WKT-string is read from loc->name field. Multipart geometries are
 * decomposed into geometry primitives and stored in corresponding
 * data members.
 *
 * Name for the wkt-geometry can be given by appending string 'as <name>'
 * to the wkt-string. If no name is given the whole wkt-string is used
 * as a name, but if wkt-string is longer than 60 characters the name is
 * truncated into 30 characters in order to be more usable in output document.
 *
 */
// ======================================================================

#pragma once

#include <gis/OGR.h>
#include <newbase/NFmiSvgPath.h>
#include <spine/Location.h>

namespace SmartMet
{
namespace Engine
{
namespace Geonames
{
class Engine;
}
}  // namespace Engine

namespace Plugin
{
namespace TimeSeries
{
class WktGeometry
{
 public:
  WktGeometry(const Spine::LocationPtr loc,
              const std::string& language,
              const SmartMet::Engine::Geonames::Engine& geoengine);
  ~WktGeometry();

  const OGRGeometry* getGeometry() const;
  Spine::LocationPtr getLocation() const;
  Spine::LocationList getLocations() const;
  NFmiSvgPath getSvgPath() const;
  std::list<NFmiSvgPath> getSvgPaths() const;
  const std::string& getName() const;

 private:
  void init(const Spine::LocationPtr loc,
            const std::string& language,
            const SmartMet::Engine::Geonames::Engine& geoengine);
  void geometryFromWkt(const std::string& wktString, double radius);
  void svgPathsFromGeometry();
  void locationsFromGeometry(const Spine::LocationPtr loc,
                             const std::string& language,
                             const SmartMet::Engine::Geonames::Engine& geoengine);
  Spine::LocationPtr locationFromGeometry(const OGRGeometry* geom,
                                          const Spine::LocationPtr loc,
                                          const std::string& language,
                                          const SmartMet::Engine::Geonames::Engine& geoengine);

  std::string itsName;                 // Name of geometry
  OGRGeometry* itsGeom = nullptr;      // Geometry created from wkt
  NFmiSvgPath itsSvgPath;              // NFmiSvgPath for original geometry
  std::list<NFmiSvgPath> itsSvgPaths;  // NFmiSvgPaths for geometries inside multipart geometry
  Spine::LocationPtr itsLocation;      // Location for original geometry
  Spine::LocationList itsLocations;    // Locations for geometries inside multipart geometry
};

typedef std::shared_ptr<const WktGeometry> WktGeometryPtr;

class WktGeometries
{
 public:
  void addWktGeometry(const std::string& id, WktGeometryPtr wktGeometry);
  Spine::LocationPtr getLocation(const std::string& id) const;
  Spine::LocationList getLocations(const std::string& id) const;
  NFmiSvgPath getSvgPath(const std::string& id) const;
  std::list<NFmiSvgPath> getSvgPaths(const std::string& id) const;
  const OGRGeometry* getGeometry(const std::string& id) const;
  std::string getName(const std::string& id) const;

 private:
  std::map<std::string, WktGeometryPtr> itsWktGeometries;
};

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
