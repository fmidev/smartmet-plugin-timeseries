
ParamValueMissing = -16777216;
debug = 0;


local yesno = {};
yesno['en'] = {};
yesno['en'][1] = "No";
yesno['en'][2] = "Yes";

yesno['fi'] = {};
yesno['fi'][1] = "Ei";
yesno['fi'][2] = "Kyllä";

yesno['sv'] = {};
yesno['sv'][1] = "Nej";
yesno['sv'][2] = "Ja";

yesno['default'] = yesno['en'];


 
-- ***********************************************************************
--  FUNCTION : ABS
-- ***********************************************************************
--  The function returns the absolute value of the given parameter.
-- ***********************************************************************

function ABS(numOfParams,params)

  local result = {};

  if (numOfParams == 1) then
    result.message = 'OK';
    if (params[1] ~= ParamValueMissing) then
      if (params[1] >= 0) then
        result.value = params[1];
      else
        result.value = -params[1];
      end
    else
      result.value = ParamValueMissing;
    end
  else
    result.message = 'Invalid number of parameters given ('..numOfParams..')!';
    result.value = 0;  
  end
  
  return result.value,result.message;
  
end



-- ***********************************************************************
--  FUNCTION : STD
-- ***********************************************************************
--  The function returns the standard deviation value of the given parameters.
-- ***********************************************************************

function STD(numOfParams,params)

  local result = {};
  local count = 0;

  if (numOfParams > 0) then
    local sum = 0;
    local vm;
    local mean;

    mean = AVG(numOfParams, params)

    for index, value in pairs(params) do
      if value ~= ParamValueMissing then
        vm = value - mean;
        sum = sum + (vm * vm);
        count = count + 1;
      end
    end

    result.message = 'OK';
    result.value = math.sqrt(sum / count);

  else
    result.message = 'OK';
    result.value = ParamValueMissing;
  end

 return result.value,result.message;

end



-- ***********************************************************************
--  FUNCTION : AVG
-- ***********************************************************************
--  The function returns the average value of the given parameters.
-- ***********************************************************************

function AVG(numOfParams,params)

  local result = {};
  local count = 0;
  
  if (numOfParams > 0) then    
    local sum = 0;
    for index, value in pairs(params) do
      if (value ~= ParamValueMissing) then
	    sum = sum + value;
	    count = count + 1;
	  end
    end
    result.message = 'OK';
    result.value = sum / count;
  else
    result.message = 'OK';
    result.value = ParamValueMissing;
  end
    
  return result.value,result.message;

end





-- ***********************************************************************
--  FUNCTION : DIV
-- ***********************************************************************
--  The function divides the first parameter with the second parameter.
-- ***********************************************************************

function DIV(numOfParams,params)

  local result = {};
  
  if (numOfParams == 2) then    
    result.message = 'OK';
    if (params[1] ~= ParamValueMissing and params[2] ~= ParamValueMissing) then
      result.value = params[1] / params[2];
    else
      result.value = ParamValueMissing;  
    end
  else
    result.message = 'Invalid number of parameters given ('..numOfParams..')!';
    result.value = 0;  
  end
    
  return result.value,result.message;

end




-- ***********************************************************************
--  FUNCTION : MAX
-- ***********************************************************************
--  The function returns the maximum value of the given parameters.
-- ***********************************************************************

function MAX(numOfParams,params)

  local result = {};
  
  if (numOfParams > 0) then    
    local max = params[1];
    for index, value in pairs(params) do
      if (value > max and value ~= ParamValueMissing) then
        max = value;
      end
    end

    result.message = 'OK';
    result.value = max;
  else
    result.message = 'OK';
    result.value = ParamValueMissing;
  end
    
  return result.value,result.message;

end





-- ***********************************************************************
--  FUNCTION : MIN
-- ***********************************************************************
--  The function returns the minimum value of the given parameters.
-- ***********************************************************************

function MIN(numOfParams,params)

  local result = {};
  
  if (numOfParams > 0) then    
    local min = params[1];
    for index, value in pairs(params) do
      if (value < min and value ~= ParamValueMissing) then
        min = value;
      end
    end

    result.message = 'OK';
    result.value = min;
  else
    result.message = 'OK';
    result.value = ParamValueMissing;
  end
    
  return result.value,result.message;

end
 

 


-- ***********************************************************************
--  FUNCTION : MUL
-- ***********************************************************************
--  The function multiplies the first parameter with the second parameter.
-- ***********************************************************************

function MUL(numOfParams,params)

  local result = {};
  
  if (numOfParams == 2) then    
    result.message = 'OK';
    if (params[1] ~= ParamValueMissing and params[2] ~= ParamValueMissing) then
      result.value = params[1] * params[2];
    else
      result.value = ParamValueMissing;  
    end
  else
    result.message = 'OK';
    result.value = ParamValueMissing;
  end
    
  return result.value,result.message;

end





-- ***********************************************************************
--  FUNCTION : NEG
-- ***********************************************************************
--  The function returns the negative value of the given parameter. Notice
--  that if the given parameter is negative then the result is positive.
-- ***********************************************************************

function NEG(numOfParams,params)

  local result = {};

  if (numOfParams == 1) then
    result.message = 'OK';
    if (params[1] ~= ParamValueMissing) then    
      result.value = -params[1];
    else
      result.value = ParamValueMissing;
    end    
  else
    result.message = 'OK';
    result.value = ParamValueMissing;
  end
  
  return result.value,result.message;
  
end





-- ***********************************************************************
--  FUNCTION : SQRT
-- ***********************************************************************
--  The function returns the sqrt value of the given parameter.
-- ***********************************************************************

function SQRT(numOfParams,params)

  local result = {};

  if (numOfParams == 1) then
    result.message = 'OK';
    if (params[1] ~= ParamValueMissing) then    
      result.value = math.sqrt(params[1]);
    else
      result.value = ParamValueMissing;
    end    
  else
    result.message = 'OK';
    result.value = ParamValueMissing;
  end
  
  return result.value,result.message;
  
end





-- ***********************************************************************
--  FUNCTION : SUM
-- ***********************************************************************
--  The function returns the sum of the given parameters.
-- ***********************************************************************

function SUM(numOfParams,params)

  local result = {};
  
  for index, value in pairs(params) do
    print("SUM "..index.." : "..value);
  end

  if (numOfParams > 0) then   
    local sum = 0;
    for index, value in pairs(params) do
      if (value ~= ParamValueMissing) then
        sum = sum + value;
      else
        result.message = 'OK';
        result.value = ParamValueMissing;
        return result.value,result.message;
      end
    end
    result.message = 'OK';
    result.value = sum;
  else
    result.message = 'OK';
    result.value = ParamValueMissing;
  end
    
  return result.value,result.message;

end




-- ***********************************************************************
--  FUNCTION : IFSUM
-- ***********************************************************************
--  The function returns the sum of the given parameters if the first 
--  parameter is 1. Otherwise it returns the second parameter.
-- ***********************************************************************

function IFSUM(numOfParams,params)

  local result = {};
  
  -- for index, value in pairs(params) do
  --  print("IFSUM "..index.." : "..value);
  -- end

  if (numOfParams > 1) then   
    local sum = 0;
    
    if (params[1] == 1) then
      for index, value in pairs(params) do
        if (index > 1) then
          if (value ~= ParamValueMissing) then
            sum = sum + value;
          else
            result.message = 'OK';
            result.value = ParamValueMissing;
            return result.value,result.message;
          end
        end
      end
    else
      sum = params[2];
    end
    
    result.message = 'OK';
    result.value = sum;
  else
    result.message = 'No parameters given!';
    result.value = 0;  
  end
    
  return result.value,result.message;

end



-- ***********************************************************************
--  FUNCTION : DIFF
-- ***********************************************************************
--  The function returns the sum of the given parameters.
-- ***********************************************************************

function DIFF(numOfParams,params)

  local result = {};

  if (numOfParams == 2) then
    result.message = 'OK';
    if (params[1] ~= ParamValueMissing and params[2] ~= ParamValueMissing) then    
      result.value = params[1]-params[2];
    else
      result.value = ParamValueMissing;
    end    
  else
    result.message = 'Invalid number of parameters given ('..numOfParams..')!';
    result.value = 0;  
  end
  
  return result.value,result.message;

end




-- ***********************************************************************
--  FUNCTION : HYPOT
-- ***********************************************************************
--  The function returns the hypotenuse of the given parameters.
-- ***********************************************************************

function HYPOT(numOfParams,params)

  local result = {};

  if (numOfParams == 2) then
    result.message = 'OK';
    if (params[1] ~= ParamValueMissing and params[2] ~= ParamValueMissing) then    
      result.value = math.sqrt(params[1]*params[1] + params[2]*params[2]);
    else
      result.value = ParamValueMissing;
    end    
  else
    result.message = 'Invalid number of parameters given ('..numOfParams..')!';
    result.value = 0;  
  end
  
  return result.value,result.message;

end





-- ***********************************************************************
--  FUNCTION : ROUND
-- ***********************************************************************
--  The function returns the rounded integer value of the given parameter.
-- ***********************************************************************

function ROUND(numOfParams,params)

  local result = {};

  if (numOfParams == 1) then
    result.message = 'OK';
    if (params[1] ~= ParamValueMissing) then
        result.value = math.floor(params[1]+0.5);
    else
      result.value = ParamValueMissing;
    end
  else
    result.message = 'Invalid number of parameters given ('..numOfParams..')!';
    result.value = 0;  
  end
  
  return result.value,result.message;
  
end





-- ***********************************************************************
--  FUNCTION : FLOOR
-- ***********************************************************************
--  The function returns the floor alue of the given parameter.
-- ***********************************************************************

function FLOOR(numOfParams,params)

  local result = {};

  if (numOfParams == 1) then
    result.message = 'OK';
    if (params[1] ~= ParamValueMissing) then
        result.value = math.floor(params[1]);
    else
      result.value = ParamValueMissing;
    end
  else
    result.message = 'Invalid number of parameters given ('..numOfParams..')!';
    result.value = 0;  
  end
  
  return result.value,result.message;
  
end





-- ***********************************************************************
--  FUNCTION : ITEM
-- ***********************************************************************
--  The function returns the item of the given index.
-- ***********************************************************************

function ITEM(numOfParams,params)

  local result = {};
  
  if (numOfParams > 0) then    
    local idx = params[1] + 1;
    for index, value in pairs(params) do
      if (idx == index) then
        result.message = 'OK';
        result.value = value;
        return result.value,result.message;
      end
    end

  end
    
  result.message = 'OK';
  result.value = ParamValueMissing;
  return result.value,result.message;
  
end




-- ***********************************************************************
--  FUNCTION : LIST
-- ***********************************************************************
--  The function returns values in a string
-- ***********************************************************************

function LIST(language,numOfParams,params)
  
  local result = {};
  local str = "";
  
  -- for index, value in pairs(params) do
  --   print(index.." : "..value);
  -- end

  if (numOfParams > 0) then    
    for index, value in pairs(params) do
      --if (value ~= ParamValueMissing) then
        str = str..value;
      --else
      --  str = str.."None"
      --end
      if (index < numOfParams) then
        str = str..";"
      end
    end
  end
    
  result.message = 'OK';
  result.value = str;
  return result.value,result.message;

end




-- ***********************************************************************
--  FUNCTION : IF
-- ***********************************************************************
--  The if the first parameter is 1 then function returns the second 
--  parameter. Otherwise it returns the third parameter.
-- ***********************************************************************

function IF(numOfParams,params)

  local result = {};
  
  if (numOfParams == 3) then
    if (params[1] == 1) then
      result.value = params[2];
    else
      result.value = params[3];
    end
      
    result.message = 'OK';
    return result.value,result.message;
  else
    result.message = 'Invalid number of parameters!';
    result.value = 0;  
  end
    
  return result.value,result.message;

end




-- ***********************************************************************
--  FUNCTION : EQ
-- ***********************************************************************
--  The function returns 1 if the first parameter is equal with the seocnd 
--  parameter. Otherwise it returns 0.
-- ***********************************************************************

function EQ(numOfParams,params)

  local result = {};
  
  if (numOfParams == 2) then
    if (params[1] == params[2]) then
      result.value = 1;
    else
      result.value = 0;
    end
      
    result.message = 'OK';
    return result.value,result.message;
  else
    result.message = 'Invalid number of parameters!';
    result.value = 0;  
  end
    
  return result.value,result.message;

end



-- ***********************************************************************
--  FUNCTION : NEQ
-- ***********************************************************************
--  The function returns 1 if the first parameter is not equal with the seocnd 
--  parameter. Otherwise it returns 0.
-- ***********************************************************************

function EQ(numOfParams,params)

  local result = {};
  
  if (numOfParams == 2) then
    if (params[1] ~= params[2]) then
      result.value = 1;
    else
      result.value = 0;
    end
      
    result.message = 'OK';
    return result.value,result.message;
  else
    result.message = 'Invalid number of parameters!';
    result.value = 0;  
  end
    
  return result.value,result.message;

end




-- ***********************************************************************
--  FUNCTION : NOT
-- ***********************************************************************
--  The function returns 1 if the first parameter is 0. Otherwise it returns 0.
-- ***********************************************************************

function NOT(numOfParams,params)

  local result = {};
  
  if (numOfParams == 1) then
    if (params[1] == 0) then
      result.value = 1;
    else
      result.value = 0;
    end
      
    result.message = 'OK';
    return result.value,result.message;
  else
    result.message = 'Invalid number of parameters!';
    result.value = 0;  
  end
    
  return result.value,result.message;

end





-- ***********************************************************************
--  FUNCTION : GT
-- ***********************************************************************
--  The function returns 1 if the first parameter is greater than the seocnd 
--  parameter. Otherwise it returns 0.
-- ***********************************************************************

function GT(numOfParams,params)

  local result = {};
  
  if (numOfParams == 2) then
    if (params[1] > params[2]) then
      result.value = 1;
    else
      result.value = 0;
    end
      
    result.message = 'OK';
    return result.value,result.message;
  else
    result.message = 'Invalid number of parameters!';
    result.value = 0;  
  end
    
  return result.value,result.message;

end



-- ***********************************************************************
--  FUNCTION : GTE
-- ***********************************************************************
--  The function returns 1 if the first parameter is greater or equal than 
--  the seocnd parameter. Otherwise it returns 0.
-- ***********************************************************************

function GTE(numOfParams,params)

  local result = {};
  
  if (numOfParams == 2) then
    if (params[1] >= params[2]) then
      result.value = 1;
    else
      result.value = 0;
    end
      
    result.message = 'OK';
    return result.value,result.message;
  else
    result.message = 'Invalid number of parameters!';
    result.value = 0;  
  end
    
  return result.value,result.message;

end




-- ***********************************************************************
--  FUNCTION : LT
-- ***********************************************************************
--  The function returns 1 if the first parameter is less than the seocnd 
--  parameter. Otherwise it returns 0.
-- ***********************************************************************

function LT(numOfParams,params)

  local result = {};
  
  if (numOfParams == 2) then
    if (params[1] < params[2]) then
      result.value = 1;
    else
      result.value = 0;
    end
      
    result.message = 'OK';
    return result.value,result.message;
  else
    result.message = 'Invalid number of parameters!';
    result.value = 0;  
  end
    
  return result.value,result.message;

end




-- ***********************************************************************
--  FUNCTION : LTE
-- ***********************************************************************
--  The function returns 1 if the first parameter is less or equal than 
--  the seocnd parameter. Otherwise it returns 0.
-- ***********************************************************************

function LTE(numOfParams,params)

  local result = {};
  
  if (numOfParams == 2) then
    if (params[1] <= params[2]) then
      result.value = 1;
    else
      result.value = 0;
    end
      
    result.message = 'OK';
    return result.value,result.message;
  else
    result.message = 'Invalid number of parameters!';
    result.value = 0;  
  end
    
  return result.value,result.message;

end




-- ***********************************************************************
--  FUNCTION : AND
-- ***********************************************************************
--  The function returns 1 if all parameters are 1. Otherwise it returns 0.
-- ***********************************************************************

function AND(numOfParams,params)

  local result = {};

  if (numOfParams > 0) then   
    for index, value in pairs(params) do
      if (value ~= 1) then
        result.message = 'OK';
        result.value = 0;
        return result.value,result.message;        
      end
    end
    result.value = 1;
    result.message = 'OK';
    result.value = sum;
  else
    result.message = 'No parameters given!';
    result.value = 0;  
  end
    
  return result.value,result.message;

end




-- ***********************************************************************
--  FUNCTION : OR
-- ***********************************************************************
--  The function returns 1 if all parameters are 1. Otherwise it returns 0.
-- ***********************************************************************

function OR(numOfParams,params)

  local result = {};

  if (numOfParams > 0) then   
    for index, value in pairs(params) do
      if (value == 1) then
        result.message = 'OK';
        result.value = 1;
        return result.value,result.message;        
      end
    end
    result.value = 0;
    result.message = 'OK';
    result.value = sum;
  else
    result.message = 'No parameters given!';
    result.value = 0;  
  end
    
  return result.value,result.message;

end




-- ***********************************************************************
--  FUNCTION : IN
-- ***********************************************************************
--  The function returns 1 if the first parameter can be found from the
--  the rest of the parameters. Otherwise it returns 0.
-- ***********************************************************************

function IN(numOfParams,params)

  local result = {};

  if (numOfParams > 1) then   
    local s = params[1];
    for index, value in pairs(params) do
      if (index > 1  and  value == s) then
        result.message = 'OK';
        result.value = 1;
        return result.value,result.message;        
      end
    end
    result.value = 0;
    result.message = 'OK';
    result.value = sum;
  else
    result.message = 'Invalid number of parameters given!';
    result.value = 0;  
  end
    
  return result.value,result.message;

end




-- ***********************************************************************
--  FUNCTION : INSIDE
-- ***********************************************************************
--  The function returns 1 if the first parameter is inside the value range
--  defined by the second and the third parameter. Otherwise it returns 0.
-- ***********************************************************************

function INSIDE(numOfParams,params)

  local result = {};

  if (numOfParams == 3) then
    if (params[1] >= params[2] and  params[1] <= params[3]) then
      result.value = 1;
    else
      result.value = 0;
    end
      
    result.message = 'OK';
    return result.value,result.message;
  else
    result.message = 'Invalid number of parameters!';
    result.value = 0;  
  end
    
  return result.value,result.message;

end




-- ***********************************************************************
--  FUNCTION : OUTSIDE
-- ***********************************************************************
--  The function returns 1 if the first parameter is outside the value range
--  defined by the second and the third parameter. Otherwise it returns 0.
-- ***********************************************************************

function OUTSIDE(numOfParams,params)

  local result = {};

  if (numOfParams == 3) then
    if (params[1] < params[2] and  params[1] > params[3]) then
      result.value = 1;
    else
      result.value = 0;
    end
      
    result.message = 'OK';
    return result.value,result.message;
  else
    result.message = 'Invalid number of parameters!';
    result.value = 0;  
  end
    
  return result.value,result.message;

end




-- ***********************************************************************
--  FUNCTION : YESNO
-- ***********************************************************************

function YESNO(language,numOfParams,params)

  local result = {};

  if (numOfParams ~= 1) then
    result.message = 'Invalid number of parameters!';
    result.value = 0;  
    return result.value,result.message;
  end

  local idx = params[1] + 1;

  result.message = "OK"
  
  if (yesno[language] ~= nil  and  yesno[language][idx] ~= nil)  then 
    result.value = yesno[language][idx];
  else
    if (yesno['default'] ~= nil and yesno['default'][idx] ~= nil) then  
      result.value = yesno['default'][idx];
    end
  end

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
--  
-- ***********************************************************************


function getFunctionNames(type)

  local functionNames = '';

  if (type == 1) then 
    functionNames = 'ABS,STD,AVG,DIV,ITEM,MAX,MIN,MUL,NEG,SUM,DIFF,HYPOT,SQRT,IF,EQ,NEG,GT,GTE,LT,LTE,AND,OR,NOT,IN,INSIDE,OUTSIDE,IFSUM,ROUND,FLOOR';
  end

  if (type == 5) then 
    functionNames = 'LIST,YESNO';
  end
 
  
  
  return functionNames;

end

