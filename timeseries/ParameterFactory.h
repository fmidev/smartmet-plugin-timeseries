// ======================================================================
/*!
 * \brief Generate parameter descriptor from parameter name
 */
// ======================================================================

#pragma once

#include "DataFunction.h"

#include <newbase/NFmiEnumConverter.h>
#include <spine/Parameter.h>
#include <string>

namespace SmartMet
{
namespace TimeSeries
{
struct ParameterAndFunctions
{
  Spine::Parameter parameter;
  DataFunctions functions;

  ParameterAndFunctions(const Spine::Parameter& param, const DataFunctions& pfs)
      : parameter(param), functions(pfs)
  {
  }

  friend std::ostream& operator<<(std::ostream& out, const ParameterAndFunctions& paramfuncs);
};

std::ostream& operator<<(std::ostream& out, const ParameterAndFunctions& paramfuncs);

class ParameterFactory
{
 private:
  ParameterFactory();
  mutable NFmiEnumConverter converter;

  std::string parse_parameter_functions(const std::string& theParameterRequest,
                                        std::string& theOriginalName,
                                        std::string& theParameterNameAlias,
                                        DataFunction& theInnerDataFunction,
                                        DataFunction& theOuterDataFunction) const;

 public:
  static const ParameterFactory& instance();
  Spine::Parameter parse(const std::string& name, bool ignoreBadParameter = false) const;
  ParameterAndFunctions parseNameAndFunctions(const std::string& name,
                                              bool ignoreBadParameter = false) const;

  // Newbase parameter conversion
  std::string name(int number) const;
  int number(const std::string& name) const;

};  // class ParameterFactory
}  // namespace TimeSeries
}  // namespace SmartMet

// ======================================================================
