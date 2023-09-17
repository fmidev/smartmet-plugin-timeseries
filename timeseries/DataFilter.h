#pragma once

#include <memory>
#include <string>

namespace SmartMet
{
namespace TimeSeries
{
class DataFilter
{
 public:
  ~DataFilter();
  DataFilter();

  // For example name = "data_quality", value = "le 5"
  void setDataFilter(const std::string& name, const std::string& value);

  bool exist(const std::string& name) const;
  bool empty() const;
  bool valueOK(const std::string& name, int val) const;
  std::string getSqlClause(const std::string& name, const std::string& dbfield) const;

  void print() const;

 private:
  class Impl;
  std::shared_ptr<Impl> impl;  // shared for copying the Settings object
};

}  // namespace TimeSeries
}  // namespace SmartMet
