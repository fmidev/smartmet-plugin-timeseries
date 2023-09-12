namespace SmartMet
{
  /*
namespace Engine
{
namespace Querydata
{
class Engine;
}
namespace Geonames
{
class Engine;
}
namespace Gis
{
class Engine;
}
namespace Grid
{
class Engine;
}
}  // namespace Engine
  */
namespace Plugin
{
namespace TimeSeries
{

struct Engines
{
  Engine::Querydata::Engine* qEngine = nullptr;
  Engine::Geonames::Engine* geoEngine = nullptr;
  Engine::Gis::Engine* gisEngine = nullptr;
  Engine::Grid::Engine* gridEngine = nullptr;
#ifndef WITHOUT_OBSERVATION
  Engine::Observation::Engine* obsEngine = nullptr;
#endif
};

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
