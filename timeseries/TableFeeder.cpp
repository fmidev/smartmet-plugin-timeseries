#include "TableFeeder.h"
#include "TimeSeriesOutput.h"
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace TimeSeries
{
const TableFeeder& TableFeeder::operator<<(const TimeSeries& ts)
{
  try
  {
    if (ts.empty())
      return *this;

    for (const auto& t : ts)
    {
      Value val = t.value;
      boost::apply_visitor(itsTableVisitor, val);
    }

    return *this;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

const TableFeeder& TableFeeder::operator<<(const TimeSeriesGroup& ts_group)
{
  try
  {
    // no time series
    if (ts_group.empty())
      return *this;

    // one time series
    if (ts_group.size() == 1)
      return (*this << ts_group[0].timeseries);

    // several time series
    size_t n_locations = ts_group.size();
    size_t n_timestamps = ts_group[0].timeseries.size();
    // iterate through timestamps
    for (size_t i = 0; i < n_timestamps; i++)
    {
      std::stringstream ss;
      OStreamVisitor ostream_visitor(
          ss, itsValueFormatter, itsPrecisions[itsTableVisitor.getCurrentColumn()]);
      ostream_visitor << itsLonLatFormat;

      // get values of the same timestep from all locations and concatenate them into one string
      ss << "[";
      // iterate through locations
      for (size_t k = 0; k < n_locations; k++)
      {
        // take time series from k:th location
        const TimeSeries& timeseries = ts_group[k].timeseries;

        if (k > 0)
          ss << " ";

        // take value from i:th timestep
        Value val = timeseries[i].value;
        // append value to the ostream
        boost::apply_visitor(ostream_visitor, val);
      }
      // if no data added (timestep not included)
      if (ss.str().size() == 1)
        continue;

      ss << "]";
      std::string str_value(ss.str());
      // remove spaces
      while (str_value[1] == ' ')
        str_value.erase(1, 1);
      while (str_value[str_value.size() - 2] == ' ')
        str_value.erase(str_value.size() - 2, 1);

      Value dv = str_value;
      boost::apply_visitor(itsTableVisitor, dv);
    }

    return *this;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

const TableFeeder& TableFeeder::operator<<(const TimeSeriesVector& ts_vector)
{
  try
  {
    // no time series
    if (ts_vector.empty())
      return *this;

    unsigned int startRow(itsTableVisitor.getCurrentRow());

    for (const auto& tvalue : ts_vector)
    {
      itsTableVisitor.setCurrentRow(startRow);

      TimeSeries ts(tvalue);
      for (auto& t : ts)
      {
        boost::apply_visitor(itsTableVisitor, t.value);
      }
      itsTableVisitor.setCurrentColumn(itsTableVisitor.getCurrentColumn() + 1);
    }

    return *this;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

const TableFeeder& TableFeeder::operator<<(const std::vector<Value>& value_vector)
{
  try
  {
    // no values
    if (value_vector.empty())
      return *this;

    for (const auto& val : value_vector)
      boost::apply_visitor(itsTableVisitor, val);

    return *this;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

TableFeeder& TableFeeder::operator<<(Spine::LonLatFormat newformat)
{
  try
  {
    itsLonLatFormat = newformat;
    this->itsTableVisitor << newformat;
    return *this;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace TimeSeries
}  // namespace SmartMet
