// ======================================================================
/*!
 * \brief Smartmet TimeSeries plugin query engine query processing
 */
// ======================================================================

#pragma once

#include "Plugin.h"
#include "ProducerDataPeriod.h"
#include "QueryLevelDataCache.h"

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
class Engine;
}
}  // namespace Engine

namespace Plugin
{
namespace TimeSeries
{
class QEngineQuery
{
 public:
  QEngineQuery(const Plugin& thePlugin);

  void processQEngineQuery(const State& state,
                           Query& masterquery,
                           TS::OutputData& outputData,
                           const AreaProducers& areaproducers,
                           const ProducerDataPeriod& producerDataPeriod) const;
  Engine::Querydata::Producer selectProducer(const Spine::Location& location,
                                             const Query& query,
                                             const AreaProducers& areaproducers) const;

 private:
  void resolveAreaLocations(Query& query,
                            const State& state,
                            const AreaProducers& areaproducers) const;
  void fetchQEngineValues(const State& state,
                          const TS::ParameterAndFunctions& paramfunc,
                          int precision,
                          const Spine::TaggedLocation& tloc,
                          Query& query,
                          const AreaProducers& areaproducers,
                          const ProducerDataPeriod& producerDataPeriod,
                          QueryLevelDataCache& queryLevelDataCache,
                          TS::OutputData& outputData) const;
  TS::TimeSeriesGenerator::LocalTimeList generateQEngineQueryTimes(
      const Query& query, const std::string& paramname) const;

  void pointQuery(const Query& theQuery,
                  const std::string& theProducer,
                  const TS::ParameterAndFunctions& theParamFunc,
                  const Spine::TaggedLocation& theTLoc,
                  const TS::TimeSeriesGenerator::LocalTimeList& theQueryDataTlist,
                  const TS::TimeSeriesGenerator::LocalTimeList& theRequestedTList,
                  const std::pair<float, std::string>& theCacheKey,
                  const State& theState,
                  const Engine::Querydata::Q& theQ,
                  const NFmiPoint& theNearestPoint,
                  int thePrecision,
                  bool theLoadDataLevels,
                  boost::optional<float> thePressure,
                  boost::optional<float> theHeight,
                  QueryLevelDataCache& theQueryLevelDataCache,
                  std::vector<TS::TimeSeriesData>& theAggregateData) const;

  void areaQuery(const Query& theQuery,
                 const std::string& theProducer,
                 const TS::ParameterAndFunctions& theParamFunc,
                 const Spine::TaggedLocation& theTLoc,
                 const TS::TimeSeriesGenerator::LocalTimeList& theQueryDataTlist,
                 const TS::TimeSeriesGenerator::LocalTimeList& theRequestedTList,
                 const std::pair<float, std::string>& theCacheKey,
                 const State& theState,
                 const Engine::Querydata::Q& theQ,
                 const NFmiPoint& theNearestPoint,
                 int thePrecision,
                 bool theLoadDataLevels,
                 boost::optional<float> thePressure,
                 boost::optional<float> theHeight,
                 QueryLevelDataCache& theQueryLevelDataCache,
                 std::vector<TS::TimeSeriesData>& theAggregatedData) const;

  Spine::LocationPtr resolveLocation(const Spine::TaggedLocation& tloc,
                                     const Query& query,
                                     NFmiSvgPath& svgPath,
                                     bool& isWkt) const;
  TS::TimeSeriesGenerator::LocalTimeList generateTList(
      const Query& query,
      const std::string& producer,
      const ProducerDataPeriod& producerDataPeriod) const;
  Spine::LocationList getLocationListForPath(const Query& theQuery,
                                             const Spine::TaggedLocation& theTLoc,
                                             const std::string& place,
                                             const NFmiSvgPath svgPath,
                                             const State& theState,
                                             bool isWkt) const;
  TS::TimeSeriesGroupPtr getQEngineValuesForArea(
      const Query& theQuery,
      const std::string& theProducer,
      const TS::ParameterAndFunctions& theParamFunc,
      const Spine::TaggedLocation& theTLoc,
      const Spine::LocationPtr& loc,
      const TS::TimeSeriesGenerator::LocalTimeList& theQueryDataTlist,
      const State& theState,
      const Engine::Querydata::Q& theQ,
      const NFmiPoint& theNearestPoint,
      int thePrecision,
      bool theLoadDataLevels,
      boost::optional<float> thePressure,
      boost::optional<float> theHeight,
      const std::string& paramname,
      const Spine::LocationList& llist) const;
  Spine::LocationList getLocationListForArea(const Spine::TaggedLocation& theTLoc,
                                             const Spine::LocationPtr& loc,
                                             const Engine::Querydata::Q& theQ,
                                             NFmiSvgPath& svgPath,
                                             bool isWkt) const;

  const Plugin& itsPlugin;
};

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
