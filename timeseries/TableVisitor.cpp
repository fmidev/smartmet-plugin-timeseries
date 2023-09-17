#include "TimeSeries.h"
#include <macgyver/Exception.h>
#include <newbase/NFmiGlobals.h>
#include <spine/TableVisitor.h>

namespace SmartMet
{
using Spine::LonLat;

namespace TimeSeries
{
Spine::TableVisitor& operator<<(Spine::TableVisitor& tf, const Value& val)
{
  try
  {
    if (boost::get<int>(&val) != nullptr)
      tf << *(boost::get<int>(&val));
    else if (boost::get<double>(&val) != nullptr)
      tf << *(boost::get<double>(&val));
    else if (boost::get<std::string>(&val) != nullptr)
      tf << *(boost::get<std::string>(&val));
    else if (boost::get<LonLat>(&val) != nullptr)
    {
      LonLat coord = *(boost::get<LonLat>(&val));
      tf << coord;
    }
    else if (boost::get<boost::local_time::local_date_time>(&val) != nullptr)
    {
      boost::local_time::local_date_time ldt =
          *(boost::get<boost::local_time::local_date_time>(&val));
      tf << ldt;
    }

    return tf;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace TimeSeries
}  // namespace SmartMet
