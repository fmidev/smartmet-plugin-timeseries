# Examples of Timeseries Requests <!-- omit in toc -->

This page has example queries. For more indepth instructions on how to make them
see [Using the Timeseries API -page](Using-the-Timeseries-API.md).

Feel free to contribute your own examples on this page!

- [How to Use the Examples](#how-to-use-the-examples)
- [Observations](#observations)
  - [Basic query for surface weather observations in a point](#basic-query-for-surface-weather-observations-in-a-point)
  - [Temperatures on All Weather Stations in Finland](#temperatures-on-all-weather-stations-in-finland)
  - [Solar observations](#solar-observations)
  - [Sensor number](#sensor-number)
  - [Sensor number and quality flag](#sensor-number-and-quality-flag)
  - [Station information](#station-information)
  - [Foreign observations](#foreign-observations)
  - [Time aggregation](#time-aggregation)
  - [Area aggregation](#area-aggregation)
  - [Lightning data](#lightning-data)
  - [Heating Requirement Number](#heating-requirement-number)
- [Forecast](#forecast)
  - [ECMWF Calibrated Fractiles](#ecmwf-calibrated-fractiles)
  - [MetCoOp NowCasting](#metcoop-nowcasting)

## How to Use the Examples

Please note that some data in the examples are only only available for
commercial use and are therefore not found in the FMI Open Data.

For FMI Open Data replace the host part in the example URLs with 
`opendata.fmi.fi`.

For commercial use replace the host part with `data.fmi.fi`
and add your personal apikey with `/apikey/WriteApiKeyHere/timeseries` before the 
querystring.

Most examples here format the output as styled HTML for testing purposes. Use
Json or other machine readable formats for any real use.

## Observations

### Basic query for surface weather observations in a point

```text
http://smartmet.fmi.fi/timeseries?precision=double&timestep=data&param=utctime,fmisid,stationname,stationlat,stationlon,distance,TA_PT1H_AVG&producer=observations_fmi&latlon=60.1757,24.9336&starttime=2018-05-14T00:00:00&endtime=2018-05-15T00:00:00&tz=UTC&format=debug
```

The `param` field defines what values to return in results.

- `utctime`: Time of observation in UTC
- `fmisid`: FMI's ID-number of weather station
- `stationname`: Name of the weather station
- `stationlat`: Latitude of the weather stations location
- `stationlon`: Longitude of the weather stations location
- `distance`: Distance from requested location to the weather station in meters
- `TA_PT1H_AVG`: The actual weather observation. Temperature of the past hour.

Other querystring fields in the example are used to filte

- `precision=double`: Allow decimals in number values of observations
- `timestep=data`: Request that all available observations within the time rage are returned for requested parameters
- `producer=observations_fmi`: Use FMI's observation stations
- `latlon=60.1757,24.9336`: Point of interest where the nearest observations should be returned
- `format=debug`: Return human readable HTML-table (use `format=json` for Json-formatted data)

The weather observations, if any is availble, are within the requested time
range, including the start and end times.

- `starttime=2018-05-14T00:00:00`: Get observations made later or at this time
- `endtime=2018-05-15T00:00:00`: Get observations made before or at this time
- `tz=UTC`: Given times are in UTC timezone. This can be replaced with a `Z` at the end of timestrings.

Returns and array of Json objects.

```text
[
  {
    "utctime":"20180514T000000",
    "fmisid":100971,
    "stationname":"Helsinki Kaisaniemi",
    "stationlat":60.1752300,
    "stationlon":24.9445900,
    "distance":0.6,
    "TA_PT1H_AVG":13.0
  },
...
]
```

### Temperatures on All Weather Stations in Finland

This uses the `groupareas=0` field to return data as multiple items.
Parameter is `TA_PT1H_AVG` but might as well be `Temperature`.

```text
http://smartmet.fmi.fi/timeseries?producer=observations_fmi&area=Finland&param=utctime,fmisid,stationname,TA_PT1H_AVG&format=debug&starttime=2018-07-07T00:00:00Z&endtime=2018-07-07T00:00:00Z&groupareas=0
```

### Solar observations

```text
http://smartmet.fmi.fi/timeseries?producer=observations_fmi&place=kumpula&param=utctime,fmisid,stationname,GLOB_PT1H_SUM&format=debug&tz=utc&starttime=2018-07-07T00:00:00
```

### Sensor number

```text
http://smartmet.fmi.fi/timeseries?param=time,name,t2m(:1),t2m(:11)&fmisid=101004&producer=observations_fmi&format=ascii&timeformat=sql&timestep=data&precision=double&starttime=202002200800&endtime=202002200810
```

### Sensor number and quality flag

```text
http://smartmet.fmi.fi/timeseries?param=time,name,t2m(:1),t2m(:11),t2m(:1:qc),t2m(:11:qc)&fmisid=101004&producer=observations_fmi&format=ascii&timeformat=sql&timestep=data&precision=double&starttime=202002200800&endtime=202002200810
```

### Station information

Use `lang=fi|en|sv` to set preferred language in results.

```text
http://smartmet.fmi.fi/timeseries?producer=observations_fmi&latlon=60.20,24.96&param=utctime,TA_PT1H_AVG,fmisid,wmo,region,country,stationname,elevation,stationlatitude,stationlongitude,distance&format=debug&timesteps=1
```

### Foreign observations

Use `lang=fi|en|sv` to set preferred language in results.

```text
http://smartmet.fmi.fi/timeseries?producer=foreign&place=Tokyo&param=utctime,temperature,elevation,country,region,latitude,longitude&format=debug
```

### Time aggregation

```text
http://smartmet.fmi.fi/timeseries?producer=observations_fmi&latlon=60.20,24.96&param=fmisid,stationname,utctime,TA_PT1H_AVG,mean_t(TA_PT1H_AVG:3h)&format=debug&starttime=2018-08-08T00:00:00&timesteps=24&tz=utc
```

### Area aggregation

```text
http://smartmet.fmi.fi/timeseries?format=debug&area=Helsinki&param=fmisid,name,time,min(TA_PT1H_AVG)%20as%20mintemp,max(TA_PT1H_AVG)%20as%20maxtemp%20,mean(TA_PT1H_AVG)%20as%20meantemp&producer=observations_fmi&numberofstations=50
```

### Lightning data

```text
http://smartmet.fmi.fi/timeseries?producer=flash&starttime=2018-08-01T00:00:00&endtime=2018-08-01T01:00:00&param=utctime,flash_id,peak_current,latitude,longitude&bbox=6,51.3,49,70.2&format=json
```

### Heating Requirement Number

_These calculated values are not available in the opendata._

The parameter `HDD_P1D_AVG` aka. "lämmitystarveluku" or "astepäiväluku" in 
Finnish.

Example to get a single values for all availble weather startions for given time

```text
http://smartmet.fmi.fi/timeseries?producer=observations_fmi&param=stationname,utctime,HDD_P1D_AVG&precision=double&keyword=synop_fi&starttime=2021-05-01T00:00:00Z&timestep=data&timesteps=1&format=json
```

For any particular location use use for example `fmisid` instead of keyword.

```text
http://smartmet.fmi.fi/timeseries?producer=observations_fmi&param=stationname,utctime,HDD_P1D_AVG&precision=double&fmisid=151049&starttime=2021-05-01T00:00:00Z&timestep=data&timesteps=1&format=json
```

## Forecast

### ECMWF Calibrated Fractiles

Availabe only available for
commercial use and are therefore not found in the FMI Open Data.

Example returns latest forecast for Helsinki

```text
/timeseries?producer=ecmwf_eurooppa_calibrated_fractiles&format=json&param=TemperatureF10,TemperatureF90,TemperatureDeviation,WindSpeedF90,WindSpeedF10,TotalPrecipitationF90,TotalPrecipitationF10&place=Helsinki
```

### MetCoOp NowCasting

Example returns latest forecast for Helsinki:

```text
/timeseries?producer=metcoop_scandinavia_nowcast_surface&format=json&param=utctime,Pressure,Temperature,WindDirection,WindSpeedMS&place=Helsinki
```
