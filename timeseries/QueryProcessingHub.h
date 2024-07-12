// ======================================================================
/*!
 * \brief Smartmet TimeSeries plugin query processing hub
 */
// ======================================================================

#pragma once

#include "GridEngineQuery.h"
#include "ObsEngineQuery.h"
#include "Plugin.h"
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
  QEngineQuery itsQEngineQuery;
  ObsEngineQuery itsObsEngineQuery;
  GridEngineQuery itsGridEngineQuery;
};

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
