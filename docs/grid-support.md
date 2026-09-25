# Grid support in the timeseries plugin (developer notes)

These notes describe how the timeseries plugin answers requests from gridded data through
the grid engine. They are for developers; the user-level API is in
[Using-the-Timeseries-API.md](Using-the-Timeseries-API.md). For the grid services
themselves, see the developer guides of the
[grid engine](https://github.com/fmidev/smartmet-engine-grid/blob/master/docs/developer-guide.md)
and [grid-content](https://github.com/fmidev/smartmet-library-grid-content/blob/master/docs/developer-guide.md).

## Files

| File | Role |
|------|------|
| `timeseries/GridEngineQuery.{h,cpp}` | Decides whether a request goes to the grid engine, and runs it per location. |
| `timeseries/GridInterface.{h,cpp}` | Builds `QueryServer::Query` objects from the timeseries `Query`, executes them, and converts the results into time series. |
| `timeseries/QueryProcessingHub.cpp` | Chooses between the observation, grid and querydata paths for each producer group. |
| `timeseries/Plugin.cpp` | Gets the engine in `init()`, passes the geo engine's DEM and land cover to it, and creates the query streamer for `format=file`. |

## When a request uses the grid engine

`QueryProcessingHub` handles producer groups in turn. For each group: observation
producers go to the observation engine; otherwise, if
`GridEngineQuery::isGridEngineQuery()` is true, the grid path runs; otherwise querydata.
`isGridEngineQuery()` requires that `gridengine_disabled` is false in the plugin
configuration, that the engine is enabled, and one of:

* `source=grid` in the request;
* no `source`, and a requested producer is a grid producer (`Engine::isGridProducer()`,
  including producer aliases);
* no `source`, and a parameter names a grid producer (`T:ECG` style);
* no `source`, no producers, and `primaryForecastSource = "grid"` in the configuration.

`processGridEngineQuery()` can still decline: with no producers and no grid producer in
the parameters, the location must be inside one of `defaultGridGeometries`
(`isValidDefaultRequest()`), otherwise querydata is tried instead. With no producers,
`defaultProducerMappingName` is used as the producer.

## Building the grid query

For each location, `GridInterface::processGridQuery()`:

1. **Level type and levels.** `findLevelId()` gets the level type from the request
   (`leveltype`) or the producer. `findLevels()` runs three modes: explicit `level`
   values (or, with none given, all pressure or model levels of the producer, from
   `Engine::getProducerParameterLevelList()`), then `pressure` values, then `height`
   values. Each level becomes its own grid query.
2. **`prepareGridQuery()`** fills a `QueryServer::Query`:
   * times (`prepareQueryTimes()`): time steps or a time range from the timeseries time
     options; `starttime=data` / `endtime=data` become the `StartTimeFromData` /
     `EndTimeFromData` flags, and the data times come from the content (`getDataTimes()`);
   * producer and generation (`prepareProducer()`, `prepareGeneration()`): `origintime`
     selects a generation, and the analysis-time flags;
   * location (`prepareLocation()`): a point, a circle (`radius`), a polygon or path,
     with the coordinate type;
   * parameters (`prepareQueryParameters()`): each parameter and parameter function is
     resolved through `Engine::getProducerName()` → `getProducerAlias(name, levelId)` →
     `getParameterDetails()`, and becomes a `QueryParameter` with its producer, geometry,
     level and forecast type. Parameters that timeseries computes itself
     (`isBuildInParameter()`: `origintime`, `modtime`, `level`, `lat`, `lon`, `latlon`,
     …) are not sent to the grid engine; aggregation parameters get extra time steps
     around each time.
3. **`Engine::executeQuery(query_sptr)`** runs the query. This overload **throws** on a
   Query Server error.
4. **`exteractQueryResult()`** turns the `ParameterValues` of each parameter into
   `TS::TimeSeries` in the requested time zone, fills in the built-in parameters, and
   applies the timeseries value formatting.

## format=file

With `format=file`, the plugin creates a `QueryServer::QueryStreamer`. The grid query
runs with `GeometryHitNotRequired`, `insertFileQueries()` adds a file query for every
grid file that contributed, and the response streams those files (GRIB) instead of a
table.

## Caching

When the grid path processes a producer group, the product hash is set to
`Fmi::bad_hash`. Grid responses therefore get **no ETag** and are not cached by the
frontend. Querydata and observation responses are hashed normally. If you add hashing
for grid responses, base it on `Engine::getProducerHash()` for every producer involved
(it changes when the producer's content changes, with up to 120 s delay) plus the whole
request.

## Pitfalls

* **Routing surprises.** A producer name that exists both as querydata and as a grid
  producer goes to the grid engine only with `source=grid`, or when the grid engine
  considers it a grid producer and no `source` is given. Check
  `Engine::isGridProducer()` and `primaryForecastSource` first when a request takes the
  "wrong" path.
* **One grid query per level.** Requests with many levels (all model levels of a
  producer) run one Query Server query per level and location.
* **Engine errors abort the request.** `executeQuery(query_sptr)` throws, for example
  `NO_PRODUCERS_FOUND` for an unknown producer, and the whole timeseries request fails.
