#include "LocationTools.h"
#include "LonLatDistance.h"
#include <grid-files/common/GraphFunctions.h>
#include <macgyver/Exception.h>
#include <newbase/NFmiSvgTools.h>
#include <timeseries/ParameterKeywords.h>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
namespace
{
// Construct the locale for case conversions only once
const std::locale stdlocale = std::locale();
}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Get location name from location:radius
 */
// ----------------------------------------------------------------------

std::string get_name_base(const std::string& theName)
{
  try
  {
    std::string place = theName;

    // remove radius if exists
    auto pos = place.find(':');
    if (pos != std::string::npos)
      place.resize(pos);

    return place;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Fetch geometry from database
 */
// ----------------------------------------------------------------------

const OGRGeometry* get_ogr_geometry(const Spine::TaggedLocation& tloc,
                                    const Engine::Gis::GeometryStorage& geometryStorage)
{
  const OGRGeometry* ret = nullptr;

  try
  {
    Spine::LocationPtr loc = tloc.loc;
    std::string place = get_name_base(loc->name);
    boost::algorithm::to_lower(place, stdlocale);

    if (loc->type == Spine::Location::Place || loc->type == Spine::Location::CoordinatePoint)
    {
      ret = geometryStorage.getOGRGeometry(place, wkbPoint);
    }
    else if (loc->type == Spine::Location::Area)
    {
      ret = geometryStorage.getOGRGeometry(place, wkbPolygon);
      if (ret == nullptr)
        ret = geometryStorage.getOGRGeometry(place, wkbMultiPolygon);
    }
    else if (loc->type == Spine::Location::Path)
    {
      ret = geometryStorage.getOGRGeometry(place, wkbLineString);
      if (ret == nullptr)
        ret = geometryStorage.getOGRGeometry(place, wkbMultiLineString);
    }
    else if (loc->type == Spine::Location::Wkt)
    {
      std::unique_ptr<OGRGeometry> ret = get_ogr_geometry(place, loc->radius);
      return ret.release();
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }

  return ret;
}

std::unique_ptr<OGRGeometry> get_ogr_geometry(const std::string& wktString, double radius /*= 0.0*/)
{
  try
  {
    std::unique_ptr<OGRGeometry> ret;

    std::string wkt = get_name_base(wktString);

    OGRGeometry* geom = Fmi::OGR::createFromWkt(wkt, 4326);

    if (geom)
    {
      if (radius > 0.0)
      {
        std::unique_ptr<OGRGeometry> poly;
        poly.reset(Fmi::OGR::expandGeometry(geom, radius * 1000));
        OGRGeometryFactory::destroyGeometry(geom);
        geom = poly.release();
      }

      ret.reset(geom);
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Fetch geometry as NFmiSvgPath from database
 */
// ----------------------------------------------------------------------

void get_svg_path(const Spine::TaggedLocation& tloc,
                  const Engine::Gis::GeometryStorage& geometryStorage,
                  NFmiSvgPath& svgPath)
{
  try
  {
    Spine::LocationPtr loc = tloc.loc;
    std::string place = get_name_base(loc->name);

    if (loc->type == Spine::Location::Place || loc->type == Spine::Location::CoordinatePoint)
    {
      NFmiSvgTools::PointToSvgPath(svgPath, loc->longitude, loc->latitude);
    }
    else if (loc->type == Spine::Location::Area)
    {
      if (geometryStorage.isPolygon(place))
      {
        std::stringstream svgStringStream(geometryStorage.getSVGPath(place));
        svgPath.Read(svgStringStream);
      }
      else if (geometryStorage.isPoint(place))
      {
        std::pair<double, double> thePoint = geometryStorage.getPoint(place);
        NFmiSvgTools::PointToSvgPath(svgPath, thePoint.first, thePoint.second);
      }
      else
      {
        throw Fmi::Exception(
            BCP, "Area '" + place + "' not found in PostGIS database or is not readable!");
      }
    }
    else if (loc->type == Spine::Location::Path)
    {
      if (place.find(',') != std::string::npos)
      {
        // path given as a query parameter in format "lon,lat,lon,lat,lon,lat,..."
        std::vector<std::string> lonLatVector;
        boost::algorithm::split(lonLatVector, place, boost::algorithm::is_any_of(","));

        if (lonLatVector.size() % 2)
          throw Fmi::Exception(
              BCP, "Path is required to have even number of coordinates. Got '" + place + "'");

        for (unsigned int i = 0; i < lonLatVector.size(); i += 2)
        {
          double longitude = Fmi::stod(lonLatVector[i]);
          double latitude = Fmi::stod(lonLatVector[i + 1]);
          svgPath.push_back(NFmiSvgPath::Element(
              (i == 0 ? NFmiSvgPath::kElementMoveto : NFmiSvgPath::kElementLineto),
              longitude,
              latitude));
        }
      }
      else
      {
        // path fetched from PostGIS database
        if (geometryStorage.isPolygon(place) || geometryStorage.isLine(place))
        {
          std::stringstream svgStringStream(geometryStorage.getSVGPath(place));
          svgPath.Read(svgStringStream);
        }
        else if (geometryStorage.isPoint(place))
        {
          std::pair<double, double> thePoint = geometryStorage.getPoint(place);
          NFmiSvgTools::PointToSvgPath(svgPath, thePoint.first, thePoint.second);
        }
        else
        {
          throw Fmi::Exception(
              BCP, "Path '" + place + "' not found in PostGIS database or is not readable!");
        }
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

Spine::LocationList get_location_list(const NFmiSvgPath& thePath,
                                      const std::string& thePathName,
                                      const double& stepInKm,
                                      const Engine::Geonames::Engine& geonames)
{
  try
  {
    Spine::LocationList locationList;

    double step = std::max(0.01, stepInKm);

    std::pair<double, double> from(thePath.begin()->itsX, thePath.begin()->itsY);
    std::pair<double, double> to(thePath.begin()->itsX, thePath.begin()->itsY);

    double leftoverDistanceKmFromPreviousLeg = 0.0;
    std::string theTimezone;

    std::size_t i = 0;
    for (const auto& element : thePath)
    {
      to = std::pair<double, double>(element.itsX, element.itsY);

      // first round
      if (i++ == 0)
      {
        // fetch geoinfo only for the first coordinate, because geosearch is so slow
        // reuse name and timezone for rest of the locations
        Spine::LocationPtr locFirst = geonames.lonlatSearch(element.itsX, element.itsY, "");

        Spine::LocationPtr loc = Spine::LocationPtr(new Spine::Location(locFirst->geoid,
                                                                        thePathName,
                                                                        locFirst->iso2,
                                                                        locFirst->municipality,
                                                                        locFirst->area,
                                                                        locFirst->feature,
                                                                        locFirst->country,
                                                                        locFirst->longitude,
                                                                        locFirst->latitude,
                                                                        locFirst->timezone,
                                                                        locFirst->population,
                                                                        locFirst->elevation));

        theTimezone = loc->timezone;
        locationList.push_back(loc);
      }
      else
      {
        // Each path-element is handled separately
        if (element.itsType == NFmiSvgPath::kElementMoveto &&
            distance_in_kilometers(from, to) > step)
        {
          // Start the new leg
          locationList.push_back(Spine::LocationPtr(
              new Spine::Location(to.first, to.second, thePathName, theTimezone)));
          from = to;
          leftoverDistanceKmFromPreviousLeg = 0.0;
          continue;
        }

        double intermediateDistance = distance_in_kilometers(from, to);
        if ((intermediateDistance + leftoverDistanceKmFromPreviousLeg) <= step)
        {
          leftoverDistanceKmFromPreviousLeg += intermediateDistance;
        }
        else
        {
          // missing distance from step
          double missingDistance = step - leftoverDistanceKmFromPreviousLeg;
          std::pair<double, double> intermediatePoint =
              destination_point(from, to, missingDistance);
          locationList.push_back(Spine::LocationPtr(new Spine::Location(
              intermediatePoint.first, intermediatePoint.second, thePathName, theTimezone)));
          from = intermediatePoint;
          while (distance_in_kilometers(from, to) > step)
          {
            intermediatePoint = destination_point(from, to, step);
            locationList.push_back(Spine::LocationPtr(new Spine::Location(
                intermediatePoint.first, intermediatePoint.second, thePathName, theTimezone)));
            from = intermediatePoint;
          }
          leftoverDistanceKmFromPreviousLeg = distance_in_kilometers(from, to);
        }
      }
      from = to;
    }

    // Last leg is not full
    if (leftoverDistanceKmFromPreviousLeg > 0.001)
    {
      locationList.push_back(
          Spine::LocationPtr(new Spine::Location(to.first, to.second, thePathName, theTimezone)));
    }

    Spine::LocationPtr front = locationList.front();
    Spine::LocationPtr back = locationList.back();

    if (locationList.size() > 1 && front->longitude == back->longitude &&
        front->latitude == back->latitude)
      locationList.pop_back();

    return locationList;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

std::string get_location_id(const Spine::LocationPtr& loc)
{
  try
  {
    std::ostringstream ss;

    ss << loc->name << " " << std::fixed << std::setprecision(7) << loc->longitude << " "
       << loc->latitude << " " << loc->geoid;

    return ss.str();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION
std::vector<int> get_fmisids_for_wkt(Engine::Observation::Engine* observation,
                                     const Engine::Observation::Settings& settings,
                                     const std::string& wktstring)
{
  try
  {
    std::vector<int> fmisids;

    Spine::Stations stations;

    observation->getStationsByArea(stations, settings, wktstring);
    for (const auto& station : stations)
      fmisids.push_back(station.fmisid);

    return fmisids;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
#endif

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

Spine::LocationPtr get_location(const Engine::Geonames::Engine& geonames,
                                int id,
                                const std::string& idtype,
                                const std::string& language)
{
  try
  {
    if (idtype == GEOID_PARAM)
      return geonames.idSearch(id, language);

    Locus::QueryOptions opts;
    opts.SetCountries("all");
    opts.SetSearchVariants(true);
    opts.SetLanguage(idtype);
    opts.SetResultLimit(1);
    opts.SetFeatures({"SYNOP", "FINAVIA", "STUK"});

    const std::string id_str = Fmi::to_string(id);
    Spine::LocationList ll = geonames.nameSearch(opts, id_str);

    Spine::LocationPtr loc;

    // lets just take the first one
    if (!ll.empty())
    {
      const long geoid = (*ll.begin())->geoid;
      loc = geonames.idSearch(geoid, language);
      if (loc)
      {
        Spine::Location location(*loc);
        location.dem = geonames.demHeight(location.longitude, location.latitude);
        loc = std::make_shared<Spine::Location>(location);
      }
    }

    return loc;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION

int get_fmisid_index(const Engine::Observation::Settings& settings)
{
  try
  {
    // find out fmisid column
    for (unsigned int i = 0; i < settings.parameters.size(); i++)
      if (settings.parameters[i].name() == FMISID_PARAM)
        return i;

    return -1;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

#endif

// ----------------------------------------------------------------------
/*!
 * \brief Fetch Location from geoengine for lon-lat coordinate
 */
// ----------------------------------------------------------------------

std::unique_ptr<Spine::Location> get_coordinate_location(double lon,
                                                         double lat,
                                                         const std::string& language,
                                                         const Engine::Geonames::Engine& geoEngine)

{
  try
  {
    Spine::LocationPtr loc = geoEngine.lonlatSearch(lon, lat, language);

    std::unique_ptr<Spine::Location> ret(new Spine::Location(loc->geoid,
                                                             "",  // tloc.tag,
                                                             loc->iso2,
                                                             loc->municipality,
                                                             loc->area,
                                                             loc->feature,
                                                             loc->country,
                                                             loc->longitude,
                                                             loc->latitude,
                                                             loc->timezone,
                                                             loc->population,
                                                             loc->elevation));

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

std::unique_ptr<Spine::Location> get_bbox_location(const std::string& bbox_string,
                                                   const std::string& language,
                                                   const Engine::Geonames::Engine& geoengine)
{
  std::vector<std::string> parts;
  boost::algorithm::split(parts, bbox_string, boost::algorithm::is_any_of(","));

  double lon1 = Fmi::stod(parts[0]);
  double lat1 = Fmi::stod(parts[1]);
  double lon2 = Fmi::stod(parts[2]);
  double lat2 = Fmi::stod(parts[3]);

  // get location info for center coordinates
  double lon = (lon1 + lon2) / 2.0;
  double lat = (lat1 + lat2) / 2.0;

  return get_coordinate_location(lon, lat, language, geoengine);
}

Spine::LocationPtr get_location_for_area(const Spine::TaggedLocation& tloc,
                                         const Engine::Gis::GeometryStorage& geometryStorage,
                                         const std::string& language,
                                         const Engine::Geonames::Engine& geoengine,
                                         NFmiSvgPath* svgPath /*= nullptr*/)
{
  try
  {
    double bottom = 0.0;
    double top = 0.0;
    double left = 0.0;
    double right = 0.0;

    const OGRGeometry* geom = get_ogr_geometry(tloc, geometryStorage);

    if (geom)
    {
      OGREnvelope envelope;
      geom->getEnvelope(&envelope);
      top = envelope.MaxY;
      bottom = envelope.MinY;
      left = envelope.MinX;
      right = envelope.MaxX;
    }

    if (svgPath != nullptr)
    {
      get_svg_path(tloc, geometryStorage, *svgPath);

      if (!geom)
      {
        // get location info for center coordinate
        bottom = svgPath->begin()->itsY;
        top = svgPath->begin()->itsY;
        left = svgPath->begin()->itsX;
        right = svgPath->begin()->itsX;

        for (const auto& element : *svgPath)
        {
          left = std::min(left, element.itsX);
          right = std::max(right, element.itsX);
          bottom = std::min(bottom, element.itsY);
          top = std::max(top, element.itsY);
        }
      }
    }

    double lon = (right + left) / 2.0;
    double lat = (top + bottom) / 2.0;
    std::unique_ptr<Spine::Location> tmp = get_coordinate_location(lon, lat, language, geoengine);

    tmp->name = tloc.tag;
    tmp->type = tloc.loc->type;
    tmp->radius = tloc.loc->radius;

    return Spine::LocationPtr(tmp.release());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Spine::LocationPtr get_location_for_area(const Spine::TaggedLocation& tloc,
                                         int radius,
                                         const Engine::Gis::GeometryStorage& geometryStorage,
                                         const std::string& language,
                                         const Engine::Geonames::Engine& geoengine,
                                         NFmiSvgPath* svgPath /*= nullptr*/)
{
  try
  {
    double bottom = 0.0;
    double top = 0.0;
    double left = 0.0;
    double right = 0.0;

    const OGRGeometry* geom = get_ogr_geometry(tloc, geometryStorage);
    std::string wktString;

    std::unique_ptr<OGRGeometry> expandedGeomUptr;
    if (geom && radius > 0)
    {
      auto* expandedGeom = Fmi::OGR::expandGeometry(geom, radius);
      expandedGeomUptr.reset(expandedGeom);
      wktString = Fmi::OGR::exportToWkt(*expandedGeom);
      geom = expandedGeom;
    }

    if (geom)
    {
      OGREnvelope envelope;
      geom->getEnvelope(&envelope);
      top = envelope.MaxY;
      bottom = envelope.MinY;
      left = envelope.MinX;
      right = envelope.MaxX;
    }

    if (svgPath != nullptr)
    {
      if (!wktString.empty())
      {
        convertWktMultipolygonToSvgPath(wktString, *svgPath);
      }
      else
      {
        get_svg_path(tloc, geometryStorage, *svgPath);
      }

      if (!geom)
      {
        // get location info for center coordinate
        bottom = svgPath->begin()->itsY;
        top = svgPath->begin()->itsY;
        left = svgPath->begin()->itsX;
        right = svgPath->begin()->itsX;

        for (const auto& element : *svgPath)
        {
          left = std::min(left, element.itsX);
          right = std::max(right, element.itsX);
          bottom = std::min(bottom, element.itsY);
          top = std::max(top, element.itsY);
        }
      }
    }

    std::pair<double, double> lonlatCenter((right + left) / 2.0, (top + bottom) / 2.0);

    Spine::LocationPtr locCenter =
        geoengine.lonlatSearch(lonlatCenter.first, lonlatCenter.second, language);

    // Spine::LocationPtr contains a const Location, so some trickery is used here
    std::unique_ptr<Spine::Location> tmp(new Spine::Location(locCenter->geoid,
                                                             tloc.tag,
                                                             locCenter->iso2,
                                                             locCenter->municipality,
                                                             locCenter->area,
                                                             locCenter->feature,
                                                             locCenter->country,
                                                             locCenter->longitude,
                                                             locCenter->latitude,
                                                             locCenter->timezone,
                                                             locCenter->population,
                                                             locCenter->elevation));
    tmp->type = tloc.loc->type;
    tmp->radius = tloc.loc->radius;

    return Spine::LocationPtr(tmp.release());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Spine::TaggedLocationList get_locations_inside_geometry(const Spine::LocationList& locations,
                                                        const OGRGeometry& geom)
{
  try
  {
    Spine::TaggedLocationList ret;

    for (const auto& loc : locations)
    {
      std::string wkt =
          ("POINT(" + Fmi::to_string(loc->longitude) + " " + Fmi::to_string(loc->latitude) + ")");
      std::unique_ptr<OGRGeometry> location_geom = get_ogr_geometry(wkt);
      if (geom.Contains(location_geom.get()))
        ret.emplace_back(loc->name, loc);
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
