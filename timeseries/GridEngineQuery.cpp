#include "GridEngineQuery.h"
#include "GridInterface.h"
#include "LocationTools.h"
#include "PostProcessing.h"
#include "State.h"
#include "UtilityFunctions.h"
#include <fmt/format.h>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
GridEngineQuery::GridEngineQuery(const Plugin& thePlugin) : itsPlugin(thePlugin)
{
  if (!itsPlugin.itsConfig.gridEngineDisabled())
  {
    itsGridInterface.reset(new GridInterface(itsPlugin.itsEngines.gridEngine.get(),
                                             itsPlugin.itsEngines.geoEngine->getTimeZones()));
  }
}

void GridEngineQuery::getLocationDefinition(Spine::LocationPtr& loc,
                                            std::vector<std::vector<T::Coordinate>>& polygonPath,
                                            const Spine::TaggedLocation& tloc,
                                            Query& query) const
{
  switch (loc->type)
  {
    case Spine::Location::Wkt:
    {
      // NFmiSvgPath svgPath;
      loc = query.wktGeometries.getLocation(tloc.loc->name);
      // svgPath = query.wktGeometries.getSvgPath(tloc.loc->name);
      /*
      convertSvgPathToPolygonPath(svgPath, polygonPath);

      if (polygonPath.size() > 1 && getPolygonPathLength(polygonPath) == polygonPath.size())
      {
        T::Coordinate_vec polygonPoints;
        convertToPointVector(polygonPath, polygonPoints);
        polygonPath.clear();
        polygonPath.push_back(polygonPoints);
      }
      */

      OGRwkbGeometryType geomType =
          query.wktGeometries.getGeometry(tloc.loc->name)->getGeometryType();
      if (geomType == wkbMultiLineString)
      {
        T::Coordinate_vec polygonPoints;
        std::list<NFmiSvgPath> svgList = query.wktGeometries.getSvgPaths(tloc.loc->name);
        for (const auto& svg : svgList)
        {
          Spine::LocationList ll =
              get_location_list(svg, tloc.tag, query.step, *itsPlugin.itsEngines.geoEngine);
          for (auto it = ll.begin(); it != ll.end(); ++it)
            polygonPoints.emplace_back((*it)->longitude, (*it)->latitude);
        }
        polygonPath.push_back(polygonPoints);
      }
      else if (geomType == wkbMultiPoint)
      {
        T::Coordinate_vec polygonPoints;
        Spine::LocationList ll = query.wktGeometries.getLocations(tloc.loc->name);
        for (auto it = ll.begin(); it != ll.end(); ++it)
          polygonPoints.emplace_back((*it)->longitude, (*it)->latitude);
        polygonPath.push_back(polygonPoints);
      }
      else if (geomType == wkbLineString)
      {
        NFmiSvgPath svgPath;
        svgPath = query.wktGeometries.getSvgPath(tloc.loc->name);
        T::Coordinate_vec polygonPoints;
        Spine::LocationList ll =
            get_location_list(svgPath, tloc.tag, query.step, *itsPlugin.itsEngines.geoEngine);
        for (auto it = ll.begin(); it != ll.end(); ++it)
          polygonPoints.emplace_back((*it)->longitude, (*it)->latitude);
        polygonPath.push_back(polygonPoints);
      }
      else
      {
        NFmiSvgPath svgPath;
        loc = query.wktGeometries.getLocation(tloc.loc->name);
        svgPath = query.wktGeometries.getSvgPath(tloc.loc->name);
        convertSvgPathToPolygonPath(svgPath, polygonPath);
        if (polygonPath.size() > 1 && getPolygonPathLength(polygonPath) == polygonPath.size())
        {
          T::Coordinate_vec polygonPoints;
          convertToPointVector(polygonPath, polygonPoints);
          polygonPath.clear();
          polygonPath.push_back(polygonPoints);
        }
      }
      break;
    }
    case Spine::Location::Area:
    {
      NFmiSvgPath svgPath;
      loc = get_location_for_area(tloc,
                                  tloc.loc->radius * 1000,
                                  itsPlugin.itsGeometryStorage,
                                  query.language,
                                  *itsPlugin.itsEngines.geoEngine,
                                  &svgPath);
      convertSvgPathToPolygonPath(svgPath, polygonPath);
      break;
    }

    case Spine::Location::Path:
    {
      NFmiSvgPath svgPath;
      loc = get_location_for_area(tloc,
                                  tloc.loc->radius * 1000,
                                  itsPlugin.itsGeometryStorage,
                                  query.language,
                                  *itsPlugin.itsEngines.geoEngine,
                                  &svgPath);
      // convertSvgPathToPolygonPath(svgPath,polygonPath);
      Spine::LocationList locationList =
          get_location_list(svgPath, tloc.tag, query.step, *itsPlugin.itsEngines.geoEngine);
      std::vector<T::Coordinate> coordinates;
      for (const auto& ll : locationList)
      {
        coordinates.emplace_back(ll->longitude, ll->latitude);
      }
      polygonPath.emplace_back(coordinates);
      break;
    }

    case Spine::Location::BoundingBox:
    {
      std::string place = get_name_base(loc->name);
      // Split bounding box coordinates from the query string
      std::vector<std::string> parts;
      boost::algorithm::split(parts, place, boost::algorithm::is_any_of(","));
      if (parts.size() != 4)
        throw Fmi::Exception(BCP,
                             "Bounding box '" + place + "' is invalid (exactly 4 values required)");

      double lon1 = Fmi::stod(parts[0]);
      double lat1 = Fmi::stod(parts[1]);
      double lon2 = Fmi::stod(parts[2]);
      double lat2 = Fmi::stod(parts[3]);

      // Get location info by the center coordinates
      Spine::LocationPtr locCenter =
          get_bbox_location(place, query.language, *itsPlugin.itsEngines.geoEngine);

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

      loc.reset(tmp.release());

      if (tloc.loc->radius == 0)
      {
        // Create bounding box polygon

        std::vector<T::Coordinate> coordinates;
        coordinates.emplace_back(lon1, lat1);
        coordinates.emplace_back(lon1, lat2);
        coordinates.emplace_back(lon2, lat2);
        coordinates.emplace_back(lon2, lat1);
        coordinates.emplace_back(lon1, lat1);
        polygonPath.emplace_back(coordinates);
      }
      else
      {
        // Create expanded bounding box polygon

        const OGRGeometry* geom =
            itsPlugin.itsGeometryStorage.getOGRGeometry(locCenter->area, wkbMultiPolygon);
        std::unique_ptr<const OGRGeometry> newGeomUptr;
        std::unique_ptr<OGRGeometry> expandedGeomUptr;
        if (geom)
        {
          OGRGeometry* newGeom = geom->clone();
          newGeomUptr.reset(newGeom);

          auto wkt = fmt::format("MULTIPOLYGON ((({} {},{} {},{} {},{} {},{} {})))",
                                 lon1,
                                 lat1,
                                 lon1,
                                 lat2,
                                 lon2,
                                 lat2,
                                 lon2,
                                 lat1,
                                 lon1,
                                 lat1);

          const char* p = wkt.c_str();
          newGeom->importFromWkt(&p);

          auto* expandedGeom = Fmi::OGR::expandGeometry(newGeom, tloc.loc->radius);
          expandedGeomUptr.reset(expandedGeom);

          std::string wktString = Fmi::OGR::exportToWkt(*expandedGeom);
          convertWktMultipolygonToPolygonPath(wktString, polygonPath);
        }
        else
        {
          std::cout << "### GEOMETRY NOT FOUND\n";
        }
      }
      break;
    }

    default:
    {
      NFmiSvgPath svgPath;
      get_svg_path(tloc, itsPlugin.itsGeometryStorage, svgPath);
      convertSvgPathToPolygonPath(svgPath, polygonPath);
      break;
    }
  }
}

bool GridEngineQuery::processGridEngineQuery(const State& state,
                                             Query& query,
                                             TS::OutputData& outputData,
                                             const QueryServer::QueryStreamer_sptr& queryStreamer,
                                             const AreaProducers& areaproducers,
                                             const ProducerDataPeriod& producerDataPeriod) const
{
  try
  {
    if (itsPlugin.itsConfig.gridEngineDisabled())
      return false;

    Fmi::DateTime latestTimestep = query.latestTimestep;

    for (const auto& tloc : query.loptions->locations())
    {
      query.latestTimestep = latestTimestep;

      Spine::LocationPtr loc = tloc.loc;

      std::vector<std::vector<T::Coordinate>> polygonPath;

      getLocationDefinition(loc, polygonPath, tloc, query);

      T::GeometryId_set geometryIdList;
      if (areaproducers.empty() && !itsGridInterface->containsParameterWithGridProducer(query) &&
          !GridInterface::isValidDefaultRequest(
              itsPlugin.itsConfig.defaultGridGeometries(), polygonPath, geometryIdList))
      {
        outputData.clear();
        return false;
      }

      std::string country = itsPlugin.itsEngines.geoEngine->countryName(loc->iso2, query.language);
      // std::cout << formatLocation(*loc) << '\n';
      // std::cout << formatLocation(*(tloc.loc)) << '\n';

      AreaProducers producers = areaproducers;
      auto defaultProducer = itsPlugin.itsConfig.defaultProducerMappingName();

      if (producers.empty() && !defaultProducer.empty())
        producers.push_back(defaultProducer);

      itsGridInterface->processGridQuery(state,
                                         query,
                                         outputData,
                                         queryStreamer,
                                         producers,
                                         producerDataPeriod,
                                         tloc,
                                         loc,
                                         country,
                                         geometryIdList,
                                         polygonPath);
    }
    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool GridEngineQuery::isGridEngineQuery(const AreaProducers& theProducers,
                                        const Query& theQuery) const
{
  // Grid-query is executed if the following conditions are fulfilled:
  //   1. The usage of Grid-Engine is enabled (=> timeseries configuration file)
  //   2. The actual Grid-Engine is enabled (=> grid-engine configuration file)
  //   3. If the one of the following conditions is true:
  //       a) Grid-query is requested by the query parameter (source=grid)
  //       b) Query source is not defined and at least one of the producers is a grid
  //       producer c) Query source is not defined and at least one of the query parameters
  //       contains a grid producer d) Query source is not defined and no producers are
  //       defined and the primary forecast
  //          source defined in the config file is "grid".

  return (!itsPlugin.itsConfig.gridEngineDisabled() &&
          itsPlugin.itsEngines.gridEngine->isEnabled() &&
          (strcasecmp(theQuery.forecastSource.c_str(), "grid") == 0 ||
           (theQuery.forecastSource.empty() &&
            (((!theProducers.empty() && itsGridInterface->containsGridProducer(theQuery))) ||
             (itsGridInterface->containsParameterWithGridProducer(theQuery)) ||
             (theProducers.empty() &&
              strcasecmp(itsPlugin.itsConfig.primaryForecastSource().c_str(), "grid") == 0)))));
}

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
