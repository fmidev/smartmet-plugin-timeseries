#pragma once

#include "ParameterFactory.h"
#include <spine/HTTP.h>
#include <spine/Parameter.h>
#include <vector>

namespace SmartMet
{
namespace TimeSeries
{
namespace OptionParsers
{
using ParameterList = std::vector<Spine::Parameter>;
using ParameterFunctionList = std::vector<ParameterAndFunctions>;

class ParameterOptions
{
 public:
  const ParameterList& parameters() const { return itsParameters; }
  const ParameterFunctionList& parameterFunctions() const { return itsParameterFunctions; }
  bool empty() const { return itsParameters.empty(); }
  ParameterList::size_type size() const { return itsParameters.size(); }
  void add(const Spine::Parameter& theParam);
  void add(const Spine::Parameter& theParam, const DataFunctions& theParamFunctions);
  void expandParameter(const std::string& paramname);

 private:
  ParameterList itsParameters;
  ParameterFunctionList itsParameterFunctions;

};  // class ParameterOptions

ParameterOptions parseParameters(const Spine::HTTP::Request& theReq);

}  // namespace OptionParsers
}  // namespace TimeSeries
}  // namespace SmartMet
