#include "WktGeometry.h"
#include "LocationTools.h"

#include <engines/geonames/Engine.h>
#include <gis/Box.h>
#include <spine/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
namespace
{
std::list<const OGRGeometry*> get_geometry_list(const OGRGeometry* geom)
{
  std::list<const OGRGeometry*> ret;

  switch (geom->getGeometryType())
  {
    case wkbMultiPoint:
    {
      // OGRMultiPoint geometry -> extract OGRPoints
      const OGRMultiPoint* mpGeom = static_cast<const OGRMultiPoint*>(geom);
      int numGeoms = mpGeom->getNumGeometries();
      for (int i = 0; i < numGeoms; i++)
        ret.push_back(mpGeom->getGeometryRef(i));
    }
    break;
    case wkbMultiLineString:
    {
      // OGRMultiLineString geometry -> extract OGRLineStrings
      const OGRMultiLineString* mlsGeom = static_cast<const OGRMultiLineString*>(geom);
      int numGeoms = mlsGeom->getNumGeometries();
      for (int i = 0; i < numGeoms; i++)
        ret.push_back(mlsGeom->getGeometryRef(i));
    }
    break;
    case wkbMultiPolygon:
    {
      // OGRMultiLineString geometry -> extract OGRLineStrings
      const OGRMultiPolygon* mpolGeom = static_cast<const OGRMultiPolygon*>(geom);
      int numGeoms = mpolGeom->getNumGeometries();
      for (int i = 0; i < numGeoms; i++)
        ret.push_back(mpolGeom->getGeometryRef(i));
    }
    break;
    case wkbPoint:
    case wkbLineString:
    case wkbPolygon:
      ret.push_back(geom);
      break;
    default:
      // no other geometries are supported
      break;
  }

  return ret;
}

NFmiSvgPath get_svg_path(const OGRGeometry& geom)
{
  try
  {
    NFmiSvgPath ret;
    // Get SVG-path
    Fmi::Box box = Fmi::Box::identity();
    std::string svgString = Fmi::OGR::exportToSvg(geom, box, 6);
    svgString.insert(0, " \"\n");
    svgString.append(" \"\n");
    std::stringstream svgStringStream(svgString);
    ret.Read(svgStringStream);

    return ret;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Failed to create NFmiSvgPath from OGRGeometry");
  }
}

bool is_multi_geometry(const OGRGeometry& geom)
{
  bool ret = false;

  switch (geom.getGeometryType())
  {
    case wkbMultiPoint:
    case wkbMultiLineString:
    case wkbMultiPolygon:
    {
      ret = true;
    }
    break;
    default:
      break;
  }

  return ret;
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Initialize the WktGeometry object
 */
// ----------------------------------------------------------------------

void WktGeometry::init(const Spine::LocationPtr loc,
                       const std::string& language,
                       const SmartMet::Engine::Geonames::Engine& geoengine)
{
  try
  {
    // Create OGRGeometry
    geometryFromWkt(loc->name, loc->radius);

    // Get SVG-path
    svgPathsFromGeometry();

    // Get locations from OGRGeometry; OGRMulti* geometries may contain many locations
    locationsFromGeometry(loc, language, geoengine);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Create OGRGeometry from wkt-string
 */
// ----------------------------------------------------------------------

void WktGeometry::geometryFromWkt(const std::string& wktString, double radius)
{
  if (wktString.find(" as") != std::string::npos)
    itsName = wktString.substr(wktString.find(" as") + 3);
  else
  {
    itsName = wktString;
    if (itsName.size() > 60)
    {
      itsName = wktString.substr(0, 30);
      itsName += " ...";
    }
  }

  itsGeom = get_ogr_geometry(wktString, radius).release();
}

// ----------------------------------------------------------------------
/*!
 * \brief Create NFmiSvgPath object(s) from OGRGeometry
 */
// ----------------------------------------------------------------------

void WktGeometry::svgPathsFromGeometry()
{
  itsSvgPath = get_svg_path(*itsGeom);

  if (is_multi_geometry(*itsGeom))
  {
    std::list<const OGRGeometry*> glist = get_geometry_list(itsGeom);
    for (auto g : glist)
      itsSvgPaths.push_back(get_svg_path(*g));
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Create Spine::LocationPtr objects from OGRGeometry
 */
// ----------------------------------------------------------------------

void WktGeometry::locationsFromGeometry(const Spine::LocationPtr loc,
                                        const std::string& language,
                                        const SmartMet::Engine::Geonames::Engine& geoengine)
{
  itsLocation = locationFromGeometry(itsGeom, loc, language, geoengine);

  if (is_multi_geometry(*itsGeom))
  {
    std::list<const OGRGeometry*> glist = get_geometry_list(itsGeom);
    for (auto g : glist)
      itsLocations.push_back(locationFromGeometry(g, loc, language, geoengine));
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Create Spine::LocationPtr OGRGeometry
 */
// ----------------------------------------------------------------------

Spine::LocationPtr WktGeometry::locationFromGeometry(
    const OGRGeometry* geom,
    const Spine::LocationPtr loc,
    const std::string& language,
    const SmartMet::Engine::Geonames::Engine& geoengine)
{
  OGREnvelope envelope;
  geom->getEnvelope(&envelope);
  double top = envelope.MaxY;
  double bottom = envelope.MinY;
  double left = envelope.MinX;
  double right = envelope.MaxX;

  double lon = (right + left) / 2.0;
  double lat = (top + bottom) / 2.0;
  std::unique_ptr<Spine::Location> tmp = get_coordinate_location(lon, lat, language, geoengine);

  tmp->radius = loc->radius;
  tmp->type = loc->type;
  tmp->name = itsName;

  OGRwkbGeometryType type = geom->getGeometryType();
  switch (type)
  {
    case wkbPoint:
    {
      tmp->type = Spine::Location::CoordinatePoint;
    }
    break;
    case wkbPolygon:
    case wkbMultiPolygon:
    {
      tmp->type = Spine::Location::Area;
    }
    break;
    case wkbLineString:
    case wkbMultiLineString:
    case wkbMultiPoint:
    {
      // LINESTRING, MULTILINESTRING and MULTIPOINT are handled in a similar fashion
      tmp->type = Spine::Location::Path;
    }
    break;
    default:
      break;
  };

  Spine::LocationPtr ret(tmp.release());

  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize the WktGeometry object
 */
// ----------------------------------------------------------------------

WktGeometry::WktGeometry(const Spine::LocationPtr loc,
                         const std::string& language,
                         const SmartMet::Engine::Geonames::Engine& geoengine)
{
  init(loc, language, geoengine);
}

// ----------------------------------------------------------------------
/*!
 * \brief Destroy OGRGeometry object
 */
// ----------------------------------------------------------------------

WktGeometry::~WktGeometry()
{
  if (itsGeom)
  {
    OGRGeometryFactory::destroyGeometry(itsGeom);
    itsGeom = nullptr;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get Spine::LocationPtr
 */
// ----------------------------------------------------------------------

Spine::LocationPtr WktGeometry::getLocation() const
{
  return itsLocation;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get list of Spine::LocationPtr objects
 *
 * For multipart geometry this returns list of Spine::LocationPtr objects,
 * that has been created from geometiry primitives inside multipart geometry.
 *
 * For geometry primitives this returns empty list.
 */
// ----------------------------------------------------------------------

Spine::LocationList WktGeometry::getLocations() const
{
  return itsLocations;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get NFmiSvgPath object
 */
// ----------------------------------------------------------------------

NFmiSvgPath WktGeometry::getSvgPath() const
{
  return itsSvgPath;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get list of NFmiSvgPath objects
 *
 * For multipart geometry this returns list of NFmiSvgPath objects,
 * that has been created from geometiry primitives inside multipart geometry.
 *
 * For geometry primitives this returns empty list.
 */
// ----------------------------------------------------------------------

std::list<NFmiSvgPath> WktGeometry::getSvgPaths() const
{
  return itsSvgPaths;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get OGRGeometry object
 */
// ----------------------------------------------------------------------

const OGRGeometry* WktGeometry::getGeometry() const
{
  return itsGeom;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get name of geometry
 */
// ----------------------------------------------------------------------

const std::string& WktGeometry::getName() const
{
  return itsName;
}

// ----------------------------------------------------------------------
/*!
 * \brief Adds new geometry into container
 */
// ----------------------------------------------------------------------

void WktGeometries::addWktGeometry(const std::string& id, WktGeometryPtr wktGeometry)
{
  itsWktGeometries.insert(std::make_pair(id, wktGeometry));
}

Spine::LocationPtr WktGeometries::getLocation(const std::string& id) const
{
  Spine::LocationPtr ret = nullptr;

  if (itsWktGeometries.find(id) != itsWktGeometries.end())
  {
    WktGeometryPtr wktGeom = itsWktGeometries.at(id);
    ret = wktGeom->getLocation();
  }

  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get list of Spine::LocationPtr objects of specified geometry
 */
// ----------------------------------------------------------------------

Spine::LocationList WktGeometries::getLocations(const std::string& id) const
{
  if (itsWktGeometries.find(id) != itsWktGeometries.end())
    return itsWktGeometries.at(id)->getLocations();

  return Spine::LocationList();
}

// ----------------------------------------------------------------------
/*!
 * \brief Get NFmiSvgPath object of specified geometry
 */
// ----------------------------------------------------------------------

NFmiSvgPath WktGeometries::getSvgPath(const std::string& id) const
{
  if (itsWktGeometries.find(id) != itsWktGeometries.end())
    return itsWktGeometries.at(id)->getSvgPath();

  return NFmiSvgPath();
}

// ----------------------------------------------------------------------
/*!
 * \brief Get list of NFmiSvgPath objects of specified geometry
 */
// ----------------------------------------------------------------------

std::list<NFmiSvgPath> WktGeometries::getSvgPaths(const std::string& id) const
{
  if (itsWktGeometries.find(id) != itsWktGeometries.end())
    return itsWktGeometries.at(id)->getSvgPaths();

  return std::list<NFmiSvgPath>();
}

// ----------------------------------------------------------------------
/*!
 * \brief Get OGRGeometry object of specified geometry
 */
// ----------------------------------------------------------------------

const OGRGeometry* WktGeometries::getGeometry(const std::string& id) const
{
  const OGRGeometry* ret = nullptr;
  if (itsWktGeometries.find(id) != itsWktGeometries.end())
  {
    WktGeometryPtr wktGeom = itsWktGeometries.at(id);
    ret = wktGeom->getGeometry();
  }
  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get name of specified geometry
 */
// ----------------------------------------------------------------------

std::string WktGeometries::getName(const std::string& id) const
{
  if (itsWktGeometries.find(id) != itsWktGeometries.end())
    return itsWktGeometries.at(id)->getName();

  return std::string();
}

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
