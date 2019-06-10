#include "grid-files/common/Exception.h"
#include "grid-files/common/GeneralFunctions.h"
#include "grid-files/common/ConfigurationFile.h"


using namespace SmartMet;




int main(int argc, char *argv[])
{
  try
  {
    if (argc < 4)
    {
      fprintf(stderr,"USAGE: TestConfigCreator <configurationFile> <inputFile> <outpufFile> [-D attrName attValue ... -D attrName attrValue]\n");
      return -1;
    }

    char *configFile = argv[1];
    char *inputFile = argv[2];
    char *outputFile = argv[3];

    ConfigurationFile config(configFile);

    for (int t=4; t<argc; t++)
    {
      if (strcmp(argv[t],"-D") == 0  &&  (t+2) < argc)
      {
        std::string value = argv[t+2];
        config.setAttributeValue(argv[t+1],value);
      }
    }

    config.replaceAttributeNamesWithValues(std::string(inputFile),std::string(outputFile));
    return 0;
  }
  catch (SmartMet::Spine::Exception& e)
  {
    SmartMet::Spine::Exception exception(BCP,"Service call failed!",NULL);
    exception.printError();
    return -7;
  }
}

