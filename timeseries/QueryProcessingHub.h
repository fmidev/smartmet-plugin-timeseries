// ======================================================================
/*!
 * \brief Smartmet TimeSeries plugin query processing hub
 */
// ======================================================================

#pragma once

#include "GridEngineQuery.h"
#include "ObsEngineQuery.h"
#include "Plugin.h"
#include "ProducerDataPeriod.h"
#include "QEngineQuery.h"
#include <spine/HTTP.h>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
class QueryProcessingHub
{
 public:
  QueryProcessingHub(const Plugin& thePlugin);

  std::shared_ptr<std::string> processQuery(const State& state,
                                              Spine::Table& table,
                                              Query& masterquery,
                                              const QueryServer::QueryStreamer_sptr& queryStreamer,
                                              size_t& product_hash);

  std::size_t hash_value(const State& state,
                         const Spine::HTTP::Request& request,
                         Query masterquery) const;

 private:
  enum class LocationHash
  {
    NoData,      // no producer has data for the location
    Hashed,      // the location's time series is in the hash
    Uncacheable  // the request cannot be cached
  };
  Query location_subquery(const State& state,
                          const Query& q,
                          const Spine::TaggedLocation& tloc,
                          const AreaProducers& areaproducers,
                          const ProducerDataPeriod& producerDataPeriod,
                          const Fmi::DateTime& first_timestep,
                          bool& firstProducer) const;
  LocationHash hash_location_timeseries(const State& state,
                                        const Query& masterquery,
                                        const Query& q,
                                        const Spine::TaggedLocation& tloc,
                                        const AreaProducers& areaproducers,
                                        Query& subquery,
                                        std::set<std::size_t>& handled_timeseries,
                                        std::size_t& hash) const;

  QEngineQuery itsQEngineQuery;
  ObsEngineQuery itsObsEngineQuery;
  GridEngineQuery itsGridEngineQuery;
};

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
