#include "LocationTools.h"
#include "LonLatDistance.h"
#include <engines/observation/Keywords.h>
#include <macgyver/Exception.h>
#include <newbase/NFmiSvgTools.h>

namespace ts = SmartMet::Spine::TimeSeries;

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
    if (place.find(':') != std::string::npos)
      place = place.substr(0, place.find(':'));

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
        throw Fmi::Exception(BCP, "Area '" + place + "' not found in PostGIS database!");
      }
    }
    else if (loc->type == Spine::Location::Path)
    {
      if (place.find(',') != std::string::npos)
      {
        // path given as a query parameter in format "lon,lat,lon,lat,lon,lat,..."
        std::vector<std::string> lonLatVector;
        boost::algorithm::split(lonLatVector, place, boost::algorithm::is_any_of(","));
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
          throw Fmi::Exception(BCP, "Path '" + place + "' not found in PostGIS database!");
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

    double step = stepInKm;
    if (step < 0.01)
      step = 0.01;

    std::pair<double, double> from(thePath.begin()->itsX, thePath.begin()->itsY);
    std::pair<double, double> to(thePath.begin()->itsX, thePath.begin()->itsY);

    double leftoverDistanceKmFromPreviousLeg = 0.0;
    std::string theTimezone;

    for (NFmiSvgPath::const_iterator it = thePath.begin(); it != thePath.end(); ++it)
    {
      to = std::pair<double, double>(it->itsX, it->itsY);

      // first round
      if (it == thePath.begin())
      {
        // fetch geoinfo only for the first coordinate, because geosearch is so slow
        // reuse name and timezone for rest of the locations
        Spine::LocationPtr locFirst = geonames.lonlatSearch(it->itsX, it->itsY, "");

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
        if (it->itsType == NFmiSvgPath::kElementMoveto && distance_in_kilometers(from, to) > step)
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
                                     const std::string& producer,
                                     const boost::posix_time::ptime& starttime,
                                     const boost::posix_time::ptime& endtime,
                                     const std::string& wktstring)
{
  try
  {
    std::vector<int> fmisids;

    Spine::Stations stations;

    observation->getStationsByArea(stations, producer, starttime, endtime, wktstring);
    for (unsigned int i = 0; i < stations.size(); i++)
      fmisids.push_back(stations[i].fmisid);

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
                                const int id,
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

    Spine::LocationList ll = geonames.nameSearch(opts, Fmi::to_string(id));

    Spine::LocationPtr loc;

    // lets just take the first one
    if (ll.size() > 0)
      loc = geonames.idSearch((*ll.begin())->geoid, language);

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
 * \brief
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION

int get_fmisid_value(const ts::Value& value)
{
  try
  {
    // fmisid can be std::string or double
    if (boost::get<std::string>(&value))
    {
      std::string fmisidstr = boost::get<std::string>(value);
      boost::algorithm::trim(fmisidstr);
      if (!fmisidstr.empty())
        return std::stoi(fmisidstr);
      else
        throw Fmi::Exception(BCP, "fmisid value is an empty string");
    }
    else if (boost::get<int>(&value))
      return boost::get<int>(value);
    else if (boost::get<double>(&value))
      return boost::get<double>(value);
    else if (boost::get<Spine::TimeSeries::None>(&value))
      throw Fmi::Exception(BCP, "Station with null fmisid encountered!");
    else if (boost::get<Spine::TimeSeries::LonLat>(&value))
      throw Fmi::Exception(BCP, "Station with latlon as fmisid encountered!");
    else
      throw Fmi::Exception(BCP, "Unknown fmisid type");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

#endif

// ----------------------------------------------------------------------
/*!
 * \brief Extract fmisid from a timeseries vector where fmisid may not be set for all rows
 */
// ----------------------------------------------------------------------

int get_fmisid_value(const ts::TimeSeries& ts)
{
  for (std::size_t i = 0; i < ts.size(); i++)
  {
    try
    {
      return get_fmisid_value(ts[i].value);
    }
    catch (...)
    {
    }
  }
  return -1;
}

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

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
