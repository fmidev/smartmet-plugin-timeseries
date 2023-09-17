#include "ParameterFactory.h"
#include <boost/algorithm/string.hpp>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <spine/Convenience.h>
#include <spine/Parameter.h>
#include <spine/Parameters.h>
#include <stdexcept>

namespace SmartMet
{
namespace TimeSeries
{
// ----------------------------------------------------------------------
/*!
 * \brief Print parameter and functions
 */
// ----------------------------------------------------------------------

std::ostream& operator<<(std::ostream& out, const ParameterAndFunctions& paramfuncs)
{
  out << "parameter =\t" << paramfuncs.parameter << "\n"
      << "functions =\t" << paramfuncs.functions << "\n";
  return out;
}

namespace
{

int get_function_index(const std::string& theFunction)
{
  static const char* names[] = {"mean_a",
                                "mean_t",
                                "nanmean_a",
                                "nanmean_t",
                                "max_a",
                                "max_t",
                                "nanmax_a",
                                "nanmax_t",
                                "min_a",
                                "min_t",
                                "nanmin_a",
                                "nanmin_t",
                                "median_a",
                                "median_t",
                                "nanmedian_a",
                                "nanmedian_t",
                                "sum_a",
                                "sum_t",
                                "nansum_a",
                                "nansum_t",
                                "integ_a",
                                "integ_t",
                                "naninteg_a",
                                "naninteg_t",
                                "sdev_a",
                                "sdev_t",
                                "nansdev_a",
                                "nansdev_t",
                                "percentage_a",
                                "percentage_t",
                                "nanpercentage_a",
                                "nanpercentage_t",
                                "count_a",
                                "count_t",
                                "nancount_a",
                                "nancount_t",
                                "change_a",
                                "change_t",
                                "nanchange_a",
                                "nanchange_t",
                                "trend_a",
                                "trend_t",
                                "nantrend_a",
                                "nantrend_t",
                                "nearest_t",
                                "nannearest_t",
                                "interpolate_t",
                                "naninterpolate_t",

                                "interpolatedir_t",
                                "naninterpolatedir_t",
                                "meandir_t",
                                "nanmeandir_t",
                                "sdevdir_t",
                                "nansdevdir_t",
                                ""};

  std::string func_name(theFunction);

  // If ending is missing, add area aggregation ending
  if (func_name.find("_a") == std::string::npos && func_name.find("_t") == std::string::npos)
    func_name += "_a";

  for (unsigned int i = 0; strlen(names[i]) > 0; i++)
  {
    if (names[i] == func_name)
      return i;
  }

  return -1;
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse a function id
 *
 * Throws if the name is not recognized.
 *
 * \param theFunction The name of the function
 * \return The respective enumerated value
 */
// ----------------------------------------------------------------------

FunctionId parse_function(const std::string& theFunction)
{
  try
  {
    static const FunctionId functions[] = {FunctionId::Mean,
                                           FunctionId::Mean,
                                           FunctionId::Mean,
                                           FunctionId::Mean,
                                           FunctionId::Maximum,
                                           FunctionId::Maximum,
                                           FunctionId::Maximum,
                                           FunctionId::Maximum,
                                           FunctionId::Minimum,
                                           FunctionId::Minimum,
                                           FunctionId::Minimum,
                                           FunctionId::Minimum,
                                           FunctionId::Median,
                                           FunctionId::Median,
                                           FunctionId::Median,
                                           FunctionId::Median,
                                           FunctionId::Sum,
                                           FunctionId::Sum,
                                           FunctionId::Sum,
                                           FunctionId::Sum,
                                           FunctionId::Sum,
                                           FunctionId::Integ,
                                           FunctionId::Sum,
                                           FunctionId::Integ,
                                           FunctionId::StandardDeviation,
                                           FunctionId::StandardDeviation,
                                           FunctionId::StandardDeviation,
                                           FunctionId::StandardDeviation,
                                           FunctionId::Percentage,
                                           FunctionId::Percentage,
                                           FunctionId::Percentage,
                                           FunctionId::Percentage,
                                           FunctionId::Count,
                                           FunctionId::Count,
                                           FunctionId::Count,
                                           FunctionId::Count,
                                           FunctionId::Change,
                                           FunctionId::Change,
                                           FunctionId::Change,
                                           FunctionId::Change,
                                           FunctionId::Trend,
                                           FunctionId::Trend,
                                           FunctionId::Trend,
                                           FunctionId::Trend,
                                           FunctionId::Nearest,
                                           FunctionId::Nearest,
                                           FunctionId::Interpolate,
                                           FunctionId::Interpolate,

                                           FunctionId::Interpolate,
                                           FunctionId::Interpolate,
                                           FunctionId::Mean,
                                           FunctionId::Mean,
                                           FunctionId::StandardDeviation,
                                           FunctionId::StandardDeviation};

    int function_index = get_function_index(theFunction);
    if (function_index >= 0)
      return functions[function_index];

    throw Fmi::Exception(BCP, "Unrecognized function name '" + theFunction + "'!");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void parse_intervals(std::string& paramname,
                     unsigned int& aggregation_interval_behind,
                     unsigned int& aggregation_interval_ahead)
{
  try
  {
    std::string intervalSeparator(":");
    if (paramname.find('/') != std::string::npos)
      intervalSeparator = "/";
    else if (paramname.find(';') != std::string::npos)
      intervalSeparator = ";";
    else if (paramname.find(':') != std::string::npos)
      intervalSeparator = ":";

    if (paramname.find(intervalSeparator) != std::string::npos)
    {
      std::string aggregation_interval_string_behind =
          paramname.substr(paramname.find(intervalSeparator) + 1);
      std::string aggregation_interval_string_ahead = "0";
      paramname.resize(paramname.find(intervalSeparator));

      int agg_interval_behind = 0;
      int agg_interval_ahead = 0;
      // check if second aggregation interval is defined
      if (aggregation_interval_string_behind.find(intervalSeparator) != std::string::npos)
      {
        aggregation_interval_string_ahead = aggregation_interval_string_behind.substr(
            aggregation_interval_string_behind.find(intervalSeparator) + 1);
        aggregation_interval_string_behind.resize(
            aggregation_interval_string_behind.find(intervalSeparator));
        agg_interval_ahead = Spine::duration_string_to_minutes(aggregation_interval_string_ahead);
        aggregation_interval_ahead = boost::numeric_cast<unsigned int>(agg_interval_ahead);
      }

      agg_interval_behind = Spine::duration_string_to_minutes(aggregation_interval_string_behind);

      if (agg_interval_behind < 0 || agg_interval_ahead < 0)
      {
        throw Fmi::Exception(BCP,
                             "The 'interval' option for '" + paramname + "' must be positive!");
      }
      aggregation_interval_behind = boost::numeric_cast<unsigned int>(agg_interval_behind);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract the function modifier from a function call string
 *
 * This parses strings of the following forms
 *
 *  - name
 *  - name[lo:hi]
 *
 * \param theString The function call to parse
 * \return The function modified alone (possibly empty string)
 */
// ----------------------------------------------------------------------
std::string extract_limits(const std::string& theString)
{
  try
  {
    auto pos1 = theString.find('[');
    if (pos1 == std::string::npos)
      return "";

    auto pos2 = theString.find(']', pos1);
    if (pos2 == std::string::npos && pos2 != theString.size() - 1)
      throw Fmi::Exception(BCP, "Invalid function modifier in '" + theString + "'!");

    return theString.substr(pos1 + 1, pos2 - pos1 - 1);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract the function name from a function call string
 *
 * This parses strings of the following forms
 *
 *  - name
 *  - name[lo:hi]
 *
 * \param theString The function call to parse
 * \return The function name alone
 */
// ----------------------------------------------------------------------

std::string extract_function(const std::string& theString,
                             double& theLowerLimit,
                             double& theUpperLimit)
{
  try
  {
    auto pos = theString.find('[');

    theLowerLimit = -std::numeric_limits<double>::max();
    theUpperLimit = std::numeric_limits<double>::max();

    if (pos == std::string::npos)
      return theString;

    auto function_name = theString.substr(0, pos);

    auto limits = extract_limits(theString);

    if (!limits.empty())
    {
      const auto pos = limits.find(':');

      if (pos == std::string::npos)
        throw Fmi::Exception(BCP, "Unrecognized modifier format '" + limits + "'!");

      auto lo = limits.substr(0, pos);
      auto hi = limits.substr(pos + 1);
      Fmi::trim(lo);
      Fmi::trim(hi);

      if (lo.empty() && hi.empty())
        throw Fmi::Exception(BCP, "Both lower and upper limit are missing from the modifier!");

      if (!lo.empty())
        theLowerLimit = Fmi::stod(lo);
      if (!hi.empty())
        theUpperLimit = Fmi::stod(hi);
    }

    return function_name;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Returns parameter name
 *
 * This function handles the aliases and returns same name for all aliases
 *
 *  - original_name
 *
 * \return The parameter name
 */
// ----------------------------------------------------------------------

std::string parse_parameter_name(const std::string& param_name)
{
  try
  {
    static const char* names[] = {"temperature",
                                  "t2m",
                                  "t",
                                  "precipitation",
                                  "precipitation1h",
                                  "rr1h",
                                  "radarprecipitation1h",
                                  "precipitationtype",
                                  "rtype",
                                  "precipitationform",
                                  "rform",
                                  "precipitationprobability",
                                  "pop",
                                  "totalcloudcover",
                                  "cloudiness",
                                  "n",
                                  "humidity",
                                  "windspeed",
                                  "windspeedms",
                                  "wspd",
                                  "ff",
                                  "winddirection",
                                  "dd",
                                  "wdir",
                                  "thunder",
                                  "probabilitythunderstorm",
                                  "pot",
                                  "roadtemperature",
                                  "troad",
                                  "roadcondition",
                                  "wroad",
                                  "waveheight",
                                  "wavedirection",
                                  "relativehumidity",
                                  "rh",
                                  "forestfirewarning",
                                  "forestfireindex",
                                  "mpi",
                                  "evaporation",
                                  "evap",
                                  "dewpoint",
                                  "tdew",
                                  "windgust",
                                  "gustspeed",
                                  "gust",
                                  "fogintensity",
                                  "fog",
                                  "maximumwind",
                                  "hourlymaximumwindspeed",
                                  "wmax",
                                  ""};

    static const char* parameters[] = {
        "Temperature",              // "Temperature"
        "Temperature",              // "t2m"
        "Temperature",              // "t"
        "Precipitation1h",          // "Precipitation"
        "Precipitation1h",          // "Precipitation1h"
        "Precipitation1h",          // "rr1h"
        "RadarPrecipitation1h",     // "radarprecipitation1h"
        "PrecipitationType",        // "PrecipitationType"
        "PrecipitationType",        // "rtype"
        "PrecipitationForm",        // "PrecipitationForm"
        "PrecipitationForm",        // "rform"
        "PoP",                      // "PrecipitationProbability"
        "PoP",                      // "pop"
        "TotalCloudCover",          // "TotalCloudCover"
        "TotalCloudCover",          // "Cloudiness"
        "TotalCloudCover",          // "n"
        "Humidity",                 // "Humidity"
        "WindSpeedMS",              // "WindSpeed"
        "WindSpeedMS",              // "WindSpeedMS"
        "WindSpeedMS",              // "wspd"
        "WindSpeedMS",              // "ff"
        "WindDirection",            // "WindDirection"
        "WindDirection",            // "dd"
        "WindDirection",            // "wdir"
        "ProbabilityThunderstorm",  // "Thunder"
        "ProbabilityThunderstorm",  // "ProbabilityThunderstorm"
        "ProbabilityThunderstorm",  // "pot"
        "RoadTemperature",          // "RoadTemperature"
        "RoadTemperature",          // "troad"
        "RoadCondition",            // "RoadCondition"
        "RoadCondition",            // "wroad"
        "SigWaveHeight",            // "WaveHeight"
        "WaveDirection",            // "WaveDirection"
        "RelativeHumidity",         // "RelativeHumidity"
        "RelativeHumidity",         // "rh"
        "ForestFireWarning",        // "ForestFireWarning"
        "ForestFireWarning",        // "ForestFireIndex"
        "ForestFireWarning",        // "mpi"
        "Evaporation",              // "Evaporation"
        "Evaporation",              // "evap"
        "DewPoint",                 // "DewPoint"
        "DewPoint",                 // "tdew"
        "WindGust",                 // "WindGust"
        "WindGust",                 // "GustSpeed"
        "WindGust",                 // "gust"
        "FogIntensity",             // "FogIntensity"
        "FogIntensity",             // "fog"
        "MaximumWind",              // "MaximumWind"
        "HourlyMaximumWindSpeed",   // "HourlyMaximumWindSpeed"
        "MaximumWind"               // "wmax"
    };

    for (unsigned int i = 0; names[i][0] != 0; i++)
    {
      if (param_name == names[i])
        return parameters[i];
    }

    return param_name;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::list<std::string> parse_parameter_parts(const std::string& paramreq)
{
  try
  {
    std::list<std::string> parts;
    std::string::size_type pos1 = 0;
    while (pos1 < paramreq.size())
    {
      std::string::size_type pos2 = pos1;
      for (; pos2 < paramreq.size(); ++pos2)
        if (paramreq[pos2] == '(' || paramreq[pos2] == ')')
          break;
      if (pos2 - pos1 > 0)
        parts.emplace_back(paramreq.substr(pos1, pos2 - pos1));

      pos1 = pos2 + 1;
    }

    return parts;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void parse_functions(const std::string& functionname1,
                     const std::string& functionname2,
                     std::string& paramname,
                     DataFunction& theInnerDataFunction,
                     DataFunction& theOuterDataFunction)
{
  try
  {
    double lower_limit = std::numeric_limits<double>::lowest();
    double upper_limit = std::numeric_limits<double>::max();
    unsigned int aggregation_interval_behind = std::numeric_limits<unsigned int>::max();
    unsigned int aggregation_interval_ahead = std::numeric_limits<unsigned int>::max();

    // inner and outer functions exist
    auto f_name = extract_function(functionname2, lower_limit, upper_limit);

    theInnerDataFunction.setLimits(lower_limit, upper_limit);

    theInnerDataFunction.setId(parse_function(f_name));
    theInnerDataFunction.setType(boost::algorithm::ends_with(f_name, "_t")
                                     ? FunctionType::TimeFunction
                                     : FunctionType::AreaFunction);

    theInnerDataFunction.setIsNaNFunction(boost::algorithm::starts_with(f_name, "nan"));
    theInnerDataFunction.setIsDirFunction(boost::algorithm::ends_with(f_name, "dir_t"));

    // Nearest && Interpolate functions always accepts NaNs in time series
    if (theInnerDataFunction.id() == FunctionId::Nearest ||
        theInnerDataFunction.id() == FunctionId::Interpolate)
      theInnerDataFunction.setIsNaNFunction(true);
    if (theInnerDataFunction.type() == FunctionType::TimeFunction)
    {
      parse_intervals(paramname, aggregation_interval_behind, aggregation_interval_ahead);
      theInnerDataFunction.setAggregationIntervalBehind(aggregation_interval_behind);
      theInnerDataFunction.setAggregationIntervalAhead(aggregation_interval_ahead);
    }

    f_name = extract_function(functionname1, lower_limit, upper_limit);
    theOuterDataFunction.setLimits(lower_limit, upper_limit);

    theOuterDataFunction.setId(parse_function(f_name));
    theOuterDataFunction.setType(boost::algorithm::ends_with(f_name, "_t")
                                     ? FunctionType::TimeFunction
                                     : FunctionType::AreaFunction);

    theOuterDataFunction.setIsNaNFunction(boost::algorithm::starts_with(f_name, "nan"));
    theOuterDataFunction.setIsDirFunction(boost::algorithm::ends_with(f_name, "dir_t"));

    if (theOuterDataFunction.type() == FunctionType::TimeFunction)
    {
      parse_intervals(paramname, aggregation_interval_behind, aggregation_interval_ahead);
      theOuterDataFunction.setAggregationIntervalBehind(aggregation_interval_behind);
      theOuterDataFunction.setAggregationIntervalAhead(aggregation_interval_ahead);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void parse_function(const std::string& functionname1,
                    std::string& paramname,
                    DataFunction& theInnerDataFunction)
{
  try
  {
    double lower_limit = std::numeric_limits<double>::lowest();
    double upper_limit = std::numeric_limits<double>::max();
    unsigned int aggregation_interval_behind = std::numeric_limits<unsigned int>::max();
    unsigned int aggregation_interval_ahead = std::numeric_limits<unsigned int>::max();

    // only inner function exists,
    auto f_name = extract_function(functionname1, lower_limit, upper_limit);
    theInnerDataFunction.setLimits(lower_limit, upper_limit);

    theInnerDataFunction.setId(parse_function(f_name));
    theInnerDataFunction.setType(boost::algorithm::ends_with(f_name, "_t")
                                     ? FunctionType::TimeFunction
                                     : FunctionType::AreaFunction);

    theInnerDataFunction.setIsNaNFunction(boost::algorithm::starts_with(f_name, "nan"));
    theInnerDataFunction.setIsDirFunction(boost::algorithm::ends_with(f_name, "dir_t"));

    // Nearest && Interpolate functions always accepts NaNs in time series
    if (theInnerDataFunction.id() == FunctionId::Nearest ||
        theInnerDataFunction.id() == FunctionId::Interpolate)
      theInnerDataFunction.setIsNaNFunction(true);

    if (theInnerDataFunction.type() == FunctionType::TimeFunction)
    {
      parse_intervals(paramname, aggregation_interval_behind, aggregation_interval_ahead);
      theInnerDataFunction.setAggregationIntervalBehind(aggregation_interval_behind);
      theInnerDataFunction.setAggregationIntervalAhead(aggregation_interval_ahead);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Get an instance of the parameter factory (singleton)
 */
// ----------------------------------------------------------------------

const ParameterFactory& ParameterFactory::instance()
{
  try
  {
    static ParameterFactory factory;
    return factory;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize the factory
 *
 * This is only called once by instance()
 */
// ----------------------------------------------------------------------

ParameterFactory::ParameterFactory()

{
  try
  {
    // We must make one query to make sure the converter is
    // initialized while the constructor is run thread safely
    // only once. Unfortunately the init method is private.

    converter.ToString(1);  // 1 == Pressure
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return name for the given parameter
 */
// ----------------------------------------------------------------------

std::string ParameterFactory::name(int number) const
{
  try
  {
    return converter.ToString(number);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return number for the given name
 */

int ParameterFactory::number(const std::string& name) const
{
  try
  {
    return converter.ToEnum(name);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the given parameter name and functions
 */
// ----------------------------------------------------------------------

std::string ParameterFactory::parse_parameter_functions(const std::string& theParameterRequest,
                                                        std::string& theOriginalName,
                                                        std::string& theParameterNameAlias,
                                                        DataFunction& theInnerDataFunction,
                                                        DataFunction& theOuterDataFunction) const
{
  try
  {
    std::string paramreq = theParameterRequest;
    std::string date_formatting_string;

    // extract alias name for the parameter
    size_t alias_name_pos(paramreq.find("as "));
    if (alias_name_pos != std::string::npos)
    {
      theParameterNameAlias = paramreq.substr(alias_name_pos + 3);
      paramreq.resize(alias_name_pos);
      Fmi::trim(theParameterNameAlias);
      Fmi::trim(paramreq);
    }

    // special handling for date formatting, for example date(%Y-...)
    if (paramreq.find("date(") != std::string::npos)
    {
      size_t startIndex = paramreq.find("date(") + 4;
      std::string restOfTheParamReq = paramreq.substr(startIndex);

      if (restOfTheParamReq.find(')') == std::string::npos)
        throw Fmi::Exception(BCP, "Errorneous parameter request '" + theParameterRequest + "'!");

      size_t sizeOfTimeFormatString = restOfTheParamReq.find(')') + 1;
      restOfTheParamReq = restOfTheParamReq.substr(sizeOfTimeFormatString);
      date_formatting_string = paramreq.substr(startIndex, sizeOfTimeFormatString);
      paramreq.erase(startIndex, sizeOfTimeFormatString);
    }

    // If there is no timeseries functions involved, don't continue parsing
    if (paramreq.find('(') == std::string::npos)
    {
      theOriginalName = paramreq + date_formatting_string;
      Fmi::ascii_tolower(paramreq);

      return paramreq + date_formatting_string;
    }

    auto parts = parse_parameter_parts(paramreq);

    if (parts.empty() || parts.size() > 3)
      throw Fmi::Exception(BCP, "Errorneous parameter request '" + theParameterRequest + "'!");

    std::string paramname = parts.back();

    //    unsigned int aggregation_interval_behind = std::numeric_limits<unsigned int>::max();
    //    unsigned int aggregation_interval_ahead = std::numeric_limits<unsigned int>::max();

    parts.pop_back();
    const std::string functionname1 = (parts.empty() ? "" : parts.front());
    if (!parts.empty())
      parts.pop_front();
    const std::string functionname2 = (parts.empty() ? "" : parts.front());
    if (!parts.empty())
      parts.pop_front();

    if (!functionname1.empty() && !functionname2.empty())
    {
      parse_functions(
          functionname1, functionname2, paramname, theInnerDataFunction, theOuterDataFunction);
    }
    else
    {
      parse_function(functionname1, paramname, theInnerDataFunction);
    }

    if (theOuterDataFunction.type() == theInnerDataFunction.type() &&
        theOuterDataFunction.type() != FunctionType::NullFunctionType)
    {
      // remove outer function
      theOuterDataFunction = DataFunction();
    }

    // We assume ASCII chars only in parameter names
    theOriginalName = paramname + date_formatting_string;
    Fmi::ascii_tolower(paramname);
    return paramname + date_formatting_string;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

ParameterAndFunctions ParameterFactory::parseNameAndFunctions(
    const std::string& name, bool ignoreBadParameter /* = false*/) const
{
  try
  {
    auto tmpname = Fmi::trim_copy(name);

    DataFunction innerFunction;
    DataFunction outerFunction;

    size_t parenhesis_start = std::count(tmpname.begin(), tmpname.end(), '(');
    size_t parenhesis_end = std::count(tmpname.begin(), tmpname.end(), ')');

    if (parenhesis_start != parenhesis_end)
      throw Fmi::Exception(BCP, "Wrong number of parenthesis: " + tmpname);

    std::string sensor_no;
    std::string sensor_parameter;

    std::string innermost_item = tmpname;
    // If sensor info exists it is inside the innermost parenthesis
    while (innermost_item.find_first_of('(') != innermost_item.find_last_of('('))
    {
      size_t count = innermost_item.find_last_of(')') - innermost_item.find_first_of('(') - 1;
      innermost_item = innermost_item.substr(innermost_item.find_first_of('(') + 1, count);
    }

    if (!boost::algorithm::istarts_with(name, "date(") &&
        innermost_item.find('(') != std::string::npos)
    {
      Fmi::trim(innermost_item);
      std::string innermost_name = innermost_item.substr(0, innermost_item.find('('));
      if (innermost_item.find('[') != std::string::npos)
      {
        // Remove [..., for example percentage_t[0:60](TotalCloudCover)
        innermost_name.substr(innermost_item.find('['));
      }
      // If the name before innermost parenthesis is not a function it must be a parameter

      if (get_function_index(innermost_name) < 0)
      {
        // Sensor info
        bool sensor_parameter_exists = false;
        auto len = innermost_item.find(')') - innermost_item.find('(') + 1;
        auto sensor_info = innermost_item.substr(innermost_item.find('('), len);
        if (sensor_info.find(':') != sensor_info.rfind(':'))
        {
          auto len = sensor_info.rfind(')') - sensor_info.rfind(':') - 1;
          sensor_parameter = sensor_info.substr(sensor_info.rfind(':') + 1, len);
          len = sensor_info.rfind(':') - sensor_info.find(':') - 1;
          sensor_no = sensor_info.substr(sensor_info.find(':') + 1, len);
          sensor_parameter_exists = true;
        }
        else if (sensor_info.find(':') != std::string::npos)
        {
          size_t len = sensor_info.rfind(')') - sensor_info.find(':') - 1;
          sensor_no = sensor_info.substr(sensor_info.find(':') + 1, len);
        }
        Fmi::trim(sensor_parameter);
        Fmi::trim(sensor_no);

        if (sensor_no.empty())
          throw Fmi::Exception(BCP, "Sensor number can not be empty!");
        if (sensor_parameter_exists &&
            (sensor_parameter.empty() ||
             (sensor_parameter != "qc" && sensor_parameter != "longitude" &&
              sensor_parameter != "latitude")))
          throw Fmi::Exception(BCP,
                               "Sensor parameter must be of the following: qc,longitide,latitude!");

        boost::algorithm::replace_first(tmpname, sensor_info, "");
      }
    }

    std::string paramnameAlias = tmpname;
    std::string originalParamName = tmpname;

    std::string paramname = parse_parameter_name(parse_parameter_functions(
        tmpname, originalParamName, paramnameAlias, innerFunction, outerFunction));

    Spine::Parameter parameter = parse(paramname, ignoreBadParameter);

    parameter.setAlias(paramnameAlias);
    parameter.setOriginalName(originalParamName);

    if (boost::algorithm::starts_with(paramname, "qc_"))
      sensor_parameter = "qc";

    if (!sensor_no.empty())
      parameter.setSensorNumber(Fmi::stoi(sensor_no));
    if (!sensor_parameter.empty())
      parameter.setSensorParameter(sensor_parameter);

    return {parameter, DataFunctions(innerFunction, outerFunction)};
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}  // namespace Spine

// ----------------------------------------------------------------------
/*!
 * \brief Parse the given parameter name
 */
// ----------------------------------------------------------------------

Spine::Parameter ParameterFactory::parse(const std::string& paramname,
                                         bool ignoreBadParameter /* = false*/) const
{
  using Spine::Parameter;
  using namespace Spine::Parameters;
  try
  {
    if (paramname.empty())
      throw Fmi::Exception(BCP, "Empty parameters are not allowed!");

    auto pname = paramname;
    if (pname == "cloudceiling" || pname == "cloudceilingft")
      pname = "cloudceilinghft";
    // Metaparameters are required to have a FmiParameterName too
    auto number = FmiParameterName(converter.ToEnum(pname));

    if (number == kFmiBadParameter && Fmi::looks_signed_int(pname))
      number = FmiParameterName(Fmi::stol(pname));

    Parameter::Type type = Parameter::Type::Data;

    if (Spine::Parameters::IsLandscaped(number))
      type = Parameter::Type::Landscaped;
    else if (Spine::Parameters::IsDataIndependent(number))
      type = Parameter::Type::DataIndependent;
    else if (Spine::Parameters::IsDataDerived(number))
      type = Parameter::Type::DataDerived;
    else if (pname.substr(0, 5) == "date(" && paramname[pname.size() - 1] == ')')
      type = Parameter::Type::DataIndependent;
    else if (boost::algorithm::iends_with(pname, ".raw"))
      number = FmiParameterName(converter.ToEnum(pname.substr(0, pname.size() - 4)));

    if (number == kFmiBadParameter && !ignoreBadParameter)
      throw Fmi::Exception(BCP, "Unknown parameter '" + pname + "'");

    return {paramname, type, number};
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace TimeSeries
}  // namespace SmartMet
