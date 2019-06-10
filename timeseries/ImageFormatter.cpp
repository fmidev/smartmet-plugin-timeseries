// ======================================================================
/*!
 * \brief Implementation of class ImageFormatter
 */
// ======================================================================

#include "ImageFormatter.h"
#include <spine/Convenience.h>
#include <spine/Exception.h>
#include <spine/Table.h>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <iostream>
#include <set>
#include <stdexcept>

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Convert a comma separated string into a set of strings
 */
// ----------------------------------------------------------------------

std::set<std::string> parse_debug_attributes(const std::string& theStr)
{
  try
  {
    std::set<std::string> ret;
    boost::algorithm::split(ret, theStr, boost::algorithm::is_any_of(","));
    return ret;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Format a 2D table using tags style
 */
// ----------------------------------------------------------------------

void ImageFormatter::format(std::ostream& theOutput,
                            const Table& theTable,
                            const TableFormatter::Names& theNames,
                            const HTTP::Request& theReq,
                            const TableFormatterOptions& /* theConfig */) const
{
  try
  {
    std::string miss;
    auto missing = theReq.getParameter("missingtext");

    if (!missing)
      miss = "nan";
    else
      miss = *missing;

    const Table::Indexes cols = theTable.columns();
    const Table::Indexes rows = theTable.rows();

    theOutput << "<!DOCTYPE html><html><head><title>Debug mode output</title>"
              << "<style>"
              << "table {border-collapse: collapse;}"
              << "th, td {border-bottom: 1px solid black; padding: 3px 0.5em 3px 0.5em; "
              << "text-align: center;}"
              << "tr:nth-child(even) {background-color: #f2f2f2;}"
              << "tr:hover {background-color: #e2e2e2;}"
              << "</style>" << std::endl
              << "</head><body>" << std::endl;

    // Output headers

    theOutput << "<table><tr>";
    for (const auto& nam : cols)
    {
      const std::string& name = theNames[nam];
      theOutput << "<th>" << htmlescape(name) << "</th>";
    }
    theOutput << "</tr>";

    for (const auto j : rows)
    {
      theOutput << "<tr>" << std::endl;
      for (const auto i : cols)
      {
        //std::string value = htmlescape(theTable.get(i, j));
        std::string value = theTable.get(i, j);
        theOutput << "<td>" << (value.empty() ? miss : value) << "</td>";
      }
      theOutput << "</tr>";
    }
    theOutput << "</table></body></html>";
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
