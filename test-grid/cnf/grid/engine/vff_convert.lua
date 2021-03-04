
PI = 3.1415926;
ParamValueMissing = -16777216;
debug = 0;


function printTable(params)

  local result = {};

  for index, value in pairs(params) do
    if (value ~= ParamValueMissing) then
      print(index..':'..value);
    end
  end
end




-- ***********************************************************************
--  FUNCTION : SSI / SummerSimmerIndex
-- ***********************************************************************
-- ***********************************************************************

function SSI(columns,rows,len,params,extParams)

  local result = {};
  local simmer_limit = 14.5;

  if (len ~= 2) then
    return result;
  end
  
  local inIdx = 1;
  local outIdx = 1;
  
  for r=1,rows do
    for c=1,columns do       
      local rh = params[inIdx];
      local temp = params[inIdx+1];

      if (temp == ParamValueMissing or rh == ParamValueMissing) then
        result[outIdx] = ParamValueMissing;
      else
        temp = temp - 273.15;
        if (temp <= simmer_limit) then
          result[outIdx] = temp;
        else
          local rh_ref = 50.0 / 100.0;
          local r = rh / 100.0;

          result[outIdx] =  (1.8 * temp- 0.55 * (1 - r) * (1.8 * temp - 26) - 0.55 * (1 - rh_ref) * 26) / (1.8 * (1 - 0.55 * (1 - rh_ref)));
        end
      end
      
      inIdx = inIdx + 2;
      outIdx = outIdx + 1;
    end
  end
  
  return result;

end





-- ***********************************************************************
--  FUNCTION : SSI_COUNT / SummerSimmerIndex
-- ***********************************************************************
--  This is an internal function used by FEELS_LIKE function.
-- ***********************************************************************

function SSI_COUNT(rh,t)

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
--  FUNCTION : FEELS_LIKE
-- ***********************************************************************

function FEELS_LIKE(columns,rows,len,params,extParams)

  local result = {};

  if (len ~= 4) then
    return result;
  end
  
  local inIdx = 1;
  local outIdx = 1;
  
  for r=1,rows do
    for c=1,columns do       
      local temp = params[inIdx];
      local wind = params[inIdx+1];
      local rh = params[inIdx+2];
      local rad = params[inIdx+3];

      if (temp == ParamValueMissing or wind == ParamValueMissing or rh == ParamValueMissing) then
        result[outIdx] = ParamValueMissing;
      else
        temp = temp - 273.15;

        -- Calculate adjusted wind chill portion. Note that even though
        -- the Canadien formula uses km/h, we use m/s and have fitted
        -- the coefficients accordingly. Note that (a*w)^0.16 = c*w^16,
        -- i.e. just get another coefficient c for the wind reduced to 1.5 meters.

        local a = 15.0;   -- using this the two wind chills are good match at T=0
        local t0 = 37.0;  -- wind chill is horizontal at this T

        local chill = a + (1 - a / t0) * temp + a / t0 * math.pow(wind + 1, 0.16) * (temp - t0);

        -- Heat index

        local heat = SSI_COUNT(rh, temp);

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
    
        result[outIdx] = feels;

      end
      
      inIdx = inIdx + 4;
      outIdx = outIdx + 1;
    end
  end
  
  return result;
  
end




-- ***********************************************************************
--  FUNCTION : WIND_CHILL
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

function WIND_CHILL(columns,rows,len,params,extParams)

  local result = {};

  if (len ~= 2) then
    return result;
  end
  
  local inIdx = 1;
  local outIdx = 1;
  
  for r=1,rows do
    for c=1,columns do       
      local temp = params[inIdx];
      local wind = params[inIdx+1];

      if (temp == ParamValueMissing or wind == ParamValueMissing or wind < 0) then
        result[outIdx] = ParamValueMissing;
      else
        temp = temp - 273.15;
        local kmh = wind * 3.6;

        if (kmh < 5.0) then
          result[outIdx] = temp + (-1.59 + 0.1345 * temp) / 5.0 * kmh;
        else
          local wpow = math.pow(kmh, 0.16);
          result[outIdx] = 13.12 + 0.6215 * temp - 11.37 * wpow + 0.3965 * temp * wpow;
        end
      end
      
      inIdx = inIdx + 2;
      outIdx = outIdx + 1;
    end
  end
  
  return result;
end




-- ***********************************************************************
--  FUNCTION : C2F
-- ***********************************************************************
--  The function converts given Celcius degrees to Fahrenheit degrees.
-- ***********************************************************************

function C2F(columns,rows,len,params,extParams)

  local result = {};

  if (len ~= 1) then
    return result;
  end

  for index, value in pairs(params) do
    if (value ~= ParamValueMissing) then
      result[index] = value*1.8 + 32;
    else
      result[index] = ParamValueMissing;
    end
  end
  
  -- printTable(result);
  
  return result;
  
end






-- ***********************************************************************
--  FUNCTION : WIND_SPEED
-- ***********************************************************************
--  Counts the size of the hypotenuse assuming that params[0] and params[1]
--  represents vectors and the angle between them is 90 degrees.
-- ***********************************************************************

function WIND_SPEED(columns,rows,len,params,extParams)

  local result = {};
  local simmer_limit = 14.5;

  if (len ~= 2) then
    return result;
  end
  
  local inIdx = 1;
  local outIdx = 1;
  
  for r=1,rows do
    for c=1,columns do       
      local p1 = params[inIdx];
      local p2 = params[inIdx+1];

      if (p1 == ParamValueMissing or p2 == ParamValueMissing) then
        result[outIdx] = ParamValueMissing;
      else
        result[outIdx] = math.sqrt(p1*p1 + p2*p2);
      end
      
      inIdx = inIdx + 2;
      outIdx = outIdx + 1;
    end
  end
  
  return result;
  
end




-- ***********************************************************************
--  FUNCTION : WIND_DIR
-- ***********************************************************************
--  Counts the direction of the wind (wind blows FROM this direction, not
--  TO this direction). 
-- ***********************************************************************

function WIND_DIR(columns,rows,u,v,angles)

  local result = {};
  
  for index, value in pairs(v) do

    a = angles[index];
    b = math.atan(v[index]/u[index]);      
            
    if (u[index] >= 0  and  v[index] >= 0) then
      c = b;
    end
	      
    if (u[index] < 0  and  v[index] >= 0) then
      c = b + PI;
    end
	      
    if (u[index] < 0  and  v[index] < 0) then
      c = b + PI;
    end
	            
    if (u[index] >= 0  and  v[index] < 0) then
      c = b;      
    end
	                
    if (a < (PI/2)) then
      d = c - a
	else
      d = c - (PI-a);
	end   
	
	result[index] = 270-((d*180)/PI);
	
  end
   
  return result;
  
end



-- ***********************************************************************
--  FUNCTION : WIND_TO_DIR
-- ***********************************************************************
--  Counts the direction of the wind (wind blows TO this direction, not
--  FROM this direction). 

-- ***********************************************************************

function WIND_TO_DIR(columns,rows,u,v,angles)

  local result = {};
  
  for index, value in pairs(v) do

    a = angles[index];
    b = math.atan(v[index]/u[index]);      
            
    if (u[index] >= 0  and  v[index] >= 0) then
      c = b;
    end
	      
    if (u[index] < 0  and  v[index] >= 0) then
      c = b + PI;
    end
	      
    if (u[index] < 0  and  v[index] < 0) then
      c = b + PI;
    end
	            
    if (u[index] >= 0  and  v[index] < 0) then
      c = b;      
    end
	                
    if (a < (PI/2)) then
      d = c - a
	else
      d = c - (PI-a);
	end   
	
	result[index] = ((d*180)/PI);
	
  end
   
  return result;
  
end



-- ***********************************************************************
--  FUNCTION : WIND_V
-- ***********************************************************************
-- ***********************************************************************

function WIND_V(columns,rows,u,v,angles)

  local result = {};
  
  for index, value in pairs(v) do

    a = angles[index];
    b = math.atan(v[index]/u[index]);      
    hh = math.sqrt(u[index]*u[index] + v[index]*v[index]);     
            
    if (u[index] >= 0  and  v[index] >= 0) then
      c = b;
    end
	      
    if (u[index] < 0  and  v[index] >= 0) then
      c = b + PI;
    end
	      
    if (u[index] < 0  and  v[index] < 0) then
      c = b + PI;
    end
	            
    if (u[index] >= 0  and  v[index] < 0) then
      c = b;      
    end
	                
    if (a < (PI/2)) then
      d = c - a
	else
      d = c - (PI-a);
	end   
	
  val = hh * math.sin(d);         
	result[index] = val;
	
  end
    
  return result;
  
end




-- ***********************************************************************
--  FUNCTION : WIND_U
-- ***********************************************************************

function WIND_U(columns,rows,u,v,angles)

  local result = {};
  
  for index, value in pairs(v) do

    a = angles[index];
    b = math.atan(v[index]/u[index]);      
    hh = math.sqrt(u[index]*u[index] + v[index]*v[index]);     
            
    if (u[index] >= 0  and  v[index] >= 0) then
      c = b;
    end
	      
    if (u[index] < 0  and  v[index] >= 0) then
      c = b + PI;
    end
	      
    if (u[index] < 0  and  v[index] < 0) then
      c = b + PI;
    end
	            
    if (u[index] >= 0  and  v[index] < 0) then
      c = b;      
    end
	                
    if (a < (PI/2)) then
      d = c - a
	else
      d = c - (PI-a);
	end   
	
    val = hh * math.cos(d);         
	result[index] = val;
	
  end
    
  return result;
  
end





-- ***********************************************************************
--  FUNCTION : C2K
-- ***********************************************************************
--  The function converts given Celcius degrees to Kelvin degrees.
-- ***********************************************************************

function C2K(columns,rows,len,params,extParams)

  local result = {};

  if (len ~= 1) then
    return result;
  end


  for index, value in pairs(params) do
    if (value ~= ParamValueMissing) then
      -- print(index..':'..value);
      result[index] = value + 273.15;
    else
      result[index] = ParamValueMissing;
    end
  end  
  
  return result;
  
end





-- ***********************************************************************
--  FUNCTION : F2C
-- ***********************************************************************
--  The function converts given Fahrenheit degrees to Celcius degrees.
-- ***********************************************************************

function F2C(columns,rows,len,params,extParams)

  local result = {};

  if (len ~= 1) then
    return result;
  end

  for index, value in pairs(params) do
    if (value ~= ParamValueMissing) then
      -- print(index..':'..value);
      result[index] = 5*(value - 32)/9;
    else
      result[index] = ParamValueMissing;
    end    
  end
  
  return result;
  
end




-- ***********************************************************************
--  FUNCTION : F2K
-- ***********************************************************************
--  The function converts given Fahrenheit degrees to Kelvin degrees.
-- ***********************************************************************

function F2K(columns,rows,len,params,extParams)

  local result = {};

  if (len ~= 1) then
    return result;
  end

  for index, value in pairs(params) do
    if (value ~= ParamValueMissing) then
      -- print(index..':'..value);
      result[index] = 5*(value - 32)/9 + 273.15;
    else
      result[index] = ParamValueMissing;
    end    
  end
  
  return result;
  
end




-- ***********************************************************************
--  FUNCTION : K2C
-- ***********************************************************************
--  The function converts given Kelvin degrees to Celcius degrees.
-- ***********************************************************************

function K2C(columns,rows,len,params,extParams)

  print("K2C");
  local result = {};

  if (len ~= 1) then
    return result;
  end

  for index, value in pairs(params) do
    if (value ~= ParamValueMissing) then
      result[index] = value - 273.15;
    else
      result[index] = ParamValueMissing;
    end
  end
  
  return result;
  
end





-- ***********************************************************************
--  FUNCTION : K2F
-- ***********************************************************************
--  The function converts given Kelvin degrees to Fahrenheit degrees.
-- ***********************************************************************

function K2F(columns,rows,len,params,extParams)

  local result = {};

  if (len ~= 1) then
    return result;
  end

  for index, value in pairs(params) do
    if (value ~= ParamValueMissing) then
      -- print(index..':'..value);
      result[index] = (value - 273.15)*1.8 + 32;
    else
      result[index] = ParamValueMissing;
    end
  end
  
  return result;
  
end




-- ***********************************************************************
--  FUNCTION : DEG2RAD
-- ***********************************************************************
--  The function converts given degrees to radians.
-- ***********************************************************************

function DEG2RAD(columns,rows,len,params,extParams)

  local result = {};

  if (len ~= 1) then
    return result;
  end

  for index, value in pairs(params) do
    if (value ~= ParamValueMissing) then
      -- print(index..':'..value);
      result[index] = 2*PI*value/360;
    else
      result[index] = ParamValueMissing;
    end
  end
  
  return result;
  
end




-- ***********************************************************************
--  FUNCTION : RAD2DEG
-- ***********************************************************************
--  The function converts given radians to degrees.
-- ***********************************************************************

function RAD2DEG(columns,rows,len,params,extParams)

  local result = {};

  if (len ~= 1) then
    return result;
  end

  for index, value in pairs(params) do
    if (value ~= ParamValueMissing) then
      -- print(index..':'..value);
      result[index] = 360*value/(2*PI);
    else
      result[index] = ParamValueMissing;
    end
  end
  
  return result;
  
end




-- ***********************************************************************
--  FUNCTION : IN_PRCNT
-- ***********************************************************************
--  Notice that there is also C++ implementation of this function and it
--  is used when generating virtual files. 
-- ***********************************************************************

function IN_PRCNT(columns,rows,len,params,extParams)

  print("IN_PRCNT");

  local result = {};
  local inIdx = 1;
  local outIdx = 1;
  local min = extParams[1];
  local max = extParams[2];
  
  for r=1,rows do
    for c=1,columns do    
      local agree = 0;
      local cnt = 0;
      for i=1,len do
        value = params[inIdx];      
        if (value ~= ParamValueMissing) then
          cnt = cnt + 1;
          if (value >= min and value <= max) then 
            agree = agree + 1;
          end
        end
        inIdx = inIdx + 1;        
      end
      if (cnt > 0) then
        result[outIdx] = agree / len;
      else
        result[outIdx] = ParamValueMissing;
      end
      outIdx = outIdx + 1;
    end
  end
  
  return result;
  
end





-- ***********************************************************************
--  FUNCTION : OUT_PRCNT
-- ***********************************************************************
--  Notice that there is also C++ implementation of this function and it
--  is used when generating virtual files. 
-- ***********************************************************************

function OUT_PRCNT(columns,rows,len,params,extParams)

  local result = {};
  local inIdx = 1;
  local outIdx = 1;
  local min = extParams[1];
  local max = extParams[2];
  
  for r=1,rows do
    for c=1,columns do    
      local agree = 0;
      local cnt = 0;
      for i=1,len do
        value = params[inIdx];      
        if (value ~= ParamValueMissing) then
          cnt = cnt + 1;
          if (value < min or value > max) then 
            agree = agree + 1;
          end
        end
        inIdx = inIdx + 1;        
      end
      if (cnt > 0) then
        result[outIdx] = agree / len;
      else
        result[outIdx] = ParamValueMissing;
      end
      outIdx = outIdx + 1;
    end
  end
  
  return result;
  
end






-- ***********************************************************************
--  FUNCTION : MIN
-- ***********************************************************************
--  Notice that there is also C++ implementation of this function and it
--  is used when generating virtual files. 
-- ***********************************************************************

function MIN(columns,rows,len,params,extParams)

  print("MIN");
  local result = {};
  local inIdx = 1;
  local outIdx = 1;
    
  for r=1,rows do
    for c=1,columns do   
      local min = ParamValueMissing;      
      for i=1,len do
        value = params[inIdx];      
        if (value ~= ParamValueMissing and (min == ParamValueMissing or value < min)) then
          min = value;
        end
        inIdx = inIdx + 1;        
      end
      result[outIdx] = min;
      outIdx = outIdx + 1;
    end
  end
  
  return result;
  
end



-- ***********************************************************************
--  FUNCTION : MAX
-- ***********************************************************************
--  Notice that there is also C++ implementation of this function and it
--  is used when generating virtual files. 
-- ***********************************************************************

function MAX(columns,rows,len,params,extParams)

  print("MAX");
  local result = {};
  local inIdx = 1;
  local outIdx = 1;
    
  for r=1,rows do
    for c=1,columns do
      local max = ParamValueMissing;
      for i=1,len do
        value = params[inIdx];      
        if (value ~= ParamValueMissing and (max == ParamValueMissing or value > max)) then
          max = value;
        end
        inIdx = inIdx + 1;        
      end
      result[outIdx] = max;
      outIdx = outIdx + 1;
    end
  end
  
  return result;
  
end


-- ***********************************************************************
--  FUNCTION : FRACTILE
-- ***********************************************************************
-- Calculate fractile values from ensemble
-- Algorithm matches that in Himan:
-- https://github.com/fmidev/himan/blob/master/doc/plugin-fractile.md
-- ***********************************************************************

function FRACTILE(columns,rows,len,params,extParams)
  local result = {}
  local inIdx = 1
  local outIdx = 1
  local fractile = extParams[1]

  for r=1,rows do
    for c=1,columns do
      local list = {}
      for i=1,len do
        value = params[inIdx];
        if (value ~= ParamValueMissing) then
          list[#list+1] = value
        end
        inIdx = inIdx + 1
      end

      local N = #list

      if (N == 0) then
        result[outIdx] = ParamValueMissing
      else
        table.sort(list)
        list[#list+1] = 0.0

        local x
        if (fractile / 100.0 <= 1.0 / (N + 1)) then
          x = 1.0
        elseif (fractile / 100.0 >= (N) / (N+1)) then
          x = N
        else
          x = fractile / 100.0 * (N + 1)
        end
        x = math.floor(x) + 1

        result[outIdx] = list[x - 1] + math.fmod(x, 1.0) * (list[x] - list[x-1])
      end
      outIdx = outIdx + 1
    end
  end

  return result

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

  if (type == 4) then 
    functionNames = 'WIND_V,WIND_U,WIND_DIR,WIND_TO_DIR';
  end 
  if (type == 9) then 
    functionNames = 'FEELS_LIKE,SSI,WIND_CHILL,WIND_SPEED,C2F,C2K,F2C,F2K,K2C,K2F,DEG2RAD,RAD2DEG,IN_PRCNT,OUT_PRCNT,GT_PRCNT,LT,_PRCNT,MIN,MAX,FRACTILE';
  end

  return functionNames;

end

