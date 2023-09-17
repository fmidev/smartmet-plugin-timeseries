#include "DataFilter.h"
#include <boost/algorithm/string.hpp>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <list>
#include <map>
#include <set>
#include <vector>

namespace SmartMet
{
namespace TimeSeries
{
namespace
{
enum class ComparisonType
{
  LT,
  LE,
  EQ,
  GE,
  GT
};

enum class JoinType
{
  AND,
  OR
};

std::vector<const char*> comparison_str{"lt", "le", "eq", "ge", "gt"};
std::vector<const char*> comparison_sql{" < ", " <= ", " = ", " >= ", " > "};

struct Comparison
{
  int value;
  ComparisonType cmp;
  JoinType join;
};

// We keep AND operations at the front
using Comparisons = std::list<Comparison>;

ComparisonType parse_comparison(const std::string& str)
{
  if (str == "lt")
    return ComparisonType::LT;
  if (str == "le")
    return ComparisonType::LE;
  if (str == "eq")
    return ComparisonType::EQ;
  if (str == "ge")
    return ComparisonType::GE;
  if (str == "gt")
    return ComparisonType::GT;
  throw Fmi::Exception(BCP, "Invalid data comparison operator '" + str + "'");
}

JoinType parse_join(const std::string& str)
{
  if (str == "OR")
    return JoinType::OR;
  if (str == "AND")
    return JoinType::AND;
  throw Fmi::Exception(BCP, "Invalid logical expression '" + str + "'");
}
}  // namespace

class DataFilter::Impl
{
 public:
  void addDataFilter(const std::string& name, const std::string& filter_str)
  {
    try
    {
      std::vector<std::string> parts;
      boost::algorithm::split(parts, filter_str, boost::algorithm::is_any_of(" "));

      if (parts.size() == 1)
        filtermap[name].push_back(
            Comparison{Fmi::stoi(parts[0]), ComparisonType::EQ, JoinType::OR});
      else if (parts.size() == 2)
        filtermap[name].push_back(
            Comparison{Fmi::stoi(parts[1]), parse_comparison(parts[0]), JoinType::OR});
      else if (parts.size() == 5)
      {
        // "lt X OR gt X" etc
        auto cmp1 = parse_comparison(parts[0]);
        auto val1 = Fmi::stoi(parts[1]);
        auto join = parse_join(parts[2]);
        auto cmp2 = parse_comparison(parts[3]);
        auto val2 = Fmi::stoi(parts[4]);
        // keep AND conditions at the front so that the loop in valueOK works correctly
        if (join == JoinType::AND)
        {
          filtermap[name].push_front(Comparison{val1, cmp1, join});
          filtermap[name].push_front(Comparison{val2, cmp2, join});
        }
        else
        {
          filtermap[name].push_back(Comparison{val1, cmp1, join});
          filtermap[name].push_back(Comparison{val2, cmp2, join});
        }
      }
      else
        throw Fmi::Exception(BCP, "Incorrect number of elements in data comparison expression")
            .addParameter("size", Fmi::to_string(parts.size()));
    }
    catch (...)
    {
      throw Fmi::Exception::Trace(BCP, "Invalid data comparison expression '" + filter_str + "'");
    }
  }

  bool exist(const std::string& name) const { return filtermap.find(name) != filtermap.end(); }

  bool empty() const { return filtermap.empty(); }

  bool valueOK(const std::string& name, int val) const
  {
    const auto pos = filtermap.find(name);

    // Value is ok if there is no filter for the parameter
    if (pos == filtermap.end())
      return true;

    // Initial value for joins is true, if the first comparison is AND. Otherwise start with FALSE
    // for OR's
    bool result = (pos->second.front().join == JoinType::AND);

    for (const auto& comp : pos->second)
    {
      bool flag = true;

      if (comp.cmp == ComparisonType::LT)
        flag = (val < comp.value);
      else if (comp.cmp == ComparisonType::LE)
        flag = (val <= comp.value);
      else if (comp.cmp == ComparisonType::EQ)
        flag = (val == comp.value);
      else if (comp.cmp == ComparisonType::GE)
        flag = (val >= comp.value);
      else
        flag = (val > comp.value);

      if (comp.join == JoinType::AND)
        result &= flag;
      else
        result |= flag;
    }

    return result;
  }

  void print() const
  {
    for (const auto& name_filters : filtermap)
    {
      printf("NAME = %s\n", name_filters.first.c_str());
      for (const auto& filter : name_filters.second)
      {
        printf("\t%s %d (%d)\n",
               comparison_str[static_cast<int>(filter.cmp)],
               filter.value,
               int(filter.join));
      }
    }
  }

  std::string getSqlClause(const std::string& name, const std::string& dbfield) const
  {
    std::string ret;

    const auto pos = filtermap.find(name);
    if (pos == filtermap.end())
      return ret;

    ret += '(';
    for (const auto& filter : pos->second)
    {
      if (ret != "(")
      {
        if (filter.join == JoinType::AND)
          ret += " AND ";
        else
          ret += " OR ";
      }
      ret += dbfield;
      ret += comparison_sql[static_cast<int>(filter.cmp)];
      ret += Fmi::to_string(filter.value);
    }
    ret += ')';
    return ret;
  }

 private:
  using FilterMap = std::map<std::string, Comparisons>;
  FilterMap filtermap;
};

DataFilter::~DataFilter() = default;

DataFilter::DataFilter() : impl(new Impl()) {}

void DataFilter::setDataFilter(const std::string& name, const std::string& value)
{
  std::vector<std::string> parts;
  boost::algorithm::split(parts, value, boost::algorithm::is_any_of(","));

  for (const auto& filter : parts)
    impl->addDataFilter(name, filter);
}

bool DataFilter::exist(const std::string& name) const
{
  return impl->exist(name);
}

bool DataFilter::empty() const
{
  return impl->empty();
}

bool DataFilter::valueOK(const std::string& name, int val) const
{
  return impl->valueOK(name, val);
}

std::string DataFilter::getSqlClause(const std::string& name, const std::string& dbfield) const
{
  return impl->getSqlClause(name, dbfield);
}

void DataFilter::print() const
{
  impl->print();
}

}  // namespace TimeSeries
}  // namespace SmartMet
