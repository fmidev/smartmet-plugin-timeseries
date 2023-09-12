// ======================================================================
/*!
 * \brief Smartmet TimeSeries plugin observation engine query processing
 */
// ======================================================================

#pragma once

#include "Plugin.h"
#include "ObsParameter.h"
#include "ProducerDataPeriod.h"

namespace SmartMet
{
namespace Engine
{
namespace Observation
{
class Engine;
}
}  // namespace Engine

namespace Plugin
{
namespace TimeSeries
{

struct SettingsInfo
{
  Engine::Observation::Settings settings;
  bool is_area = false;
  std::string area_name;

  SettingsInfo(Engine::Observation::Settings s, bool isa, std::string an)
      : settings(std::move(s)), is_area(isa), area_name(std::move(an))
  {
  }
};

class ObsEngineQuery
{
public:
  ObsEngineQuery(const Plugin& thePlugin);

#ifndef WITHOUT_OBSERVATION
  void processObsEngineQuery(const State& state,
                             Query& query,
                             TS::OutputData& outputData,
                             const AreaProducers& areaproducers,
                             const ProducerDataPeriod& producerDataPeriod,
                             const ObsParameters& obsParameters) const;
  bool isObsProducer(const std::string& producer) const;
  std::vector<ObsParameter> getObsParameters(const Query& query) const;

private:

  void fetchObsEngineValuesForPlaces(const State& state,
									 const std::string& producer,
									 const ObsParameters& obsParameterss,
									 Engine::Observation::Settings& settings,
									 Query& query,
									 TS::OutputData& outputData) const;
  void fetchObsEngineValuesForArea(const State& state,
								   const std::string& producer,
								   const ObsParameters& obsParameters,
								   const std::string& areaName,
								   Engine::Observation::Settings& settings,
								   Query& query,
								   TS::OutputData& outputData) const;
  void getObsSettings(std::vector<SettingsInfo>& settingsVector,
					  const std::string& producer,
					  const ProducerDataPeriod& producerDataPeriod,
					  const boost::posix_time::ptime& now,
					  const ObsParameters& obsParameters,
					  Query& query) const;
  void getCommonObsSettings(Engine::Observation::Settings& settings,
							const std::string& producer,
							Query& query) const;
  bool resolveAreaStations(const Spine::LocationPtr& location,
						   const std::string& producer,
						   const Query& query,
						   Engine::Observation::Settings& settings,
						   std::string& name) const;
  void resolveStationsForPath(const std::string& producer,
							  const Spine::LocationPtr& loc,
							  const std::string& loc_name_original,
							  const std::string& loc_name,										 
							  const Query& query,
							  const Engine::Observation::Settings& settings,
							  bool isWkt,
							  std::string& wktString,
							  Engine::Observation::StationSettings& stationSettings) const;
  void resolveStationsForArea(const std::string& producer,
							  const Spine::LocationPtr& loc,
							  const std::string& loc_name_original,
							  const std::string& loc_name,										 
							  const Query& query,
							  const Engine::Observation::Settings& settings,
							  bool isWkt,
							  std::string& wktString,
							  Engine::Observation::StationSettings& stationSettings) const;
  void resolveStationsForBBox(const std::string& producer,
							  const Spine::LocationPtr& loc,
							  const std::string& loc_name_original,
							  const std::string& loc_name,										 
							  const Query& query,
							  const Engine::Observation::Settings& settings,
							  bool isWkt,
							  std::string& wktString,
							  Engine::Observation::StationSettings& stationSettings) const;
  void resolveStationsForPlaceWithRadius(const std::string& producer,
										 const Spine::LocationPtr& loc,
										 const std::string& loc_name,										 
										 const Engine::Observation::Settings& settings,
										 std::string& wktString,
										 Engine::Observation::StationSettings& stationSettings) const;
  void resolveStationsForCoordinatePointWithRadius(const std::string& producer,
												   const Spine::LocationPtr& loc,
												   const std::string& loc_name_original,
												   const std::string& loc_name,										 
												   const Query& query,
												   const Engine::Observation::Settings& settings,
												   bool isWkt,
												   std::string& wktString,
												   Engine::Observation::StationSettings& stationSettings) const;
  TS::TimeSeriesVectorPtr handleObsParametersForPlaces(const State& state,
													   const std::string& producer,
													   const Spine::LocationPtr& loc,
													   const Query& query,
													   const ObsParameters& obsParameters,
													   const TS::TimeSeriesVectorPtr& observation_result,
													   const std::vector<boost::local_time::local_date_time>& timestep_vector,
													   std::map<std::string, unsigned int>& parameterResultIndexes) const;
  TS::TimeSeriesVectorPtr doAggregationForPlaces(const State& state,
												 const ObsParameters& obsParameters,
												 const TS::TimeSeriesVectorPtr& observation_result,
												 std::map<std::string, unsigned int>& parameterResultIndexes) const;
  void handleSpecialParameter(const ObsParameter& obsparam,
							  const std::string& areaName,
							  TS::TimeSeriesGroupPtr& tsg) const;
  TS::TimeSeriesVectorPtr handleObsParametersForArea(const State& state,
													 const std::string& producer,
													 const Spine::LocationPtr& loc,
													 const ObsParameters& obsParameters,
													 const TS::TimeSeriesVector* tsv_observation_result,
													 const std::vector<boost::local_time::local_date_time>& ts_vector,
													 const Query& query) const;

  void handleLocationSettings(const Query& query,
							  const std::string& producer,
							  const Spine::TaggedLocation& tloc,
							  Engine::Observation::Settings& settings,
							  std::vector<SettingsInfo>& settingsVector,
							  Engine::Observation::StationSettings& stationSettings) const;

#endif

  const Plugin& itsPlugin;
};

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
