// ======================================================================
/*!
 * \brief Smartmet GridQuery interface
 */
// ======================================================================

#pragma once

#include "Plugin.h"

#include "ProducerDataPeriod.h"
#include <engines/grid/Engine.h>
#include <grid-content/queryServer/definition/QueryStreamer.h>
#include <grid-files/common/AdditionalParameters.h>
#include <grid-files/grid/Typedefs.h>
#include <macgyver/TimeZones.h>
#include <timeseries/TimeSeriesInclude.h>

namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{
class GridInterface
{
 public:
  GridInterface(Engine::Grid::Engine* engine, const Fmi::TimeZones& timezones);

  virtual ~GridInterface() = default;

  GridInterface() = delete;
  GridInterface(const GridInterface& other) = delete;
  GridInterface& operator=(const GridInterface& other) = delete;
  GridInterface(GridInterface&& other) = delete;
  GridInterface& operator=(GridInterface&& other) = delete;

  void processGridQuery(const State& state,
                        Query& query,
                        TS::OutputData& outputData,
                        const QueryServer::QueryStreamer_sptr& queryStreamer,
                        const AreaProducers& areaproducers,
                        const ProducerDataPeriod& producerDataPeriod,
                        const Spine::TaggedLocation& tloc,
                        const Spine::LocationPtr& loc,
                        const std::string& country,
                        T::GeometryId_set& geometryIdList,
                        std::vector<std::vector<T::Coordinate>>& polygonPath);

  bool isGridProducer(const std::string& producer);

  static bool isValidDefaultRequest(const std::vector<uint>& defaultGeometries,
                                    const std::vector<std::vector<T::Coordinate>>& polygonPath,
                                    T::GeometryId_set& geometryIdList);

  bool containsGridProducer(const Query& masterquery);

  bool containsParameterWithGridProducer(const Query& masterquery);

 private:
  void prepareGridQuery(QueryServer::Query& gridQuery,
                        const Query& masterquery,
                        uint mode,
                        int origLevelId,
                        double origLevel,
                        const AreaProducers& areaproducers,
                        const Spine::TaggedLocation& tloc,
                        const Spine::LocationPtr& loc,
                        const T::GeometryId_set& geometryIdList,
                        std::vector<std::vector<T::Coordinate>>& polygonPath);

  static void insertFileQueries(QueryServer::Query& query,
                                const QueryServer::QueryStreamer_sptr& queryStreamer);

  void getDataTimes(const AreaProducers& areaproducers,
                    std::string& startTime,
                    std::string& endTime);

  static int getParameterIndex(QueryServer::Query& gridQuery, const std::string& param);

  T::ParamLevelId getLevelId(const char* producerName, const Query& masterquery);

  Engine::Grid::Engine* itsGridEngine;
  const Fmi::TimeZones& itsTimezones;

};  // class GridInterface

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
