#include "OptionParsers.h"
#include <boost/date_time/posix_time/ptime.hpp>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <spine/Convenience.h>
#include <stdexcept>

namespace SmartMet
{
namespace TimeSeries
{
namespace OptionParsers
{
namespace
{
int get_parameter_index(const ParameterList& parameters, const std::string& paramname)
{
  for (unsigned int i = 0; i < parameters.size(); i++)
    if (parameters.at(i).name() == paramname)
      return i;

  return -1;
}
}  // anonymous namespace

void ParameterOptions::expandParameter(const std::string& paramname)
{
  // Expand parameter in index
  int expand_index = get_parameter_index(itsParameters, paramname);

  if (expand_index > -1)
  {
    const DataFunctions& expand_func = itsParameterFunctions.at(expand_index).functions;

    std::vector<Spine::Parameter> dataSourceParams;
    std::vector<ParameterAndFunctions> dataSourceParameterFunctions;
    for (const auto& p : itsParameters)
    {
      if (p.type() == Spine::Parameter::Type::Data ||
          p.type() == Spine::Parameter::Type::Landscaped)
      {
        if (!p.getSensorParameter().empty())
        {
          // data_source field is not added to qc-field
          continue;
        }
        std::string name = p.name() + "_data_source";
        Fmi::ascii_tolower(name);

        Spine::Parameter param(name, p.type(), p.number());
        if (p.getSensorNumber())
          param.setSensorNumber(*(p.getSensorNumber()));
        ParameterAndFunctions paramfunc(param, expand_func);
        dataSourceParams.push_back(param);
        dataSourceParameterFunctions.push_back(paramfunc);
      }
    }

    if (!dataSourceParams.empty())
    {
      // Replace the original parameter with expanded list of parameters
      itsParameters.erase(itsParameters.begin() + expand_index);
      itsParameters.insert(
          itsParameters.begin() + expand_index, dataSourceParams.begin(), dataSourceParams.end());
      itsParameterFunctions.erase(itsParameterFunctions.begin() + expand_index);
      itsParameterFunctions.insert(itsParameterFunctions.begin() + expand_index,
                                   dataSourceParameterFunctions.begin(),
                                   dataSourceParameterFunctions.end());
    }
  }
}

void ParameterOptions::add(const Spine::Parameter& theParam)
{
  itsParameters.push_back(theParam);
  itsParameterFunctions.push_back(ParameterAndFunctions(theParam, DataFunctions()));
}

void ParameterOptions::add(const Spine::Parameter& theParam, const DataFunctions& theParamFunctions)
{
  itsParameters.push_back(theParam);
  itsParameterFunctions.push_back(ParameterAndFunctions(theParam, theParamFunctions));
}

ParameterOptions parseParameters(const Spine::HTTP::Request& theReq)
{
  try
  {
    ParameterOptions options;

    // Get the option string
    std::string opt =
        Spine::required_string(theReq.getParameter("param"), "Option param is required");

    // Protect against empty selection
    if (opt.empty())
      throw Fmi::Exception(BCP, "param option is empty");

    // Split

    std::vector<std::string> names;
    boost::algorithm::split(names, opt, boost::algorithm::is_any_of(","));

    // Validate and convert

    for (const std::string& name : names)
    {
      options.add(ParameterFactory::instance().parse(name));
    }

    return options;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace OptionParsers
}  // namespace TimeSeries
}  // namespace SmartMet
