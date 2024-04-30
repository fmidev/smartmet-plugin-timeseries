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
                      GridInterface() = delete;
                      GridInterface(const GridInterface& other) = delete;
                      GridInterface(GridInterface&& other) = delete;
    virtual           ~GridInterface() = default;

      GridInterface&  operator=(const GridInterface& other) = delete;
      GridInterface&  operator=(GridInterface&& other) = delete;

      bool            containsGridProducer(const Query& masterquery);
      bool            containsParameterWithGridProducer(const Query& masterquery);
      bool            isGridProducer(const std::string& producer);

      static bool     isValidDefaultRequest(const std::vector<uint>& defaultGeometries,
                        const std::vector<std::vector<T::Coordinate>>& polygonPath,T::GeometryId_set& geometryIdList);

      void            processGridQuery(const State& state,Query& query,TS::OutputData& outputData,
                        const QueryServer::QueryStreamer_sptr& queryStreamer,const AreaProducers& areaproducers,
                        const ProducerDataPeriod& producerDataPeriod,const Spine::TaggedLocation& tloc,
                        const Spine::LocationPtr& loc,const std::string& country,T::GeometryId_set& geometryIdList,
                        std::vector<std::vector<T::Coordinate>>& polygonPath);

  private:

      void            exteractCoordinatesAndAggrecationTimes(std::shared_ptr<QueryServer::Query>& gridQuery,
                        Fmi::TimeZonePtr tz,T::Coordinate_vec& coordinates,
                        std::set<Fmi::LocalDateTime>& aggregationTimes);

      void            exteractQueryResult(std::shared_ptr<QueryServer::Query>& gridQuery,const State& state,
                        Query& masterquery,TS::OutputData& outputData,const QueryServer::QueryStreamer_sptr& queryStreamer,
                        const AreaProducers& areaproducers,Fmi::TimeZonePtr tz,const Spine::TaggedLocation& tloc,
                        const Spine::LocationPtr& loc,const std::string& country,int levelId,double level);

      void            getDataTimes(const AreaProducers& areaproducers,std::string& startTime,std::string& endTime);
      static int      getParameterIndex(QueryServer::Query& gridQuery, const std::string& param);

      void            findLevelId(Query& masterquery,const AreaProducers& areaproducers,int& levelId,std::string& geometryIdStr);
      void            findLevels(Query& masterquery,const AreaProducers& areaproducers,uint mode,int& levelId,std::vector<double>& levels);

      static void     insertFileQueries(QueryServer::Query& query,const QueryServer::QueryStreamer_sptr& queryStreamer);
      bool            isBuildInParameter(const char *parameter);

      void            prepareProducer(QueryServer::Query& gridQuery,const Query& masterquery,int origLevelId,
                        const AreaProducers& areaproducers,int& levelId,int& geometryId);

      void            prepareGeneration(QueryServer::Query& gridQuery,const Query& masterquery,
                        bool& sameParamAnalysisTime);

      void            prepareLocation(QueryServer::Query& gridQuery,const Query& masterquery,
                        const Spine::LocationPtr& loc,const T::GeometryId_set& geometryIdList,
                        std::vector<std::vector<T::Coordinate>>& polygonPath,uchar& locationType);

      void            prepareQueryTimes(QueryServer::Query& gridQuery,const Query& masterquery,
                        const Spine::LocationPtr& loc);

      void            prepareQueryParameters(QueryServer::Query& gridQuery,const Query& masterquery,
                        uint mode,int levelId,int geometryId,uchar locationType,bool sameParamAnalysisTime,
                        double origLevel,const AreaProducers& areaproducers);

      void            prepareGridQuery(QueryServer::Query& gridQuery,const Query& masterquery,
                        uint mode,int origLevelId,double origLevel,const AreaProducers& areaproducers,
                        const Spine::TaggedLocation& tloc,const Spine::LocationPtr& loc,
                        const T::GeometryId_set& geometryIdList,std::vector<std::vector<T::Coordinate>>& polygonPath);


  private:

      Engine::Grid::Engine* itsGridEngine;
      const Fmi::TimeZones& itsTimezones;

};  // class GridInterface

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
