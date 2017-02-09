#pragma once

#include <utility>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
/** @brief Computes the distance between two WGS-84 positions.
 *
 */
double distance_in_meters(const std::pair<double, double>& from,
                          const std::pair<double, double>& to);
double distance_in_kilometers(const std::pair<double, double>& from,
                              const std::pair<double, double>& to);

/**
 * Returns the destination point from this point having travelled the given distance (in km) on the
 * given initial bearing (bearing may vary before destination is reached)
 *
 */
std::pair<double, double> destination_point(const std::pair<double, double>& from,
                                            const std::pair<double, double>& to,
                                            const double& distance);

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
