----------------------------------------------------------------------
-- Global definitions
----------------------------------------------------------------------

local ParamValueMissing = -16777216;
local debug = 0;

local thunder_limit1 = 30;
local thunder_limit2 = 60;

local rain_limit1 = 0.025;
local rain_limit2 = 0.04;
local rain_limit3 = 0.4;
local rain_limit4 = 1.5;
local rain_limit5 = 2;
local rain_limit6 = 4;
local rain_limit7 = 7;

local cloud_limit1 = 7;
local cloud_limit2 = 20;
local cloud_limit3 = 33;
local cloud_limit4 = 46;
local cloud_limit5 = 59;
local cloud_limit6 = 72;
local cloud_limit7 = 85;
local cloud_limit8 = 93;
  
  

local smartSymbol = {};

smartSymbol['en'] = {}
smartSymbol['en'][1] = "clear";
smartSymbol['en'][2] = "mostly clear";
smartSymbol['en'][4] = "partly cloudy";
smartSymbol['en'][6] = "mostly cloudy";
smartSymbol['en'][7] = "overcast";
smartSymbol['en'][9] = "fog";
smartSymbol['en'][71] = "isolated thundershowers";
smartSymbol['en'][74] = "scattered thundershowers";
smartSymbol['en'][77] = "thundershowers";
smartSymbol['en'][21] = "isolated showers";
smartSymbol['en'][24] = "scattered showers";
smartSymbol['en'][27] = "showers";
smartSymbol['en'][11] = "drizzle";
smartSymbol['en'][14] = "freezing drizzle";
smartSymbol['en'][17] = "freezing rain";
smartSymbol['en'][31] = "periods of light rain";
smartSymbol['en'][34] = "periods of light rain";
smartSymbol['en'][37] = "light rain";
smartSymbol['en'][32] = "periods of moderate rain";
smartSymbol['en'][35] = "periods of moderate rain";
smartSymbol['en'][38] = "moderate rain";
smartSymbol['en'][33] = "periods of heavy rain";
smartSymbol['en'][36] = "periods of heavy rain";
smartSymbol['en'][39] = "heavy rain";
smartSymbol['en'][41] = "isolated light sleet showers";
smartSymbol['en'][44] = "scattered light sleet showers";
smartSymbol['en'][47] = "light sleet";
smartSymbol['en'][42] = "isolated moderate sleet showers";
smartSymbol['en'][45] = "scattered moderate sleet showers";
smartSymbol['en'][48] = "moderate sleet";
smartSymbol['en'][43] = "isolated heavy sleet showers";
smartSymbol['en'][46] = "scattered heavy sleet showers";
smartSymbol['en'][49] = "heavy sleet";
smartSymbol['en'][51] = "isolated light snow showers";
smartSymbol['en'][54] = "scattered light snow showers";
smartSymbol['en'][57] = "light snowfall";
smartSymbol['en'][52] = "isolated moderate snow showers";
smartSymbol['en'][55] = "scattered moderate snow showers";
smartSymbol['en'][58] = "moderate snowfall";
smartSymbol['en'][53] = "isolated heavy snow showers";
smartSymbol['en'][56] = "scattered heavy snow showers";
smartSymbol['en'][59] = "heavy snowfall";
smartSymbol['en'][61] = "isolated hail showers";
smartSymbol['en'][64] = "scattered hail showers";
smartSymbol['en'][67] = "hail showers";

smartSymbol['sv'] = {}
smartSymbol['sv'][1] = "klart";
smartSymbol['sv'][2] = "mest klart";
smartSymbol['sv'][4] = "halvklart";
smartSymbol['sv'][6] = "molnight";
smartSymbol['sv'][7] = "mulet";
smartSymbol['sv'][9] = "dimma";
smartSymbol['sv'][71] = "enstaka åskskurar";
smartSymbol['sv'][74] = "lokalt åskskurar";
smartSymbol['sv'][77] = "åskskurar";
smartSymbol['sv'][21] = "enstaka regnskurar";
smartSymbol['sv'][24] = "lokalt regnskurar";
smartSymbol['sv'][27] = "regnskurar";
smartSymbol['sv'][11] = "duggregn";
smartSymbol['sv'][14] = "underkylt duggregn";
smartSymbol['sv'][17] = "underkylt regn";
smartSymbol['sv'][31] = "tidvis lätt regn";
smartSymbol['sv'][34] = "tidvis lätt regn";
smartSymbol['sv'][37] = "lätt regn";
smartSymbol['sv'][32] = "tidvis måttligt regn";
smartSymbol['sv'][35] = "tidvis måttligt regn";
smartSymbol['sv'][38] = "måttligt regn";
smartSymbol['sv'][33] = "tidvis kraftigt regn";
smartSymbol['sv'][36] = "tidvis kraftigt regn";
smartSymbol['sv'][39] = "kraftigt regn";
smartSymbol['sv'][41] = "tidvis lätta byar ov snöblandat regn";
smartSymbol['sv'][44] = "tidvis lätta byar avd snäblandat regn";
smartSymbol['sv'][47] = "lätt snöblandat regn";
smartSymbol['sv'][42] = "tidvis måttliga byar av snöblandat regn";
smartSymbol['sv'][45] = "tidvis måttliga byar av snöblandat regn";
smartSymbol['sv'][48] = "måttligt snöblandat regn";
smartSymbol['sv'][43] = "tidvis kraftiga byar av snöblandat regn";
smartSymbol['sv'][46] = "tidvis kraftiga byar av snöblandat regn";
smartSymbol['sv'][49] = "kraftigt snöblandat regn";
smartSymbol['sv'][51] = "tidvis lätta snöbyar";
smartSymbol['sv'][54] = "tidvis lätta snöbyar";
smartSymbol['sv'][57] = "tidvis lätt snöfall";
smartSymbol['sv'][52] = "tidvis måttliga snöbyar";
smartSymbol['sv'][55] = "tidvis måttliga snöbyar";
smartSymbol['sv'][58] = "måttligt snöfall";
smartSymbol['sv'][53] = "tidvis ymniga snöbyar";
smartSymbol['sv'][56] = "tidvis ymniga snöbyar";
smartSymbol['sv'][59] = "ymnigt snöfall";
smartSymbol['sv'][61] = "enstaka hagelskurar";
smartSymbol['sv'][64] = "lokalt hagelskurar";
smartSymbol['sv'][67] = "hagelskurar";

smartSymbol['fi'] = {}
smartSymbol['fi'][1] = "selkeää";
smartSymbol['fi'][2] = "enimmäkseen selkeää";
smartSymbol['fi'][4] = "puolipilvistä";
smartSymbol['fi'][6] = "enimmäkseen pilvistä";
smartSymbol['fi'][7] = "pilvistä";
smartSymbol['fi'][9] = "sumua";
smartSymbol['fi'][71] = "yksittäisiä ukkoskuuroja";
smartSymbol['fi'][74] = "paikoin ukkoskuuroja";
smartSymbol['fi'][77] = "ukkoskuuroja";
smartSymbol['fi'][21] = "yksittäisiä sadekuuroja";
smartSymbol['fi'][24] = "paikoin sadekuuroja";
smartSymbol['fi'][27] = "sadekuuroja";
smartSymbol['fi'][11] = "tihkusadetta";
smartSymbol['fi'][14] = "jäätävää tihkua";
smartSymbol['fi'][17] = "jäätävää sadetta";
smartSymbol['fi'][31] = "ajoittain heikkoa vesisadetta";
smartSymbol['fi'][34] = "ajoittain heikkoa vesisadetta";
smartSymbol['fi'][37] = "heikkoa vesisadetta";
smartSymbol['fi'][32] = "ajoittain kohtalaista vesisadetta";
smartSymbol['fi'][35] = "ajoittain kohtalaista vesisadetta";
smartSymbol['fi'][38] = "kohtalaista vesisadetta";
smartSymbol['fi'][33] = "ajoittain voimakasta vesisadetta";
smartSymbol['fi'][36] = "ajoittain voimakasta vesisadetta";
smartSymbol['fi'][39] = "voimakasta vesisadetta";
smartSymbol['fi'][41] = "ajoittain heikkoja räntäkuuroja";
smartSymbol['fi'][44] = "ajoittain heikkoja räntäkuuroja";
smartSymbol['fi'][47] = "heikkoa räntäsadetta";
smartSymbol['fi'][42] = "ajoittain kohtalaisia räntäkuuroja";
smartSymbol['fi'][45] = "ajoittain kohtalaisia räntäkuuroja";
smartSymbol['fi'][48] = "kohtalaista räntäsadetta";
smartSymbol['fi'][43] = "ajoittain voimakkaita räntäkuuroja";
smartSymbol['fi'][46] = "ajoittain voimakkaita räntäkuuroja";
smartSymbol['fi'][49] = "voimakasta räntäsadetta";
smartSymbol['fi'][51] = "ajoittain heikkoja lumikuuroja";
smartSymbol['fi'][54] = "ajoittain heikkoja lumikuuroja";
smartSymbol['fi'][57] = "heikkoa lumisadetta";
smartSymbol['fi'][52] = "ajoittain kohtalaisia lumikuuroja";
smartSymbol['fi'][55] = "ajoittain kohtalaisia lumikuuroja";
smartSymbol['fi'][58] = "kohtalaista lumisadetta";
smartSymbol['fi'][53] = "ajoittain sakeita lumikuuroja";
smartSymbol['fi'][56] = "ajoittain sakeita lumikuuroja";
smartSymbol['fi'][59] = "runsasta lumisadetta";
smartSymbol['fi'][61] = "yksittäisiä raekuuroja";
smartSymbol['fi'][64] = "paikoin raekuuroja";
smartSymbol['fi'][67] = "raekuuroja";

smartSymbol['default'] = smartSymbol['fi'];


----------------------------------------------------------------------
-- Weather texts for weather symbols in different languages:
----------------------------------------------------------------------

local weather = {};

weather['en'] = {}
weather['en'][1] = "sunny";
weather['en'][2] = "partly cloudy";
weather['en'][3] = "cloudy";
weather['en'][21] = "light showers";
weather['en'][22] = "showers";
weather['en'][23] = "heavy showers";
weather['en'][31] = "light rain";
weather['en'][32] = "rain";
weather['en'][33] = "heavy rain";
weather['en'][41] = "light snow showers";
weather['en'][42] = "snow showers";
weather['en'][43] = "heavy snow showers";
weather['en'][51] = "light snowfall";
weather['en'][52] = "snowfall";
weather['en'][53] = "heavy snowfall";
weather['en'][61] = "thundershowers";
weather['en'][62] = "heavy thundershowers";
weather['en'][63] = "thunder";
weather['en'][64] = "heavy thunder";
weather['en'][71] = "light sleet showers";
weather['en'][72] = "sleet showers";
weather['en'][73] = "heavy sleet showers";
weather['en'][81] = "light sleet rain";
weather['en'][82] = "sleet rain";
weather['en'][83] = "heavy sleet rain";
weather['en'][91] = "fog";
weather['en'][92] = "fog";

weather['sv'] = {}
weather['sv'][1] = "klart";
weather['sv'][2] = "halvklart";
weather['sv'][3] = "mulet";
weather['sv'][21] = "lätta regnskurar";
weather['sv'][22] = "regnskurar";
weather['sv'][23] = "kraftiga regnskurar";
weather['sv'][31] = "lätt regn";
weather['sv'][32] = "regn";
weather['sv'][33] = "rikligt regn";
weather['sv'][41] = "lätta snöbyar";
weather['sv'][42] = "snöbyar";
weather['sv'][43] = "täta snöbyar";
weather['sv'][51] = "lätt snöfall";
weather['sv'][52] = "snöfall";
weather['sv'][53] = "ymnigt snöfall";
weather['sv'][61] = "åskskurar";
weather['sv'][62] = "kraftiga åskskurar";
weather['sv'][63] = "åska";
weather['sv'][64] = "häftigt åskväder";
weather['sv'][71] = "lätta skurar av snöblandat regn";
weather['sv'][72] = "skurar av snöblandat regn";
weather['sv'][73] = "kraftiga skurar av snöblandad regn";
weather['sv'][81] = "lätt snöblandat regn";
weather['sv'][82] = "snöblandat regn";
weather['sv'][83] = "kraftigt snöblandat regn";
weather['sv'][91] = "dis";
weather['sv'][92] = "dimma";

weather['et'] = {}
weather['et'][1] = "selge";
weather['et'][2] = "poolpilves";
weather['et'][3] = "pilves";
weather['et'][21] = "kerged vihmahood";
weather['et'][22] = "hoogvihm";
weather['et'][23] = "tugevad vihmahood";
weather['et'][31] = "nõrk vihmasadu";
weather['et'][32] = "vihmasadu";
weather['et'][33] = "vihmasadu";
weather['et'][41] = "nõrgad lumehood";
weather['et'][42] = "hooglumi";
weather['et'][43] = "tihedad lumesajuhood";
weather['et'][51] = "nõrk lumesadu";
weather['et'][52] = "lumesadu";
weather['et'][53] = "tihe lumesadu";
weather['et'][61] = "äikesehood";
weather['et'][62] = "tugevad äikesehood";
weather['et'][63] = "äike";
weather['et'][64] = "tugev äike";
weather['et'][71] = "ñörgad lörtsihood";
weather['et'][72] = "lörtsihood";
weather['et'][73] = "tugev lörtsihood";
weather['et'][81] = "nõrk lörtsisadu";
weather['et'][82] = "lörtsisadu";
weather['et'][83] = "tugev lörtsisadu";
weather['et'][91] = "udu";
weather['et'][92] = "uduvinet";

weather['fi'] = {}
weather['fi'][1] = "selkeää";
weather['fi'][2] = "puolipilvistä";
weather['fi'][3] = "pilvistä";
weather['fi'][21] = "heikkoja sadekuuroja";
weather['fi'][22] = "sadekuuroja";
weather['fi'][23] = "voimakkaita sadekuuroja";
weather['fi'][31] = "heikkoa vesisadetta";
weather['fi'][32] = "vesisadetta";
weather['fi'][33] = "voimakasta vesisadetta";
weather['fi'][41] = "heikkoja lumikuuroja";
weather['fi'][42] = "lumikuuroja";
weather['fi'][43] = "voimakkaita lumikuuroja";
weather['fi'][51] = "heikkoa lumisadetta";
weather['fi'][52] = "lumisadetta";
weather['fi'][53] = "voimakasta lumisadetta";
weather['fi'][61] = "ukkoskuuroja";
weather['fi'][62] = "voimakkaita ukkoskuuroja";
weather['fi'][63] = "ukkosta";
weather['fi'][64] = "voimakasta ukkosta";
weather['fi'][71] = "heikkoja räntäkuuroja";
weather['fi'][72] = "räntäkuuroja";
weather['fi'][73] = "voimakkaita räntäkuuroja";
weather['fi'][81] = "heikkoa räntäsadetta";
weather['fi'][82] = "räntäsadetta";
weather['fi'][83] = "voimakasta räntäsadetta";
weather['fi'][91] = "utua";
weather['fi'][92] = "sumua";

weather['default'] = weather['fi'];


local temp = {};
temp['en'] = {};
temp['en'][1] = "cold";
temp['en'][2] = "hot";

temp['default'] = temp['en'];



----------------------------------------------------------------------
-- WindCompass8 texts in different languages:
----------------------------------------------------------------------

local windCompass8 = {};

windCompass8['en'] = {
  "N","NE","E","SE","S","SW","W","NW"
};

windCompass8['default'] = windCompass8['en'];


----------------------------------------------------------------------
-- WindCompass16 texts in different languages:
----------------------------------------------------------------------

local windCompass16 = {};

windCompass16['en'] = {
  "N","NNE","NE","ENE","E","ESE","SE","SSE",
  "S","SSW","SW","WSW","W","WNW","NW","NNW"
};

windCompass16['default'] = windCompass16['en'];



----------------------------------------------------------------------
-- WindCompass32 texts in different languages:
----------------------------------------------------------------------

local windCompass32 = {};

windCompass32['en'] = {
  "N", "NbE", "NNE", "NEbN", "NE", "NEbE", "ENE", "EbN",
  "E", "EbS", "ESE", "SEbE", "SE", "SEbS", "SSE", "SbE",
  "S", "SbW", "SSW", "SWbS", "SW", "SWbW", "WSW", "WbS",
  "W", "WbN", "WNW", "NWbW", "NW", "NWbN", "NNW", "NbW"
};

windCompass32['default'] = windCompass32['en'];




-- ***********************************************************************
--  FUNCTION : SSI / SummerSimmerIndex
-- ***********************************************************************
--  This is an internal function used by NB_FeelsLikeTemperature function.
-- ***********************************************************************

function SSI(rh,t)

  local simmer_limit = 14.5;

  if (t <= simmer_limit) then 
    return t;
  end

  if (rh == ParamValueMissing) then 
    return ParamValueMissing;
  end

  if (t == ParamValueMissing) then 
    return ParamValueMissing;
  end

  local rh_ref = 50.0 / 100.0;
  local r = rh / 100.0;

  local value =  (1.8 * t - 0.55 * (1 - r) * (1.8 * t - 26) - 0.55 * (1 - rh_ref) * 26) / (1.8 * (1 - 0.55 * (1 - rh_ref)));         
  return value;

end





-- ***********************************************************************
--  FUNCTION : NB_SummerSimmerIndex
-- ***********************************************************************
--  SummerSimmerIndex
-- ***********************************************************************

function NB_SummerSimmerIndex(numOfParams,params)

  local result = {};

  if (numOfParams ~= 2) then
    result.message = 'Invalid number of parameters!';
    result.value = 0;  
    return result.value,result.message;
  end

  local rh = params[1];
  local t = params[2];
  local simmer_limit = 14.5;

  -- The chart is vertical at this temperature by 0.1 degree accuracy
  if (t <= simmer_limit) then 
    result.message = 'OK';
    result.value = t;
    return result.value,result.message;
  end

  if (rh == ParamValueMissing) then 
    result.message = 'OK';
    result.value = ParamValueMissing;
    return result.value,result.message;
  end

  if (t == ParamValueMissing) then 
    result.message = 'OK';
    result.value = ParamValueMissing;
    return result.value,result.message;
  end

  -- SSI

  -- When in Finland and when > 14.5 degrees, 60% is approximately
  -- the minimum mean monthly humidity. However, Google wisdom
  -- claims most humans feel most comfortable either at 45%, or
  -- alternatively somewhere between 50-60%. Hence we choose
  -- the middle ground 50%

  local rh_ref = 50.0 / 100.0;
  local r = rh / 100.0;

  result.message = 'OK';
  result.value =  (1.8 * t - 0.55 * (1 - r) * (1.8 * t - 26) - 0.55 * (1 - rh_ref) * 26) / (1.8 * (1 - 0.55 * (1 - rh_ref)));         
  return result.value,result.message;

end






-- ***********************************************************************
--  FUNCTION : NB_FeelsLikeTemperature
-- ***********************************************************************

function NB_FeelsLikeTemperature(numOfParams,params)

  local result = {};

  if (numOfParams ~= 4) then
    result.message = 'Invalid number of parameters!';
    result.value = 0;  
    return result.value,result.message;
  end

  local wind = params[1];
  local rh = params[2];
  local temp = params[3]; 
  local rad = params[4];

  if (wind == ParamValueMissing) then 
    result.message = 'OK';
    result.value = ParamValueMissing;
    return result.value,result.message;
  end

  if (temp == ParamValueMissing) then 
    result.message = 'OK';
    result.value = ParamValueMissing;
    return result.value,result.message;
  end
  
  if (rh == ParamValueMissing) then 
    result.message = 'OK';
    result.value = ParamValueMissing;
    return result.value,result.message;
  end

  -- Calculate adjusted wind chill portion. Note that even though
  -- the Canadien formula uses km/h, we use m/s and have fitted
  -- the coefficients accordingly. Note that (a*w)^0.16 = c*w^16,
  -- i.e. just get another coefficient c for the wind reduced to 1.5 meters.

  local a = 15.0;   -- using this the two wind chills are good match at T=0
  local t0 = 37.0;  -- wind chill is horizontal at this T

  local chill = a + (1 - a / t0) * temp + a / t0 * math.pow(wind + 1, 0.16) * (temp - t0);

  -- Heat index

  local heat = SSI(rh, temp);

  -- Add the two corrections together

  local feels = temp + (chill - temp) + (heat - temp);

  -- Radiation correction done only when radiation is available
  -- Based on the Steadman formula for Apparent temperature,
  -- we just inore the water vapour pressure adjustment

  if (rad ~= ParamValueMissing) then
  
    -- Chosen so that at wind=0 and rad=800 the effect is 4 degrees
    -- At rad=50 the effect is then zero degrees
    
    local absorption = 0.07;
    feels = feels + 0.7 * absorption * rad / (wind + 10) - 0.25;

  end

  result.message = 'OK';
  result.value = feels;
  return result.value,result.message;
  
end





-- ***********************************************************************
--  FUNCTION : NB_WindChill
-- ***********************************************************************
-- Return the wind chill, e.g., the equivalent no-wind temperature
-- felt by a human for the given wind speed.
--
-- The formula is the new official one at FMI taken into use in 12.2003.
-- See: http://climate.weather.gc.ca/climate_normals/normals_documentation_e.html
--
-- Note that Canadian formula uses km/h:
--
-- W = 13.12 + 0.6215 × Tair - 11.37 × V10^0.16 + 0.3965 × Tair × V10^0.16
-- W = Tair + [(-1.59 + 0.1345 × Tair)/5] × V10m, when V10m < 5 km/h
--
-- \param wind The observed wind speed in m/s
-- \param temp The observed temperature in degrees Celsius
-- \return Equivalent no-wind temperature
-- ***********************************************************************

function NB_WindChill(numOfParams,params)

  local result = {};

  if (numOfParams ~= 2) then
    result.message = 'Invalid number of parameters!';
    result.value = 0;  
    return result.value,result.message;
  end

  local wind = params[1];
  local temp = params[2];

  if (wind == ParamValueMissing or temp == ParamValueMissing or wind < 0) then
    result.message = "OK!"
    result.value = ParamValueMissing;
    return result.value,result.message;
  end

  local kmh = wind * 3.6;

  if (kmh < 5.0) then
    result.message = "OK"
    result.value = temp + (-1.59 + 0.1345 * temp) / 5.0 * kmh;
    return result.value,result.message;
  end

  local wpow = math.pow(kmh, 0.16);

  result.message = "OK"
  result.value = 13.12 + 0.6215 * temp - 11.37 * wpow + 0.3965 * temp * wpow;
  return result.value,result.message;

end





-- ***********************************************************************
--  FUNCTION : NB_WindCompass8
-- ***********************************************************************

function NB_WindCompass8(language,numOfParams,params)

  local result = {};

    -- for index, value in pairs(params) do
    --   print(index.." : "..value);
    -- end

  if (numOfParams ~= 1) then
    result.message = 'Invalid number of parameters!';
    result.value = 0;  
    return result.value,result.message;
  end

  local windDir = params[1];

  if (windDir == ParamValueMissing) then
    result.message = "OK"
    result.value = "nan"; --ParamValueMissing;
    return result.value,result.message;
  end

  local i = math.floor(((windDir + 22.5) / 45) % 8) + 1;
 
  result.message = "OK"
  if (windCompass8[language] ~= nil  and  windCompass8[language][i] ~= nil) then 
    result.value = windCompass8[language][i];
  else
    if (windCompass8['default'] ~= nil  and  windCompass8['default'][i] ~= nil) then 
    result.value = windCompass8['default'][i]
   end
  end
  
  return result.value,result.message;
  
end





-- ***********************************************************************
--  FUNCTION : NB_WindCompass16
-- ***********************************************************************

function NB_WindCompass16(language,numOfParams,params)

  local result = {};

  if (numOfParams ~= 1) then
    result.message = 'Invalid number of parameters!';
    result.value = 0;  
    return result.value,result.message;
  end

  local windDir = params[1];

  if (windDir == ParamValueMissing) then
    result.message = "OK"
    result.value = "nan"; --ParamValueMissing;
    return result.value,result.message;
  end

  local i = math.floor(((windDir + 11.25) / 22.5)) % 16 + 1;

  result.message = "OK"
  if (windCompass16[language] ~= nil  and  windCompass16[language][i] ~= nil) then 
    result.value = windCompass16[language][i];
  else
    if (windCompass16['default'] ~= nil  and  windCompass16['default'][i] ~= nil) then 
    result.value = windCompass16['default'][i]
   end
  end

  return result.value,result.message;

end





-- ***********************************************************************
--  FUNCTION : NB_WindCompass32
-- ***********************************************************************

function NB_WindCompass32(language,numOfParams,params)

  local result = {};

  if (numOfParams ~= 1) then
    result.message = 'Invalid number of parameters!';
    result.value = 0;  
    return result.value,result.message;
  end

  local windDir = params[1];

  if (windDir == ParamValueMissing) then
    result.message = "OK"
    result.value = "nan"; --ParamValueMissing;
    return result.value,result.message;
  end

  local i = math.floor((windDir + 5.625) / 11.25) % 32 + 1;

  result.message = "OK"
  if (windCompass32[language] ~= nil  and  windCompass32[language][i] ~= nil) then 
    result.value = windCompass32[language][i];
  else
    if (windCompass32['default'] ~= nil  and  windCompass32['default'][i] ~= nil) then 
    result.value = windCompass32['default'][i]
   end
  end

  return result.value,result.message;

end






-- ***********************************************************************
--  FUNCTION : NB_WeatherText
-- ***********************************************************************

function NB_WeatherText(language,numOfParams,params)

  local result = {};

  if (numOfParams ~= 1) then
    result.message = 'Invalid number of parameters!';
    result.value = 0;  
    return result.value,result.message;
  end

  local weatherSymbol = params[1];

  result.message = "OK"
  
  if (weather[language] ~= nil  and  weather[language][weatherSymbol] ~= nil)  then 
    result.value = weather[language][weatherSymbol];
  else
    if (weather['default'] ~= nil and weather['default'][weatherSymbol] ~= nil) then  
      result.value = weather['default'][weatherSymbol];
    end
  end

  return result.value,result.message;

end
 




-- ***********************************************************************
--  FUNCTION : NB_SmartSymbolText
-- ***********************************************************************

function NB_SmartSymbolText(language,numOfParams,params)

  local result = {};

  if (numOfParams ~= 1) then
    result.message = 'Invalid number of parameters!';
    result.value = 0;  
    return result.value,result.message;
  end

  local smartSymbolNumber = params[1];
  if (smartSymbolNumber > 100) then
    smartSymbolNumber = smartSymbolNumber - 100;
  end

  result.message = "OK"
  
  if (smartSymbol[language] ~= nil  and  smartSymbol[language][smartSymbolNumber] ~= nil)  then 
    result.value = smartSymbol[language][smartSymbolNumber];
  else
    if (smartSymbol['default'] ~= nil and smartSymbol['default'][smartSymbolNumber] ~= nil) then  
      result.value = smartSymbol['default'][smartSymbolNumber];
    end
  end

  return result.value,result.message;

end




-- ***********************************************************************
--  FUNCTION : NB_TemperatureText
-- ***********************************************************************

function NB_TemperatureText(language,numOfParams,params)

  local result = {};

  if (numOfParams ~= 1) then
    result.message = 'Invalid number of parameters!';
    result.value = 0;  
    return result.value,result.message;
  end

  local tempIdx = params[1];

  result.message = "OK"
  
  if (temp[language] ~= nil  and  temp[language][tempIdx] ~= nil)  then 
    result.value = temp[language][tempIdx];
  else
    if (temp['default'] ~= nil and temp['default'][tempIdx] ~= nil) then  
      result.value = temp['default'][tempIdx];
    end
  end

  return result.value,result.message;

end




-- ***********************************************************************
--  FUNCTION : SnowWaterRatio
-- ***********************************************************************
--  Calculate water to snow conversion factor
-- 
-- The formula is based on the table given on page 12 of the
-- M.Sc thesis by Ivan Dube titled "From mm to cm... Study of
-- snow/liquid water ratios in Quebec".
-- 
-- \param t The air temperature
-- \param ff The wind speed in m/s
-- \return The snow/water ratio
-- ***********************************************************************

function SnowWaterRatio(t,ff)

  if (t == ParamValueMissing or ff == ParamValueMissing) then
    return 10.0;
  end

  local knot = 0.51444444444444444444;

  if (ff < 10 * knot) then
    if (t > 0) then
      return 10;
    elseif (t > -5) then
      return 11;
    elseif (t > -10) then
      return 14;
    elseif (t > -20) then
      return 17;
    else
      return 15;
    end  
  elseif (ff < 20 * knot) then
    if (t > -5) then
      return 10;
    elseif (t > -10) then
      return 12;
    elseif (t > -20) then
      return 15;
    else
      return 13;
    end
  end
  
  if (t > -10) then
    return 10;
  elseif (t > -15) then
    return 11;
  elseif (t > -20) then
    return 14;
  end
  
  return 12;
end





-- ***********************************************************************
--  FUNCTION : NB_Snow1h
-- ***********************************************************************

function NB_Snow1h(numOfParams,params)

  local result = {};

  if (numOfParams ~= 4) then
    result.message = 'Invalid number of parameters!';
    result.value = 0;  
    return result.value,result.message;
  end

  local snow1h = params[1];
  local t = params[2];
  local wspd = params[3];
  local prec1h = params[4];

  -- Use the actual Snow1h if it is present

  if (snow1h ~= ParamValueMissing) then
    result.message = 'OK';
    result.value = snow1h;  
    return result.value,result.message;
  end
  
  if (t == kFloatMissing or wspd == kFloatMissing or prec1h == kFloatMissing) then
    result.message = 'OK';
    result.value = ParamValueMissing;
    return result.value,result.message;
  end

  snow1h = prec1h * SnowWaterRatio(t, wspd);  

  result.message = 'OK';
  result.value = snow1h;  
  return result.value,result.message;
end




-- ***********************************************************************
--  Cloudiness8th
-- ***********************************************************************

function NB_Cloudiness8th(numOfParams,params)

  local result = {};

  if (numOfParams ~= 1) then
    result.message = 'Invalid number of parameters!';
    result.value = 0;  
    return result.value,result.message;
  end

  local totalCloudCover = params[1];

  local n = math.ceil(totalCloudCover / 12.5);

  result.message = 'OK';
  result.value = n;  
  return result.value,result.message;
end



function MY_TEXT(language,numOfParams,params)

  local result = {};

  if (numOfParams ~= 1) then
    result.message = 'Invalid number of parameters!';
    result.value = 0;  
    return result.value,result.message;
  end

  if (params[1] <= 280) then
    if (language == "en") then
      result.value = "COLD";
    else
      result.value = "KYLMÄÄ";
    end
  else
    if (language == "en") then
      result.value = "NOT SO COLD";
    else
      result.value = "EI NIIN KYLMÄÄ";
    end
  end

  result.message = "OK"
  return result.value,result.message;
  
end





function getSmartSymbol(wawa,cloudiness,temperature)

  local wawa_group1 = {0, 4, 5, 10, 20, 21, 22, 23, 24, 25};
  local wawa_group2 = {30, 31, 32, 33, 34};

  local cloudiness_limit1 = 0;
  local cloudiness_limit2 = 1;
  local cloudiness_limit3 = 5;
  local cloudiness_limit4 = 7;
  local cloudiness_limit5 = 9;

  for index, w in pairs(wawa_group1) do
    --print(index.." : "..w);

    if (w == wawa) then
  
      if (cloudiness <= cloudiness_limit1) then
        return 1;
      end
               
      if (cloudiness <= cloudiness_limit2) then
        return 2;
      end

      if (cloudiness <= cloudiness_limit3) then
        return 4;
      end

      if (cloudiness <= cloudiness_limit4) then
        return 6;
      end
      
      if (cloudiness <= cloudiness_limit5) then
        return 7;
      end
    end
  end
  
  
  for index, w in pairs(wawa_group2) do
    -- print(index.." : "..w);

    if (w == wawa) then

      if (cloudiness <= cloudiness_limit1) then
        return 1;
      end
               
      if (cloudiness <= cloudiness_limit2) then
        return 2;
      end

      if (cloudiness <= cloudiness_limit3) then
        return 4;
      end

      if (cloudiness <= cloudiness_limit4) then
        return 6;
      end
      
      if (cloudiness <= cloudiness_limit5) then
        return 9;
      end
    
    end
  
  end

  if (wawa == 40 or wawa == 41) then
  
    if (temperature <= 0) then
    
      if (cloudiness <= 5) then
        return 51;
      end

      if (cloudiness <= 7) then
        return 54;
      end

      if (cloudiness <= 9) then
        return 57;
      end
    
    else
    
      if (cloudiness <= 5) then
        return 31;
      end

      if (cloudiness <= 7) then
        return 34;
      end

      if (cloudiness <= 9) then
        return 37;
      end
    
    end
  
  end
  
  
  if (wawa == 42) then
  
    if (temperature <= 0) then
    
      if (cloudiness <= 5) then
        return 53;
      end
        
      if (cloudiness <= 7) then
        return 56;
      end

      if (cloudiness <= 9) then
        return 37;
      end
    
    else

      if (cloudiness <= 5) then
        return 33;
      end

      if (cloudiness <= 7) then
        return 36;
      end

      if (cloudiness <= 9) then
        return 39;
      end
  
    end
  
  end
  
  
  if (wawa >= 50 and wawa <= 53) then 
  
    if (cloudiness <= 9) then
      return 11;
    end
  
  end
  
  
  if (wawa >= 54 and wawa <= 56) then 
  
    if (cloudiness <= 9) then
      return 14;
    end
  
  end
  
  
  if (wawa == 60) then
  
    if (cloudiness <= 5) then
      return 31;
    end
    
    if (cloudiness <= 7) then
      return 34;
    end
    
    if (cloudiness <= 9) then
      return 37;
    end
  end
  
  
  if (wawa == 61) then

    if (cloudiness <= 5) then
      return 31;
    end
    
    if (cloudiness <= 7) then
      return 34;
    end

    if (cloudiness <= 9) then
      return 37;
    end

  end
  
  
  if (wawa == 62) then

    if (cloudiness <= 5) then
      return 32;
    end
    
    if (cloudiness <= 7) then
      return 35;
    end
    
    if (cloudiness <= 9) then
      return 38;
    end
  end

  
  if (wawa == 63) then
  
    if (cloudiness <= 5) then
      return 33;
    end
    
    if (cloudiness <= 7) then
      return 36;
    end
    
    if (cloudiness <= 9) then
      return 39;
    end

  end

  
  if (wawa >= 64 and wawa <= 66) then

    if (cloudiness <= 9) then
      return 17;
    end

  end
  

  if (wawa == 67) then

    if (cloudiness <= 5) then
      return 41;
    end
    
    if (cloudiness <= 7) then
      return 44;
    end

    if (cloudiness <= 9) then
      return 47;
    end

  end
  

  if (wawa == 68) then
  
    if (cloudiness <= 5) then
      return 42;
    end

    if (cloudiness <= 7) then
      return 45;
    end

    if (cloudiness <= 9) then
      return 48;
    end

  end
  

  if (wawa == 70) then

    if (cloudiness <= 5) then
      return 51;
    end

    if (cloudiness <= 7) then
      return 54;
    end

    if (cloudiness <= 9) then
      return 57;
    end

  end
  

  if (wawa == 71) then
  
    if (cloudiness <= 5) then
      return 51;
    end

    if (cloudiness <= 7)  then
      return 54;
    end
    
    if (cloudiness <= 9) then
      return 57;
    end

  end
  
  if (wawa == 72) then
  
    if (cloudiness <= 5) then
      return 52;
    end

    if (cloudiness <= 7) then
      return 55;
    end
    
    if (cloudiness <= 9) then
      return 58;
    end
  end

  
  if (wawa == 73) then
  
    if (cloudiness <= 5) then
      return 53;
    end

    if (cloudiness <= 7) then
      return 56;
    end

    if (cloudiness <= 9) then
      return 59;
    end

  end

  
  if (wawa == 74) then

    if (cloudiness <= 5) then
      return 51;
    end


    if (cloudiness <= 7) then
      return 54;
    end


    if (cloudiness <= 9) then
      return 57;
    end

  end

  
  if (wawa == 75) then
  
    if (cloudiness <= 5) then
      return 52;
    end


    if (cloudiness <= 7) then
      return 55;
    end

    if (cloudiness <= 9) then
      return 58;
    end
  end
  

  if (wawa == 76) then

    if (cloudiness <= 5) then
      return 53;
    end

    if (cloudiness <= 7) then
      return 56;
    end

    if (cloudiness <= 9) then
      return 59;
    end

  end
  
  
  if (wawa == 77) then
  
    if (cloudiness <= 9) then
      return 57;
    end

  end
  

  if (wawa == 78) then
  
    if (cloudiness <= 9) then
      return 57;
    end

  end
  

  if (wawa == 80) then
  
    if (temperature <= 0) then
    
      if (cloudiness <= 5) then
        return 51;
      end
      
      if (cloudiness <= 7) then
        return 54;
      end

      if (cloudiness <= 9) then
        return 57;
      end
    
    else
    
      if (cloudiness <= 5) then
        return 21;
      end
      
      if (cloudiness <= 7) then
        return 24;
      end

      if (cloudiness <= 9) then
        return 27;
      end

    end

  end

  
  if (wawa >= 81 and wawa <= 84) then
  
    if (cloudiness <= 5) then
      return 21;
    end

    if (cloudiness <= 7) then
      return 24;
    end

    if (cloudiness <= 9) then
      return 27;
    end

  end
  

  if (wawa == 85) then
  
    if (cloudiness <= 5) then
      return 51;
    end

    if (cloudiness <= 7) then
      return 54;
    end

    if (cloudiness <= 9) then
      return 57;
    end

  end

  
  if (wawa == 86) then
  
    if (cloudiness <= 5) then
      return 52;
    end

    if (cloudiness <= 7) then
      return 55;
    end

    if (cloudiness <= 9) then
      return 58;
    end

  end
  
  if (wawa == 87) then
  
    if (cloudiness <= 5) then
      return 53;
    end
    
    if (cloudiness <= 7) then
      return 56;
    end
    
    if (cloudiness <= 9) then
      return 59;
    end
  end
  
  if (wawa == 89) then 

    if (cloudiness <= 5) then
      return 61;
    end

    if (cloudiness <= 7) then
      return 64;
    end

    if (cloudiness <= 9) then
      return 67;
    end

  end
  
  return wawa;

end




function countSmartSymbolNumber(n,thunder,rain,fog,rform,rtype)

  if (n == ParamValueMissing) then
    return ParamValueMissing;
  end

  if (thundder ~= ParamValueMissing and thunder >= thunder_limit1) then    
    local nclass = 0;
    if (n < cloud_limit6) then
      nclass = 0;
    elseif (n < cloud_limit8) then
      nclass = 1;
    else
      nclass = 2;
    end

    return (71 + 3 * nclass);
  end

  -- No thunder (or not available). Then we always need precipitation rate

  if (rain == ParamValueMissing) then
    return ParamValueMissing;
  end

  if (rain < rain_limit1) then
    
    -- No precipitation. Now we need only fog/cloudiness

    if (fog > 0 and fog ~= ParamValueMissing) then 
      return  9;
    end

    -- no rain, no fog (or not available), only cloudiness
    if (n < cloud_limit2) then
      return 1;  -- clear
    elseif (n < cloud_limit3) then
      return  2;  -- mostly clear
    elseif (n < cloud_limit6) then
      return  4;  -- partly cloudy
    elseif (n < cloud_limit8) then
      return 6;  -- mostly cloudy
    else
      return 7;    -- overcast
    end

  end

  -- Since we have precipitation, we always need precipitation form
    
  if (rform == ParamValueMissing) then
    return ParamValueMissing;
  end

  if (rform == 0) then       -- drizzle
    return  11;   
  elseif (rform == 4) then  -- freezing drizzle
    return 14;
  elseif (rform == 5) then  -- freezing rain
    return 17;
  elseif (rform == 7 or rform == 8)  then  -- snow or ice particles
    return 57;                    -- convert to plain snowfall + cloudy
  end
  
  -- only water, sleet and snow left. Cloudiness limits
  -- are the same for them, precipitation limits are not.

  local nclass = 0;
  if (n < cloud_limit6) then
    nlcass = 0;
  elseif (n < cloud_limit8) then
    nclass = 1;
  else 
    nclass = 2;
  end

  if (rform == 6) then  -- hail
    return (61 + 3 * nclass);
  end
  
  if (rform == 1) then  -- water
    
    --  Now we need precipitation type too
    local rt = 1;
    if (rtype ~= ParamValueMissing) then    
      rt = math.floor(rtype + 0.5);
    end

    if (rt == 2) then                  --  convective
      return (21 + 3 * nclass);  -- 21, 24, 27 for showers
    end

    -- rtype=1:large scale precipitation (or rtype is missing)
    local rclass = 0;
    if (rain < rain_limit3) then
      rclass = 0;
    elseif (rain < rain_limit6) then
      rclass = 1;
    else 
      rclas = 2;
    end;
      
    return (31 + 3 * nclass + rclass);  -- 31-39 for precipitation
  end

  -- rform=2:sleet and rform=3:snow map to 41-49 and 51-59 respectively

  local rclass = 0;
  if (rain < rain_limit3) then
    rclass = 0;
  elseif (rain < rain_limit4) then
    rclass = 1;
  else
    rclass = 2;
  end;

  return (10 * rform + 21 + 3 * nclass + rclass);

end




function NB_SmartSymbolNumber2(numOfParams,params)

  print("SmartSymbolNumber");
  for index, value in pairs(params) do
    print(index.." : "..value);
  end

  local result = {};

  if (numOfParams ~= 4) then
    result.message = 'Invalid number of parameters!';
    result.value = 0;  
    return result.value,result.message;
  end
  
  local wawa = params[1];
  local cloudiness = params[2];
  local temperature = params[3];
  local dark = params[4];

  local smartsymbol = getSmartSymbol(wawa,cloudiness,temperature);
  
  -- print("SMART :"..smartsymbol);

  -- Add day/night information
  if (smartsymbol ~= ParamValueMissing) then
  
    if (dark == 0) then
      result.value = 100 + smartsymbol;
    else      
      result.value = smartsymbol;
    end
    
    result.message = "OK";
    result.value = 100 + smartsymbol;
      
    return result.value,result.message;
    
  end

  -- No valid combination found, return empty value
  
  result.message = "OK"
  result.value = ParamValueMissing;
  
  return result.value,result.message;
  
end




function NB_SmartSymbolNumber(numOfParams,params)

  local result = {};
  local n = params[1];           -- TotalCloudCover
  local thunder = params[2];     -- ProbabilityThunderstorm
  local rain = params[3];        -- Precipitation1h
  local fog = params[4];         -- FmiFogIntensity
  local rform = params[5];       -- PrecipitationForm
  local rtype = params[6];       -- PrecipitationType
  local dark = params[7];        -- Dark

  result.value = countSmartSymbolNumber(n,thunder,rain,fog,rform,rtype);

  result.message = "OK";
  if (dark > 0  and  result.value ~= ParamValueMissing) then
    result.value =result.value + 100; 
  end
  
  return result.value,result.message;

end



function NB_WeatherNumber(numOfParams,params)

  --print("NB_WeatherNumber");
  --for index, value in pairs(params) do
  --  print(index.." : "..value);
  --end

  local result = {};

  local cloudiness = params[1];  -- TotalCloudCover
  local rain = params[2];        -- Precipitation1h
  local rform = params[3];       -- PrecipitationForm
  local rtype = params[4];       -- PrecipitationType
  local thunder = params[5];     -- ProbabilityThunderstorm
  local fog = params[6];         -- FmiFogIntensity
    
  local n_class = 9;  
  if (cloudiness == ParamValueMissing) then
    n_class = 9;     
  elseif (cloudiness < cloud_limit1) then
    n_class = 0;
  elseif (cloudiness < cloud_limit2) then
    n_class = 1;
  elseif (cloudiness < cloud_limit3) then
    n_class = 2;
  elseif (cloudiness < cloud_limit4) then
    n_class = 3;
  elseif (cloudiness < cloud_limit5) then
    n_class = 4;
  elseif (cloudiness < cloud_limit6) then
    n_class = 5;
  elseif (cloudiness < cloud_limit7) then
    n_class = 6;
  elseif (cloudiness < cloud_limit8) then
    n_class = 7;
  else
    n_class = 8;
  end

  local rain_class = 9; 
  if (rain == ParamValueMissing) then
    rain_class = 9;
  elseif (rain < rain_limit1) then
    rain_class = 0;
  elseif (rain < rain_limit2) then
    rain_class = 1;
  elseif (rain < rain_limit3) then
    rain_class = 2;
  elseif (rain < rain_limit4) then
    rain_class = 3;
  elseif (rain < rain_limit5) then
    rain_class = 4;
  elseif (rain < rain_limit6) then
    rain_class = 5;
  elseif (rain < rain_limit7) then
    rain_class = 6;
  else
    rain_class = 7;
  end

  local rform_class = 9; 
  if (rform == ParamValueMissing) then
    rform_class = 9;
  else
    rform_class = math.floor(rform +0.5);
  end

  local rtype_class = 9; 
  if (rtype == ParamValueMissing) then
    rtype_class = 9;
  else
    rtype_class = math.floor(rtype + 0.5);
  end

  local thunder_class = 9;
  if (thunder == ParamValueMissing) then
    thunder_class = 9;
  elseif (thunder < thunder_limit1) then
    thunder_class = 0;
  elseif (thunder < thunder_limit2) then
    thunder_class = 1;
  else
    thunder_class = 2;
  end

  local fog_class = 9;
  if (fog == ParamValueMissing) then
    fog_class = 0;
  else 
    fog_class = math.floor(fog + 0.5);
  end

  -- Build the number
  local version = 1;
  local cloud_class = 0;  -- not available yet

  result.value =  (10000000 * version +
          1000000 * thunder_class +
          100000 * rform_class +
          10000 * rtype_class +
          1000 * rain_class +
          100 * fog_class +
          10 * n_class +
          cloud_class);

  result.message = 'OK';
  return result.value,result.message;

end







-- ***********************************************************************
--  FUNCTION : getFunctionNames
-- ***********************************************************************
--  The function returns the list of available functions in this file.
--  In this way the query server knows which function are available in
--  each LUA file.
-- 
--  Each LUA file should contain this function. The 'type' parameter
--  indicates how the current LUA function is implemented.
--
--    Type 1 : 
--      Function takes two parameters as input:
--        - numOfParams => defines how many values is in the params array
--        - params      => Array of float values
--      Function returns two parameters:
--        - result value (function result or ParamValueMissing)
--        - result string (=> 'OK' or an error message)
--  
--    Type 4 : 
--      Function takes five parameters as input:
--        - columns       => Number of the columns in the grid
--        - rows          => Number of the rows in the grid
--        - params1       => Grid 1 values (= Array of float values)
--        - params2       => Grid 2 values (= Array of float values)
--        - params3       => Grid point angles to latlon-north (= Array of float values)
--      Function returns one parameter:
--        - result array  => Array of float values (must have the same 
--                           number of values as the input 'params1'.
--      Can be use for example in order to calculate new Wind U- and V-
--      vectors when the input vectors point to grid-north instead of
--      latlon-north.               
--
--    Type 5 : 
--      Function takes three parameters as input:
--        - language    => defines the used language
--        - numOfParams => defines how many values is in the params array
--        - params      => Array of float values
--      Function returns two parameters:
--        - result value (string)
--        - result string (=> 'OK' or an error message)
--      Can be use for example for translating a numeric value to a string
--      by using the given language.  
--  
--    Type 6 : 
--      Function takes two parameters as input:
--        - numOfParams => defines how many values is in the params array
--        - params      => Array of string values
--      Function returns one parameters:
--        - result value (string)
--      This function takes an array of strings and returns a string. It
--      is used for example in order to get additional instructions for
--      complex interpolation operations.  
--
--    Type 9: Takes vector<float[len]> as input and returns vector<float> as output
--        - columns       => Number of the columns in the grid
--        - rows          => Number of the rows in the grid
--        - len           => Number of the values in the array
--        - params        => Grid values (vector<float[len]>)
--        - extParams     => Additional parameters (= Array of float values)
--      Function returns one parameter:
--        - result array  => Array of float values.               
--  
-- ***********************************************************************
 
function getFunctionNames(type)

  local functionNames = '';

  if (type == 1) then 
    functionNames = 'NB_SummerSimmerIndex,NB_FeelsLikeTemperature,NB_WindChill,NB_Snow1h,NB_Cloudiness8th,NB_SmartSymbolNumber,NB_WeatherNumber';
  end
  
  if (type == 5) then 
    functionNames = 'NB_WindCompass8,NB_WindCompass16,NB_WindCompass32,NB_WeatherText,NB_TemperatureText,NB_SmartSymbolText';
  end

  return functionNames;

end

