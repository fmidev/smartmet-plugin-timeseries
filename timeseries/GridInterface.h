// ======================================================================
/*!
 * \brief Smartmet GridQuery interface
 */
// ======================================================================

#pragma once

#include "Plugin.h"

#include <grid-files/common/AdditionalParameters.h>
#include <grid-files/grid/Typedefs.h>
#include <macgyver/TimeZones.h>
#include <spine/TimeSeriesGenerator.h>
#include <engines/grid/Engine.h>
#include <grid-content/queryServer/definition/QueryStreamer.h>



namespace SmartMet
{
namespace Plugin
{
namespace TimeSeries
{

namespace ts = SmartMet::Spine::TimeSeries;


class GridInterface
{
  public:

                            GridInterface(
                                Engine::Grid::Engine* engine,
                                const Fmi::TimeZones& timezones);

    virtual                 ~GridInterface();

    void                    processGridQuery(const State& state,
                                Query& query,
                                OutputData& outputData,
                                QueryServer::QueryStreamer_sptr queryStreamer,
                                const AreaProducers& areaproducers,
                                const ProducerDataPeriod& producerDataPeriod,
                                const Spine::TaggedLocation& tloc,
                                const Spine::LocationPtr loc,
                                std::string country,
                                std::vector<std::vector<T::Coordinate>>& polygonPath);

    bool                    isGridProducer(const std::string& producer);
    bool                    containsGridProducer(const Query& masterquery);
    bool                    containsParameterWithGridProducer(const Query& masterquery);

 private:

    void                    prepareGridQuery(
                                QueryServer::Query& gridQuery,
                                AdditionalParameters& additionalParameters,
                                const Query& masterquery,
                                uint mode,
                                int origLevelId,
                                double origLevel,
                                const AreaProducers& areaproducers,
                                const Spine::TaggedLocation& tloc,
                                const Spine::LocationPtr loc,
                                std::vector<std::vector<T::Coordinate>>& polygonPath);

    void                    insertFileQueries(QueryServer::Query& query,QueryServer::QueryStreamer_sptr queryStreamer);

    void                    getDataTimes(
                                const AreaProducers& areaproducers,
                                std::string& startTime,
                                std::string& endTime);

    int                     getParameterIndex(
                                QueryServer::Query& gridQuery,std::string param);


    void                    erase_redundant_timesteps(
                                ts::TimeSeries& ts,
                                std::set<boost::local_time::local_date_time>& aggregationTimes);

    ts::TimeSeriesPtr       erase_redundant_timesteps(
                                ts::TimeSeriesPtr ts,
                                std::set<boost::local_time::local_date_time>& aggregationTimes);

    ts::TimeSeriesVectorPtr erase_redundant_timesteps(
                                ts::TimeSeriesVectorPtr tsv,
                                std::set<boost::local_time::local_date_time>& aggregationTimes);

    ts::TimeSeriesGroupPtr  erase_redundant_timesteps(
                                ts::TimeSeriesGroupPtr tsg,
                                std::set<boost::local_time::local_date_time>& aggregationTimes);

    T::ParamLevelId         getLevelId(const char *producerName,const Query& masterquery);


 private:


    Engine::Grid::Engine*   itsGridEngine;
    const Fmi::TimeZones&   itsTimezones;
    string_vec              itsProducerList;
    time_t                  itsProducerList_updateTime;
    T::ProducerInfoList     itsProducerInfoList;
    time_t                  itsProducerInfoList_updateTime;
    T::GenerationInfoList   itsGenerationInfoList;
    time_t                  itsGenerationInfoList_updateTime;

};  // class GridInterface

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
