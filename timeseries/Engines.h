#pragma once

#include <memory>

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
  std::shared_ptr<Engine::Querydata::Engine> qEngine = nullptr;
  std::shared_ptr<Engine::Geonames::Engine> geoEngine = nullptr;
  std::shared_ptr<Engine::Gis::Engine> gisEngine = nullptr;
  std::shared_ptr<Engine::Grid::Engine> gridEngine = nullptr;
#ifndef WITHOUT_OBSERVATION
  std::shared_ptr<Engine::Observation::Engine> obsEngine = nullptr;
#endif
};

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet
