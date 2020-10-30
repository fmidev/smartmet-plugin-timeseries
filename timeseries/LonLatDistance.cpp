#include "LonLatDistance.h"

#include <boost/math/constants/constants.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
/// @brief Earth's quatratic mean radius for WGS-84
static const double EARTH_RADIUS_IN_METERS = 6372797.560856;

double deg_to_rad(const double& degrees)
{
  return degrees * (boost::math::constants::pi<double>() / 180.0);
}

double rad_to_deg(const double& radians)
{
  return radians * (180.0 / boost::math::constants::pi<double>());
}

/**
 * Returns the (initial) bearing from this point to the supplied point, in degrees
 *   see http://williams.best.vwh.net/avform.htm#Crs
 *
 * @param   {LatLon} point: Latitude/longitude of destination point
 * @returns {Number} Initial bearing in degrees from North
 */
double initial_bearing(const std::pair<double, double>& from, const std::pair<double, double>& to)
{
  double lat1 = deg_to_rad(from.second);
  double lat2 = deg_to_rad(to.second);
  double dLon = deg_to_rad(to.first - from.first);

  double y = sin(dLon) * cos(lat2);
  double x = (cos(lat1) * sin(lat2)) - (sin(lat1) * cos(lat2) * cos(dLon));
  double brng = atan2(y, x);

  return fmod((rad_to_deg(brng) + 360.0), 360.0);
}

/**
 * Returns final bearing arriving at supplied destination point from this point; the final bearing
 * will differ from the initial bearing by varying degrees according to distance and latitude
 *
 * @param   {LatLon} point: Latitude/longitude of destination point
 * @returns {Number} Final bearing in degrees from North
 */
double final_bearing(const std::pair<double, double>& from, const std::pair<double, double>& to)
{
  // get initial bearing from supplied point back to this point...
  double lat1 = deg_to_rad(to.second);
  double lat2 = deg_to_rad(from.second);
  double dLon = deg_to_rad(from.first - to.first);

  double y = sin(dLon) * cos(lat2);
  double x = (cos(lat1) * sin(lat2)) - (sin(lat1) * cos(lat2) * cos(dLon));
  double brng = atan2(y, x);
  // ... & reverse it by adding 180°
  return fmod((rad_to_deg(brng) + 180.0), 360.0);
}

/**
 * Returns the midpoint between this point and the supplied point.
 *   see http://mathforum.org/library/drmath/view/51822.html for derivation
 *
 * @param   {LatLon} point: Latitude/longitude of destination point
 * @returns {LatLon} Midpoint between this point and the supplied point
 */
std::pair<double, double> midpoint(const std::pair<double, double>& from,
                                   const std::pair<double, double>& to)
{
  double lat1 = deg_to_rad(from.second);
  double lon1 = deg_to_rad(from.first);
  double lat2 = deg_to_rad(to.second);
  double dLon = deg_to_rad(to.first - from.first);

  double Bx = cos(lat2) * cos(dLon);
  double By = cos(lat2) * sin(dLon);

  double lat3 = atan2(sin(lat1) + sin(lat2), sqrt((cos(lat1) + Bx) * (cos(lat1) + Bx) + By * By));
  double lon3 = lon1 + atan2(By, cos(lat1) + Bx);
  double pi(boost::math::constants::pi<double>());
  lon3 = fmod(lon3 + (3 * pi), 2 * pi) - pi;  // normalise to -180..+180º

  return {rad_to_deg(lon3), rad_to_deg(lat3)};
}

/**
 * Returns the destination point from this point having travelled the given distance (in km) on the
 * given initial bearing (bearing may vary before destination is reached)
 *
 *   see http://williams.best.vwh.net/avform.htm#LL
 *
 * @param   {Number} brng: Initial bearing in degrees
 * @param   {Number} dist: Distance in km
 * @returns {LatLon} Destination point
 */
std::pair<double, double> destination_point(const std::pair<double, double>& from,
                                            const std::pair<double, double>& to,
                                            const double& distance)
{
  double brng(initial_bearing(from, to));

  double dist =
      distance / (EARTH_RADIUS_IN_METERS / 1000.0);  // convert dist to angular distance in radians
  brng = deg_to_rad(brng);
  double lat1 = deg_to_rad(from.second);
  double lon1 = deg_to_rad(from.first);

  double lat2 = asin(sin(lat1) * cos(dist) + cos(lat1) * sin(dist) * cos(brng));
  double lon2 = lon1 + atan2(sin(brng) * sin(dist) * cos(lat1), cos(dist) - sin(lat1) * sin(lat2));
  double pi(boost::math::constants::pi<double>());
  lon2 = fmod(lon2 + 3 * pi, 2 * pi) - pi;  // normalise to -180..+180º

  return {rad_to_deg(lon2), rad_to_deg(lat2)};
}

/** @brief Computes the arc, in radian, between two WGS-84 positions.
 *
 * The result is equal to <code>Distance(from,to)/EARTH_RADIUS_IN_METERS</code>
 *    <code>= 2*asin(sqrt(h(d/EARTH_RADIUS_IN_METERS )))</code>
 *
 * where:<ul>
 *    <li>d is the distance in meters between 'from' and 'to' positions.</li>
 *    <li>h is the haversine function: <code>h(x)=sin²(x/2)</code></li>
 * </ul>
 *
 * The haversine formula gives:
 *    <code>h(d/R) = h(from.lat-to.lat)+h(from.lon-to.lon)+cos(from.lat)*cos(to.lat)</code>
 *
 * @sa http://en.wikipedia.org/wiki/Law_of_haversines
 */
double arc_in_radians(const std::pair<double, double>& from, const std::pair<double, double>& to)
{
  double latitudeArc = deg_to_rad(from.second - to.second);
  double longitudeArc = deg_to_rad(from.first - to.first);
  double latitudeH = sin(latitudeArc * 0.5);
  latitudeH *= latitudeH;
  double lontitudeH = sin(longitudeArc * 0.5);
  lontitudeH *= lontitudeH;
  double tmp = cos(deg_to_rad(from.second)) * cos(deg_to_rad(to.second));
  return 2.0 * asin(sqrt(latitudeH + tmp * lontitudeH));
}

/** @brief Computes the distance, in meters, between two WGS-84 positions.
 *
 * The result is equal to <code>EARTH_RADIUS_IN_METERS*ArcInRadians(from,to)</code>
 *
 * @sa ArcInRadians
 */
double distance_in_meters(const std::pair<double, double>& from,
                          const std::pair<double, double>& to)
{
  return EARTH_RADIUS_IN_METERS * arc_in_radians(from, to);
}

double distance_in_kilometers(const std::pair<double, double>& from,
                              const std::pair<double, double>& to)
{
  return (distance_in_meters(from, to) / 1000.0);
}

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
