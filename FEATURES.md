# smartmet-plugin-timeseries — Feature List

A structured inventory of capabilities provided by the SmartMet
TimeSeries plugin. Use as a checklist when drafting release notes.
When new functionality is added, append the new entry under the matching
section (and bump the *Last updated* line at the bottom).

`smartmet-plugin-timeseries` is the primary HTTP query API of the
SmartMet Server. It produces `timeseries.so`, registered as a content
handler, and serves meteorological time series — forecasts,
observations, and grid-based fields — over a single URL with a rich
query-string DSL. Output is formatted into one of several text/binary
formats by the spine `TableFormatter` machinery.

---

## 1. Data sources

The plugin federates three data engines behind one query:

- **`querydata` engine (Q-engine)** — forecast / model data in
  QueryData format. **Required.**
- **`observation` engine (Obs-engine)** — station observations from
  SQLite, PostgreSQL or Oracle backends. **Optional** — compiled out
  when `WITHOUT_OBSERVATION` is defined.
- **`grid` engine (Grid-engine)** — GRIB1/GRIB2/NetCDF/QueryData grids
  via the grid stack. **Optional**.

Source selection is automatic: the producer name in the query routes
to the right engine. An explicit `source=...` query parameter can
override the routing (e.g. force the cached grid-engine view).

Supporting engines used internally:

- **`geonames` engine** — location lookups, keyword location sets,
  WKT geometry resolution.
- **`gis` engine** — PostGIS-backed geometry storage.

## 2. Location selection

A single time series request can ask for one or many locations using
several alternative syntaxes:

- **Place names** — `place=...`, `places=p1,p2,...`.
- **Geo IDs** — `geoid=...`.
- **Coordinates** — `lonlat=lon,lat` or `latlon=lat,lon`, and the bulk
  forms `lonlats=...`, `latlons=...`.
- **Cartesian projected** — `x=...&y=...` (with `crs=...`).
- **Station identifiers (Obs-engine)** — `fmisid=...`, `wmo=...`,
  `lpnn=...`.
- **Keywords** — `keyword=...` — predefined location sets from the
  geonames database.
- **WKT geometries** — full POINT/LINESTRING/POLYGON/etc. via the
  geonames engine, resolved to coordinate lists or coverage areas.
- **Bounding boxes** — `bbox=lon1,lat1,lon2,lat2`.
- **Maximum search radius** — `maxdistance=<value><unit>` (e.g.
  `25km`); falls back to plugin-configured default.
- **Area source override** — `areasource=...` to pick which geometry
  table the area name resolves against.
- **Nearest-valid-station** — `findnearestvalid=1` falls back from the
  exact requested location to the nearest station with data.

## 3. Time selection

- **Range** — `starttime=...`, `endtime=...` (absolute ISO 8601 or
  relative like `data` / `now-12h`).
- **Step** — `step=...` in minutes / hours.
- **Wall-clock pinning** — `now=<timestamp>` so the query behaves as if
  the current time were a specific instant (used by tests and
  reproducible workflows).
- **Time zone** — `tz=Europe/Helsinki` etc.
- **Time format** — `timeformat=iso|xml|sql|epoch|timestamp|...`,
  `timestring=...` for custom strftime-style formats.
- **Locale** — `locale=...`, `lang=...` (affects parameter labels and
  weekday names).
- **Weekday filter** — `weekday=...`.

## 4. Parameter selection

- **Parameter list** — comma-separated names in the `param=...` URL
  parameter. Each name can be a meteorological parameter, a derived
  function, or a special column.
- **Level selection** — `leveltype=...`, plus per-parameter level
  qualifiers (`levels`, `pressures`, `heights`).
- **Grouping** — `grouplocations=1` collapses per-location rows;
  `groupareas=1` collapses per-area rows.

## 5. Aggregation & post-processing

The `PostProcessing` and `AggregationInterval` modules apply value
transforms after the engine fetch:

- **Window aggregation** — min, max, mean, sum, percentile, count, etc.
  over a configurable interval per parameter.
- **Precision control** — global and per-parameter `precision=...`
  presets selected from the config file.
- **Missing-value rendering** — `missingtext=NaN` (or any string).
- **Value padding / fill** — `fill=...` to interpolate gaps.
- **Pagination** — `startrow=...`, `maxresults=...`.
- **Display formatting** — `uppercase=1`, `adjustfield=...`,
  `floatfield=...`, `showpos=1`, `width=...` (for fixed-width output).

## 6. Output formats

Selected via `format=...`; resolved through the spine
`TableFormatterFactory`:

- **JSON** — `format=json`.
- **XML** — `format=xml`.
- **CSV** — `format=csv`.
- **ASCII (debug)** — `format=ascii` / `format=debug`.
- **HTML** — `format=html`.
- **Serial** — `format=serial`.
- **Image** — `format=image` (special handling for image streams).
- **File** — `format=file` for binary file responses.
- **Info** — `format=info` returns a debug summary instead of data.
- **MIME negotiation** — the chosen formatter dictates the response
  `Content-Type` (with `charset=UTF-8`).
- **Configurable formatter options** — width, precision, adjustment
  loaded from the plugin config at startup.

## 7. Per-query state and caching

- **`State` object** — fixes the wall-clock time at the start of the
  request so all engines see a consistent "now".
- **QueryData cache (`QCache`, `TimedQCache`)** — per-query memo of
  resolved querydata handles.
- **`QueryLevelDataCache`** — caches per-level fetch results so the
  same level isn't re-fetched per parameter.
- **`ProducerDataPeriod`** — per-producer time-range cache.

## 8. Producer routing & engine dispatch

`QueryProcessingHub::processQuery()` inspects the producer set and
routes work to the appropriate engine wrapper:

- **`QEngineQuery`** — forecast / model data from the querydata engine.
  Handles parameter lookup, level interpolation, ensemble members,
  vertical profiles.
- **`ObsEngineQuery`** — observation queries. Translates the request
  into `ObsQueryParams`, runs the obs-engine, handles station joining,
  flag filtering, and time-bucketing. Compiled out under
  `WITHOUT_OBSERVATION`.
- **`GridEngineQuery`** — grid queries via `GridInterface`. Supports
  the same point / circle / rectangle / polygon shapes as the grid
  engine.
- **Mixed-source queries** — a single request can mix producers from
  different engines; results are joined on time and location.

## 9. Configuration

Loaded once at startup from the plugin config (`Config.cpp`):

- **Default values** — `defaultMaxDistance`, default formatter
  options, default precision presets.
- **Engine wiring** — which engines are mandatory vs optional.
- **Formatter options** — per-format width, precision, missing-text,
  adjustment, separators.
- **Precision presets** — named precision profiles selectable via
  the `precision=...` query parameter.
- **Producer list** — recognised producers with metadata used by
  `ProducerDataPeriod`.
- **Standard SmartMet config extensions** — `@include`, `@ifdef`,
  `$(VAR)`, `%(DIR)`.

## 10. HTTP request / response handling

- **`Plugin::requestHandler`** — top-level dispatcher.
- **`State` injection** — every request gets a fresh state object;
  caches live for the lifetime of the request only.
- **Spine reactor integration** — standard `SmartMetPlugin` lifecycle
  (`init`, `shutdown`, `requestHandler`).
- **Debug mode** — `format=debug` or `debug=1` returns extra
  diagnostic columns.
- **Streamed responses** — large outputs are streamed through the
  spine streaming infrastructure rather than buffered.
- **Image format streamer** — special path for image-typed outputs.

## 11. Testing

Integration tests live under `test/base/` (and `test/grid/`,
`test/cnf/`) and exercise the plugin end-to-end:

- **SQLite tests** — `make test-sqlite` (the CI default).
- **PostgreSQL tests** — `make test-postgresql`.
- **Oracle tests** — `make test-oracle`.
- **Grid-engine tests** — `make test-grid` (requires Redis).
- **Per-test filtering** — `make -C test/base test-sqlite
  TEST_FILTER="area_t2m"`.
- **Configurable timeout** — `TEST_TIMEOUT` (default 300 s).
- **Failure-diff capture** — `test/base/failures-{sqlite,oracle,
  postgresql}/`.
- **Skip files** — `.testignore_*` in `test/base/input/`.
- **Regenerable test database** — `make -C test/base testdatabase`.

## 12. Documentation

- **`docs/Using-the-Timeseries-API.md`** — full URL-query reference.
- **`docs/Examples.md`** — example requests for observations and
  forecasts.
- **`docs/docker.md`** — running the plugin in Docker.
- **`docs/img/`** — diagrams used in the docs.

## 13. Build & integration

- **Output**: `timeseries.so`.
- **Loaded at**: `$(prefix)/share/smartmet/plugins/timeseries.so`.
- **Build**: `make` (release) / `make debug`.
- **No-observation build**: define `WITHOUT_OBSERVATION` to skip
  the observation engine wiring.
- **Format target**: `make format` runs clang-format.
- **Install**: `make install`.
- **RPM**: `make rpm`.
- **CI**: CircleCI on RHEL 8 / RHEL 10 (`fmidev/smartmet-cibase-{8,10}`
  Docker images). SQLite tests run by default; other backend suites are
  optional.
- **Linked libraries**: `smartmet-library-spine`,
  `smartmet-library-macgyver`, `smartmet-library-newbase`,
  `smartmet-library-gis`, plus the standard SmartMet C++ stack.
- **Engine references** are deliberately unresolved at link time and
  resolved by the server via `dlopen` at runtime.

---

*Last updated: 2026-06-01.*
