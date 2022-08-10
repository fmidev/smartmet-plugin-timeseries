# Tutorial

This tutorial explains how to configure the TimeSeries plugin when using Docker.

## Prereqs

Docker software has been installed on some Linux server where you have access to and the smartmetserver docker container is up and running. The TimeSeries plugin and the configuration file it uses have been defined in the main configuration file smartmet.conf already.

### File timeseries.conf

The purpose of the file "timeseries.conf" is to define configuration attributes so that the TimeSeries plugin can fetch the meteorological time series information over the Web using the HTTP protocol for a specific time or time interval chosen by the user. HTTP request contains the parameters needed to obtain the information in response to a query. It contains also the parameters needed for processing the results and formatting the output.

If you followed the “SmartMet Server Tutorial (Docker)” you have your configuration folders and files in the host machine at $HOME/docker-smartmetserver/smartmetconf but inside Docker they show up under /etc/smartmet. 

1. Go to the correct directory and enter command below to review the file:
```
$ less timeseries.conf 
```
You will see something like this:

![](https://github.com/fmidev/smartmet-plugin-wms/wiki/images/TimeSeriesConf.PNG)

2. Use Nano or some other editor to enable/disable configuration attributes or to change their values if needed.

## Configuration attributes

### language 
This attribute defines the default language used in the response of a request (e.g. "fi", "sv", "en").
```
language             = "en";
```

### locale
This attribute defines the default locale value (e.g. "fi_FI", "en_US.utf8") of the names and abbreviations used for week days (Sunday, monday / Sun, Mon, etc.) and months (January, February / Jan, Feb, etc.).
```
locale               = "en_US.utf8";
```

### observation_disabled
This attribute can be used to enable/disable the usage of the Observation-engine. It can have values "true" or "false". 
```
observation_disabled = true;
```

### postgis.disabled
This attribute can be used to enable/disable PostGIS shapes. It can have values "true" or "false". Alternatively one can leave out the entire postgis section.
```
postgis.disabled     = true;
```

### precision
This attribute is used to define a list of precision names that can be used with the "precision" parameter when requesting the data. Typically the following precision names are defined: 

* normal
* double
* full 

In addition to this, the meaning of these attributes is also defined on the parameter level later in this configuration file. In other words, the "normal" precision for the parameter X might be 2 decimals while the "normal" precision for the parameter Z might be 5 decimals. All the precision attributes are defined in the configuration file.

```
precision:
{
        // default is the first one listed

        enabled = ["normal","double","full"];
```

```
    // normal output mode for meteograms & tables

    normal:
    {
                default                 = 0;
                Precipitation1h        = 1;
        SigWaveHeight        = 1;
        CorrectedReflectivity    = 1;
        TotalPrecipitationF0    = 1;
        TotalPrecipitationF10    = 1;
        TotalPrecipitationF25    = 1;
        TotalPrecipitationF50    = 1;
        TotalPrecipitationF75    = 1;
etc...
```

```
    // double precision for graphs

    double:
    {
        default            = 1;

        PrecipitationForm    = 0;
        PrecipitationType    = 0;
        WeatherSymbol1        = 0;
        WeatherSymbol3        = 0;

        CorrectedReflectivity    = 2;
        TotalPrecipitationF0    = 2;
        TotalPrecipitationF10    = 2;
etc...
```

```
    // full precision for math etc

    full:
    {
        default            = 16;

        PrecipitationForm    = 0;
        PrecipitationType    = 0;
        WeatherSymbol1        = 0;
        WeatherSymbol3        = 0;
    };

```
**Note:** You can review [Attributes table](https://github.com/fmidev/smartmet-plugin-timeseries/wiki/SmartMet-plugin-TimeSeries#plugin-configuration-file) for more details about the attributes.

3. Test the plugin. The URL of the HTTP request contains parameters that have to be delivered to the TimeSeries plugin. For example, the following request fetches the temperature forecasted for the city of Helsinki:
```
http://hostname:8080/timeseries?format=debug&place=Helsinki&param=name,time,temperature
```
**Note:** Replace hostname with your host machine name, by localhost or by host-ip. This depends on where you have the container you are using.