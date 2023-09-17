#pragma once

#include "TimeSeries.h"
#include <macgyver/ValueFormatter.h>
#include <spine/Table.h>
#include <spine/TableVisitor.h>
#include <list>
#include <string>
#include <vector>

namespace SmartMet
{
namespace TimeSeries
{
// feed data to Table
// usage1: tablefeeder << TimeSeries
// usage2: tablefeeder << TimeSeriesGroup

class TableFeeder
{
 private:
  const Fmi::ValueFormatter& itsValueFormatter;
  const std::vector<int>& itsPrecisions;
  Spine::TableVisitor itsTableVisitor;
  Spine::LonLatFormat itsLonLatFormat;

 public:
  TableFeeder(Spine::Table& table,
              const Fmi::ValueFormatter& valueformatter,
              const std::vector<int>& precisions,
              unsigned int currentcolumn = 0)
      : itsValueFormatter(valueformatter),
        itsPrecisions(precisions),
        itsTableVisitor(table, valueformatter, precisions, currentcolumn, currentcolumn),
        itsLonLatFormat(Spine::LonLatFormat::LONLAT)

  {
  }

  TableFeeder(Spine::Table& table,
              const Fmi::ValueFormatter& valueformatter,
              const std::vector<int>& precisions,
              boost::shared_ptr<Fmi::TimeFormatter> timeformatter,
              boost::optional<boost::local_time::time_zone_ptr> timezoneptr,
              unsigned int currentcolumn = 0)
      : itsValueFormatter(valueformatter),
        itsPrecisions(precisions),
        itsTableVisitor(table,
                        valueformatter,
                        precisions,
                        timeformatter,
                        timezoneptr,
                        currentcolumn,
                        currentcolumn)
  {
  }

  unsigned int getCurrentRow() const { return itsTableVisitor.getCurrentRow(); }
  unsigned int getCurrentColumn() const { return itsTableVisitor.getCurrentColumn(); }
  void setCurrentRow(unsigned int currentRow) { itsTableVisitor.setCurrentRow(currentRow); }
  void setCurrentColumn(unsigned int currentColumn)
  {
    itsTableVisitor.setCurrentColumn(currentColumn);
  }

  const TableFeeder& operator<<(const TimeSeries& ts);
  const TableFeeder& operator<<(const TimeSeriesGroup& ts_group);
  const TableFeeder& operator<<(const TimeSeriesVector& ts_vector);
  const TableFeeder& operator<<(const std::vector<Value>& value_vector);

  // Set LonLat formatting
  TableFeeder& operator<<(Spine::LonLatFormat newformat);
};

}  // namespace TimeSeries
}  // namespace SmartMet
