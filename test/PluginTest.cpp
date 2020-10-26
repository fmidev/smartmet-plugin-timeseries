#include "Plugin.h"
#include <spine/PluginTest.h>

using namespace std;

void prelude(SmartMet::Spine::Reactor& reactor)
{
  // Wait for qengine to finish
  sleep(5);

#if 1
  auto handlers = reactor.getURIMap();
  while (handlers.find("/timeseries") == handlers.end())
  {
    sleep(1);
    handlers = reactor.getURIMap();
  }
  sleep(5);
#endif

  cout << endl << "Testing timeseries plugin" << endl << "=========================" << endl;
}

int main(int argc, char* argv[])
{
  SmartMet::Spine::Options options;
  options.quiet = true;
  options.defaultlogging = false;
  options.configfile = "cnf/reactor.conf";

  if(!options.parse(argc, argv))
    exit(1);

  return SmartMet::Spine::PluginTest::test(options, prelude, false, 10);
}
