ParamValueMissing = -16777216;
PI = 3.1415926;
DEBUG = 1;


-- ***********************************************************************
--  AREA INTERPOLATION METHODS
-- ***********************************************************************

AreaInterpolationMethod = {};

AreaInterpolationMethod.None             = 0;
AreaInterpolationMethod.Linear           = 1;
AreaInterpolationMethod.Nearest          = 2;
AreaInterpolationMethod.Min              = 3;
AreaInterpolationMethod.Max              = 4;
AreaInterpolationMethod.List             = 500;

AreaInterpolationMethod.ExtNone          = 1000;
AreaInterpolationMethod.ExtLinear        = 1001;
AreaInterpolationMethod.ExtNearest       = 1002;

AreaInterpolationMethod.ExtLandscape     = 1100;

AreaInterpolationMethod.ExtWindDirection = 1200;
AreaInterpolationMethod.ExtWindVector    = 1210;

AreaInterpolationMethod.ExtEnsemble      = 1300;



function round(num)
  under = math.floor(num)
  upper = math.floor(num) + 1
  underV = -(under - num)
  upperV = upper - num
  if (upperV > underV) then
      return under
  else
      return upper
  end
end



-- ***********************************************************************
--  FUNCTION : interpolate_none
-- ***********************************************************************
--  The function returns the interpolate value.
-- ***********************************************************************

function interpolate_none(x,y,val_bl,val_br,val_tr,val_tl)

  if (DEBUG == 1) then
    print("interpolate_none("..x..","..y..","..val_bl..","..val_br..","..val_tr..","..val_tl..")");
  end

  return val_bl;

end





-- ***********************************************************************
--  FUNCTION : interpolate_nearest
-- ***********************************************************************
--  The function returns the interpolate value.
-- ***********************************************************************

function interpolate_nearest(x,y,val_bl,val_br,val_tr,val_tl)

  if (DEBUG == 1) then
    print("interpolate_nearest("..x..","..y..","..val_bl..","..val_br..","..val_tr..","..val_tl..")");
  end

  local dist_x1 = x;
  local dist_y1 = y;
  local dist_x2 = 1 - dist_x1;
  local dist_y2 = 1 - dist_y1;        

  if (dist_x1 == 0  and  dist_y1 == 0) then
    return val_bl;
  end

  local dist_bl = (dist_x1)*(dist_x1) + (dist_y1)*(dist_y1);
  local dist_br = (dist_x2)*(dist_x2) + (dist_y1)*(dist_y1);
  local dist_tl = (dist_x1)*(dist_x1) + (dist_y2)*(dist_y2);
  local dist_tr = (dist_x2)*(dist_x2) + (dist_y2)*(dist_y2);

  if (dist_bl < dist_br  and  dist_bl <= dist_tl and dist_bl <= dist_tr) then
    return val_bl;
  end

  if (dist_br < dist_bl  and  dist_br <= dist_tl and dist_br <= dist_tr) then
    return val_br;
  end

  if (dist_tl < dist_bl  and  dist_tl <= dist_br  and  dist_tl <= dist_tr) then
    return val_tl;
  end

  return val_tr;

end





-- ***********************************************************************
--  FUNCTION : interpolate_linear
-- ***********************************************************************
--  The function returns the interpolate value.
-- ***********************************************************************

function interpolate_linear(x,y,val_bl,val_br,val_tr,val_tl)

  if (DEBUG == 1) then
    print("interpolate_linear("..x..","..y..","..val_bl..","..val_br..","..val_tr..","..val_tl..")");
  end

  local dist_x1 = x;
  local dist_y1 = y;
  local dist_x2 = 1- dist_x1;
  local dist_y2 = 1- dist_y1;        

  -- If the given point is close enough the actual grid point
  -- then there is no need for interpolation.

  local closeDist = 0.1;

  if (dist_x1 <= closeDist  and  dist_y1 <= closeDist) then
    return val_bl;
  end

  if (dist_x1 <= closeDist  and  dist_y2 <= closeDist) then
    return val_tl;
  end

  if (dist_x2 <= closeDist and dist_y1 <= closeDist) then
    return val_br;
  end

  if (dist_x2 <= closeDist  and  dist_y2 <= closeDist) then
    return val_tr;
  end

  -- If the given point is on the border then we can do simple
  -- linear interpolation.

  if (dist_x1 == 0) then
      
    --  Linear interpolation x1,y1 - x1,y2
    if (val_bl ~= ParamValueMissing  and  val_tl ~= ParamValueMissing) then
      return (dist_y1*val_bl + dist_y2*val_tl);  
    end

    return ParamValueMissing;  
  end

  if (dist_x2 == 0) then
    -- Linear interpolation x2,y1 - x2,y2
    if (val_br ~= ParamValueMissing  and  val_tr ~= ParamValueMissing) then
      return (dist_y1*val_br + dist_y2*val_tr);  
    end

    return ParamValueMissing;  
  end

  if (dist_y1 == 0) then
    -- Linear interpolation x1,y1 - x2,y1
    if (val_bl ~= ParamValueMissing  and  val_br ~= ParamValueMissing) then
      return (dist_x1*val_bl + dist_x2*val_br);
    end

    return ParamValueMissing;  
  end

  if (dist_y2 == 0) then
    -- Linear interpolation x1,y2 - x2,y2
    if (val_tl ~= ParamValueMissing  and  val_tr ~= ParamValueMissing) then
      return (dist_x1*val_tl + dist_x2*val_tr);
    end

    return ParamValueMissing;  
  end

  -- Bilinear interpolation

  if (val_bl ~= ParamValueMissing and val_br ~= ParamValueMissing  and val_tl ~= ParamValueMissing and  val_tr ~= ParamValueMissing) then
    -- All corners have a value.

    local fy1 = dist_x2*val_bl + dist_x1*val_br;
    local fy2 = dist_x2*val_tl + dist_x1*val_tr;
    local f = dist_y2*fy1 + dist_y1*fy2;
    return f;
  end

  -- Three corners have a value (triangular interpolation).

  if (val_bl == ParamValueMissing and val_br ~= ParamValueMissing  and  val_tl ~= ParamValueMissing and  val_tr ~= ParamValueMissing) then   
    local wsum = (dist_x2 * dist_y1 + dist_x1 * dist_y1 + dist_x1 * dist_y2);
    local f =  ((dist_x1 * dist_y2 * val_br + dist_x2 * dist_y1 * val_tl + dist_x1 * dist_y1 * val_tr) / wsum);
    return f;          
  end

  if (val_bl ~= ParamValueMissing and val_br == ParamValueMissing  and val_tl ~= ParamValueMissing and  val_tr ~= ParamValueMissing) then    
    local wsum = (dist_x2 * dist_y2 + dist_x2 * dist_y1 + dist_x1 * dist_y1);
    local f = ((dist_x2 * dist_y2 * val_bl + dist_x2 * dist_y1 * val_tl + dist_x1 * dist_y1 * val_tr) / wsum);
    return f;          
  end

  if (val_bl ~= ParamValueMissing and val_br ~= ParamValueMissing  and val_tl == ParamValueMissing and  val_tr ~= ParamValueMissing) then
    local wsum = (dist_x1 * dist_y1 + dist_x2 * dist_y2 + dist_x1 * dist_y2);
    local f = ((dist_x2 * dist_y2 * val_bl + dist_x1 * dist_y2 * val_br + dist_x1 * dist_y1 * val_tr) / wsum);
    return f;          
  end

  if (val_bl ~= ParamValueMissing and val_br ~= ParamValueMissing  and val_tl ~= ParamValueMissing and  val_tr == ParamValueMissing) then
    local wsum = (dist_x2 * dist_y1 + dist_x2 * dist_y2 + dist_x1 * dist_y2);
    local f = ((dist_x2 * dist_y2 * val_bl + dist_x1 * dist_y2 * val_br + dist_x2 * dist_y2 * val_tl) / wsum);
    return f;          
  end

  return ParamValueMissing;
  
end




function interpolate_linear_1D(factor,left,right)

  if (left == ParamValueMissing) then
    if (factor == 1) then
      return right;
    else
      return ParamValueMissing;
    end   
  end
  
  if (right == ParamValueMissing) then 
    if (theFactor == 0) then
      return left;
    else
      return ParamValueMissing;   
    end
  end
  
  return (1 - factor) * left + factor * right;
end





-- ***********************************************************************
--  FUNCTION : interpolate_windVector
-- ***********************************************************************
--  The function returns the interpolate value.
-- ***********************************************************************

function interpolate_windVector(x,y,val_bl,val_br,val_tr,val_tl)

  if (DEBUG == 1) then
    print("interpolate_windVector("..x..","..y..","..val_bl..","..val_br..","..val_tr..","..val_tl..")");
  end

  local dx = x;
  local dy = y;
  
  local windDirection = {};
  local windSpeed = {};
  local weight = {};

  if (val_tl ~= ParamValueMissing  and  val_tr ~= ParamValueMissing and val_bl ~= ParamValueMissing  and  val_br ~= ParamValueMissing) then

    -- bottomLeft
    windDirection[1] = (val_bl % 100) * 10;
    windSpeed[1] = (val_bl / 100);
    weight[1] = (1 - dx) * (1 - dy);

    -- bottomRight
    windDirection[2] = (val_br % 100) * 10;
    windSpeed[2] = (val_br / 100);
    weight[2] = dx *(1 - dy);

    -- topLeft
    windDirection[3] = (val_tl % 100) * 10;
    windSpeed[3] = (val_tl / 100);
    weight[3] = (1 - dx) * dy;

    -- topRight
    windDirection[4] = (val_tr % 100) * 10;
    windSpeed[4] = (val_tr / 100);
    weight[4] = dx *dy;

    local speedSum = 0;
    local speedSumX = 0;
    local speedSumY = 0;
    local weightSum = 0;
    local bestWeight = 0;
    local bestDirection = 0;
    
    for i=1,4 do

      if (windSpeed[i] ~= ParamValueMissing and windDirection[i] ~= ParamValueMissing and weight[i] > 0) then

        if (i == 1 or weight[i] > bestWeight) then          
          bestDirection = windDirection[i];          
          bestWeight = weight[i];
        end

        weightSum = weightSum + weight[i];
        speedSum = speedSum + weight[i] * windSpeed[i];

        -- Note that wind direction is measured against the Y-axis,
        -- not the X-axis. Hence sin and cos are not in the usual order.

        local dir = winDirection[i] / 2*PI;
  
        speedSumX = speedSumX + weight[i] * math.cos(dir);
        speedSumY = speedSumY + weight[i] * math.sin(dir);
      end;
    end
    
    -- Direction
    local wdInterp = 0;   
    local xx = speedSumX / weightSum;
    local yy = speedSumY / weightSum;

    -- If there is almost exact cancellation, return best weighted direction instead.
    if (math.sqrt(xx * xx + yy * yy) < 0.01) then    
      wdInterp = bestDirection;
    end

    -- Otherwise use the 2D mean
    local d = math.atan2(yy, xx) * 2*PI;  
    if (d < 0) then
      d = d + 360;
    end
      
    wdInterp = d;
    
    -- Speed   
    local wsInterp = speedSum / weightSum;
    
    if (wdInterp ~= ParamValueMissing and wsInterp ~= ParamValueMissing) then
      return round(wsInterp) * 100 + round(wdInterp / 10);
    else
      return ParamValueMissing;
    end
  
  end

  -- Grid cell edges

  if (dy == 0) then
    return interpolate_linear_1D(dx, val_bl, val_br);
  end
  
  if (dy == 1) then
    return interpolate_linear_1D(dx, val_tl, val_tr);
  end
  
  if (dx == 0) then
    return interpolate_linear_1D(dy, val_bl, val_tl);
  end
  
  if (dx == 1) then
    return interpolate_linear_1D(dy, val_br, val_tr);
  end

  return ParamValueMissing;

end





-- ***********************************************************************
--  FUNCTION : interpolate_windVector2
-- ***********************************************************************
--  The function returns the interpolate value.
-- ***********************************************************************

function interpolate_windVector2(x,y,wd_bl,wd_br,wd_tr,wd_tl,ws_bl,ws_br,ws_tr,ws_tl)

  if (DEBUG == 1) then
    print("interpolate_windVector2("..x..","..y..","..wd_bl..","..wd_br..","..wd_tr..","..wd_tl..","..ws_bl..","..ws_br..","..ws_tr..","..ws_tl..")");
  end

  local dx = x;
  local dy = y;
  
  local windDirection = {};
  local windSpeed = {};
  local weight = {};

  if (val_tl ~= ParamValueMissing  and  val_tr ~= ParamValueMissing and val_bl ~= ParamValueMissing  and  val_br ~= ParamValueMissing) then

    -- bottomLeft
    windDirection[1] = wd_bl;
    windSpeed[1] = ws_bl
    weight[1] = (1 - dx) * (1 - dy);

    -- bottomRight
    windDirection[2] = wd_br;
    windSpeed[2] = ws_br;
    weight[2] = dx *(1 - dy);

    -- topLeft
    windDirection[3] = wd_tl;
    windSpeed[3] = ws_tl;
    weight[3] = (1 - dx) * dy;

    -- topRight
    windDirection[4] = wd_tr;
    windSpeed[4] = ws_tr;
    weight[4] = dx *dy;

    local speedSum = 0;
    local speedSumX = 0;
    local speedSumY = 0;
    local weightSum = 0;
    local bestWeight = 0;
    local bestDirection = 0;
    
    for i=1,4 do

      if (windSpeed[i] ~= ParamValueMissing and windDirection[i] ~= ParamValueMissing and weight[i] > 0) then

        if (i == 1 or weight[i] > bestWeight) then          
          bestDirection = windDirection[i];          
          bestWeight = weight[i];
        end

        weightSum = weightSum + weight[i];
        speedSum = speedSum + weight[i] * windSpeed[i];

        -- Note that wind direction is measured against the Y-axis,
        -- not the X-axis. Hence sin and cos are not in the usual order.

        local dir = 2 * PI * windDirection[i] / 360 ;
        -- print("DIR="..dir.." WEIGHT="..weight[i].." SPEED="..windSpeed[i].." WINDIR="..windDirection[i]);
  
        speedSumX = speedSumX + weight[i] * math.cos(dir);
        speedSumY = speedSumY + weight[i] * math.sin(dir);
     
      end
    
    end
    
    -- Direction
    local wdInterp = 0;   
    local xx = speedSumX / weightSum;
    local yy = speedSumY / weightSum;
    
    -- print("DIRECTION "..xx.." "..yy.." "..speedSumX.." " ..weightSum);

    -- If there is almost exact cancellation, return best weighted direction instead.
    if (math.sqrt(xx * xx + yy * yy) < 0.01) then
      wdInterp = bestDirection;
    else

      -- Otherwise use the 2D mean
      local d = 180 * math.atan2(yy, xx) / PI;
      
      --local d = math.atan2(yy, xx) / 2*PI * 360;  
      if (d < 0) then
        d = d + 360;
      end
      wdInterp = d;
    end
      
    
    -- Speed   
    local wsInterp = speedSum / weightSum;

    --print("INTP "..wsInterp.." "..wdInterp);
    
    if (wdInterp ~= ParamValueMissing and wsInterp ~= ParamValueMissing) then
      return (wsInterp * 100) + (wdInterp / 10);
    else
      return ParamValueMissing;
    end
  
  end

  -- Grid cell edges

  if (dy == 0) then
    return interpolate_linear_1D(dx, ws_bl * 100 + (wd_bl/10), ws_br * 100 + (wd_br/10));
  end
  
  if (dy == 1) then
    return interpolate_linear_1D(dx, ws_tl * 100 + (wd_tl/10), ws_tr * 100 + (wd_tr/10));
  end
  
  if (dx == 0) then
    return interpolate_linear_1D(dy, ws_bl * 100 + (wd_bl/10), ws_tl * 100 + (wd_tl/10));
  end
  
  if (dx == 1) then
    return interpolate_linear_1D(dy, ws_br * 100 + (wd_br/10), ws_tr * 100 + (wd_tr/10));
  end

  return ParamValueMissing;

end





-- ***********************************************************************
--  FUNCTION : interpolate_windDirection
-- ***********************************************************************
--  The function returns the interpolate value.
-- ***********************************************************************

function interpolate_windDirection(x,y,val_bl,val_br,val_tr,val_tl)

  if (DEBUG == 1) then
    print("interpolate_windDirection("..x..","..y..","..val_bl..","..val_br..","..val_tr..","..val_tl..")");
  end

  return interpolate_nearest(x,y,val_bl,val_br,val_tr,val_tl);  

end




-- ***********************************************************************
--  FUNCTION : interpolate_windDirectionWithSpeedX
-- ***********************************************************************
--  The function returns the interpolate value.
-- ***********************************************************************

function interpolate_windDirectionWithSpeedX(windDir,windSpeed,weight)

  local count = 0;
  local bestWeight = 0;
  local weightSum = 0;    
  local speedSum = 0;
  local speedSumX = 0;
  local speedSumY = 0;
   
  for i, wd in pairs(windDir) do
    
    -- print(windDir[i].." "..windSpeed[i].." "..weight[i]);
    
    if (count == 0 or weight[i] > bestWeight) then
      bestDirection = windDir[i];
      bestWeight = weight[i];
    end
      
    weightSum = weightSum + weight[i];
    speedSum = speedSum + (weight[i] * windSpeed[i]); 

    local dir = windDir[i]*PI / 180;
    speedSumX = speedSumX + (weight[i] * math.cos(dir));
    speedSumY = speedSumY + (weight[i] * math.sin(dir));
      
    count = count + 1;

  end


  if (count == 0 or weightSum == 0) then  
    return ParamValueMissing;
  end


  local x = speedSumX / weightSum;
  local y = speedSumY / weightSum;

  -- print("XY "..x.." "..y.." BEST "..bestDirection.." "..bestWeight.." SUM "..speedSumX.." "..speedSumY.." WSUM"..weightSum);
  -- If there is almost exact cancellation, return best
  -- weighted direction instead.

  if (math.sqrt(x * x + y * y) < 0.01) then   
    return bestDirection;
  end

  -- Otherwise use the 2D mean

  local dir = 180 * math.atan2(y, x) / PI;
  if (dir < 0) then
    dir = dir + 360;
  end
  
  return dir;

end




-- ***********************************************************************
--  FUNCTION : interpolate_windDirectionWithSpeed
-- ***********************************************************************
--  The function returns the interpolate value.
-- ***********************************************************************

function interpolate_windDirectionWithSpeed(x,y,wd_bl,wd_br,wd_tr,wd_tl,ws_bl,ws_br,ws_tr,ws_tl)
 
  if (DEBUG == 1) then
    print("interpolate_windDirectionWithSpeed("..x..","..y..","..wd_bl..","..wd_br..","..wd_tr..","..wd_tl..","..ws_bl..","..ws_br..","..ws_tr..","..ws_tl..")");
  end

  local windDir = {};
  windDir[1] =  wd_bl;
  windDir[2] =  wd_br;
  windDir[3] =  wd_tr;
  windDir[4] =  wd_tl;
  
  local windSpeed = {};
  windSpeed[1] =  ws_bl;
  windSpeed[2] =  ws_br;
  windSpeed[3] =  ws_tr;
  windSpeed[4] =  ws_tl;

  local weight = {};
  weight[1] = (1-x) * (1-y);
  weight[2] = x * (1-y);
  weight[3] = x * y;
  weight[4] = (1-x)*y;

  return interpolate_windDirectionWithSpeedX(windDir,windSpeed,weight)

end




function lapseratefix(lapseRate,trueHeight,modelHeight,waterFlag)

  -- Limit inversion in Norwegian fjords
  local sea_lapse_rate_limit = -3.0;

  if (waterFlag == 1) then  
    if (lapseRate > sea_lapse_rate_limit) then
      lapseRate = sea_lapse_rate_limit;
    end
  end
    
  local diff = trueHeight - modelHeight;

  if (lapseRate > 0) then
    if (diff < -300) then
      diff = -300;
    end
    if (diff > 150) then
      diff = 150;
    end
  else
    if (diff < -1500) then
      diff = -1500;
    end
    if (diff > 2000) then
      diff = 2000;
    end
  end

  --  lapse rate unit is km, hence we divide by 1000 to get change per meters
  return lapseRate / 1000 * diff;

end





function interpolate_landscape(height,coverType,x,y,
                               val_bl,val_br,val_tr,val_tl,
                               height_bl,height_br,height_tr,height_tl,
                               lapserate_bl,lapserate_br,lapserate_tr,lapserate_tl,
                               land_bl,land_br,land_tr,land_tl)


  if (DEBUG == 1) then
    print("interpolate_landscape("..height..","..coverType..","..x..","..y..","..val_bl..","..val_br..","..val_tr..","..val_tl..","..height_bl..","..height_br..","..height_tr..","..height_tl..","..lapserate_bl..","..lapserate_br..","..lapserate_tr..","..lapserate_tl..","..land_bl..","..land_br..","..land_tr..","..land_tl..")");
  end

  local waterFlag = 0;
  if (coverType == 240) then
    waterFlag = 1;
  end

  if (height == ParamValueMissing or coverType == ParamValueMissing) then  
    return ParamValueMissing;
  end

  if (val_bl == ParamValueMissing or val_br == ParamValueMissing or val_tl == ParamValueMissing or val_tr == ParamValueMissing) then
    return interpolate_linear(x,y,val_bl,val_br,val_tr,val_tl);
  end

  -- print("val_bl="..val_bl.." val_br="..val_br.." val_tl="..val_tl.." val_tr="..val_tr);

  -- Do height corrections if possible

  if (height_bl ~= ParamValueMissing and height_br ~= ParamValueMissing and height_tl ~= ParamValueMissing and height_tr ~= ParamValueMissing) then
  
    local default_lapserate = -6.5;  -- degrees per kilometer
    
    if (lapserate_bl == ParamValueMissing) then 
      lapserate_bl = default_lapserate;
    end
    
    if (lapserate_br == ParamValueMissing) then 
      lapserate_br = default_lapserate;
    end
    
    if (lapserate_tl == ParamValueMissing) then
      lapserate_tl = default_lapserate;
    end
    
    if (lapserate_tr == ParamValueMissing) then
      lapserate_tr = default_lapserate;
    end

    -- Convert the values to the desired height

    -- local fix_bl = lapseratefix(lapserate_bl, height, height_bl, waterFlag);
    -- local fix_br = lapseratefix(lapserate_br, height, height_br, waterFlag);
    -- local fix_tl = lapseratefix(lapserate_tl, height, height_tl, waterFlag);
    -- local fix_tr = lapseratefix(lapserate_tr, height, height_tr, waterFlag);

    -- print("fix_bl="..fix_bl.." fix_br="..fix_br.." fix_tl="..fix_tl.." fix_tr="..fix_tr);

    val_bl = val_bl + lapseratefix(lapserate_bl, height, height_bl, waterFlag);
    val_br = val_br + lapseratefix(lapserate_br, height, height_br, waterFlag);
    val_tl = val_tl + lapseratefix(lapserate_tl, height, height_tl, waterFlag);
    val_tr = val_tr + lapseratefix(lapserate_tr, height, height_tr, waterFlag);

  end
  

  local wbl = (1 - x) * (1 - y);
  local wbr = x * (1 - y);
  local wtl = (1 - x) * y;
  local wtr = x * y;

  -- print("wbl="..wbl.." wbr="..wbr.." wtl="..wtl.." wtr="..wtr);

  -- Modify the coefficients based on the land sea mask


  if (land_bl ~= ParamValueMissing and land_br ~= ParamValueMissing and land_tl ~= ParamValueMissing and land_tr ~= ParamValueMissing) then
    
    -- Minimum weight for any value selected by Mikko Rauhala
    local wlimit = 0.3;

    --  Handle land areas
    if (waterFlag == 0) then
      -- print("**** NO WATER ******");
      -- Scale percentage from 0...1 to wlimit...1
      wbl = wbl * (land_bl + wlimit) / (1 + wlimit);
      wbr = wbr * (land_br + wlimit) / (1 + wlimit);
      wtl = wtl * (land_tl + wlimit) / (1 + wlimit);
      wtr = wtr * (land_tr + wlimit) / (1 + wlimit);
    else
      -- Scale percentage from 0...1 to 1...wlimit
      wbl = wbl * (1 - land_bl + wlimit) / (1 + wlimit);
      wbr = wbr * (1 - land_br + wlimit) / (1 + wlimit);
      wtl = wtl * (1 - land_tl + wlimit) / (1 + wlimit);
      wtr = wtr * (1 - land_tr + wlimit) / (1 + wlimit);
    end
  
  end
  
  -- print("** val_bl="..val_bl.." val_br="..val_br.." val_tl="..val_tl.." val_tr="..val_tr);
  -- print("** wbl="..wbl.." wbr="..wbr.." wtl="..wtl.." wtr="..wtr);
    
  -- Perform combined interpolation

  local value = (wbl * val_bl + wbr * val_br + wtl * val_tl + wtr * val_tr) / (wbl + wbr + wtl + wtr);

  return value;

end





-- ***********************************************************************
--  FUNCTION : IPL_NONE
-- ***********************************************************************
--  The function returns the interpolate value.
-- ***********************************************************************

function IPL_NONE(numOfParams,params)

  if (DEBUG == 1) then
    for index, value in pairs(params) do
      print(index.." : "..value);
    end
  end

  local result = {};
  
  if (numOfParams ~= 7) then   
    result.message = 'Invalid number of parameters given ('..numOfParams..')!';
    result.value = 0;  
    return result.value,result.message;
  end
    
  local n = params[1];
  local x = params[2];
  local y = params[3];
  local val_bl = params[4];
  local val_br = params[5];
  local val_tr = params[6];
  local val_tl = params[7];
        

  result.message = 'OK';
  result.value = interpolate_none(x,y,val_bl,val_br,val_tr,val_tl);  
  return result.value,result.message;
  
end





-- ***********************************************************************
--  FUNCTION : IPL_LINEAR
-- ***********************************************************************
--  The function returns the interpolate value.
-- ***********************************************************************

function IPL_LINEAR(numOfParams,params)

  if (DEBUG == 1) then
    print("IPL_LINEAR()");
    for index, value in pairs(params) do
      print(index.." : "..value);
    end
  end

  local result = {};
  
  if (numOfParams ~= 7) then   
    result.message = 'Invalid number of parameters given ('..numOfParams..')!';
    result.value = 0;  
    return result.value,result.message;
  end
    
  local n = params[1];
  local x = params[2];
  local y = params[3];
  local val_bl = params[4];
  local val_br = params[5];
  local val_tr = params[6];
  local val_tl = params[7];
        
  result.message = 'OK';
  result.value = interpolate_linear(x,y,val_bl,val_br,val_tr,val_tl);  
  return result.value,result.message;
  
end





-- ***********************************************************************
--  FUNCTION : IPL_NEAREST
-- ***********************************************************************
--  The function returns the interpolate value.
-- ***********************************************************************

function IPL_NEAREST(numOfParams,params)

  if (DEBUG == 1) then
    print("IPL_NEAREST()");
    for index, value in pairs(params) do
      print(index.." : "..value);
    end
  end

  local result = {};
  
  if (numOfParams ~= 7) then   
    result.message = 'Invalid number of parameters given ('..numOfParams..')!';
    result.value = 0;  
    return result.value,result.message;
  end
    
  local n = params[1];
  local x = params[2];
  local y = params[3];
  local val_bl = params[4];
  local val_br = params[5];
  local val_tr = params[6];
  local val_tl = params[7];
        

  result.message = 'OK';
  result.value = interpolate_nearest(x,y,val_bl,val_br,val_tr,val_tl);  
  return result.value,result.message;
  
end




-- ***********************************************************************
--  FUNCTION : IPL_MAX
-- ***********************************************************************
--  The function returns the interpolate value.
-- ***********************************************************************

function IPL_MAX(numOfParams,params)

  if (DEBUG == 1) then
    print("IPL_MAX()");
    for index, value in pairs(params) do
      print(index.." : "..value);
    end
  end

  local result = {};
  
  if (numOfParams ~= 7) then   
    result.message = 'Invalid number of parameters given ('..numOfParams..')!';
    result.value = 0;  
    return result.value,result.message;
  end

  local max = ParamValueMissing;
  for index, value in pairs(params) do
    if (index > 3 and ((value > max and value ~= ParamValueMissing) or (max == ParamValueMissing))) then
      max = value;     
    end
  end
    
  result.message = 'OK';
  result.value = max;  
  return result.value,result.message;
  
end




-- ***********************************************************************
--  FUNCTION : IPL_MIN
-- ***********************************************************************
--  The function returns the interpolate value.
-- ***********************************************************************

function IPL_MIN(numOfParams,params)

  if (DEBUG == 1) then
    print("IPL_MIN()");
    for index, value in pairs(params) do
      print(index.." : "..value);
    end
  end

  local result = {};
  
  if (numOfParams ~= 7) then   
    result.message = 'Invalid number of parameters given ('..numOfParams..')!';
    result.value = 0;  
    return result.value,result.message;
  end

  local min = ParamValueMissing;
  for index, value in pairs(params) do
    if (index > 3 and ((value < min and value ~= ParamValueMissing) or min == ParamValueMissing)) then
      min = value;     
    end
  end
    
  result.message = 'OK';
  result.value = min;  
  return result.value,result.message;
  
end




-- ***********************************************************************
--  FUNCTION : IPL_AVG
-- ***********************************************************************
--  The function returns the interpolate value.
-- ***********************************************************************

function IPL_AVG(numOfParams,params)

  if (DEBUG == 1) then
    print("IPL_AVG()");
    for index, value in pairs(params) do
      print(index.." : "..value);
    end
  end

  local result = {};
  
  if (numOfParams ~= 7) then   
    result.message = 'Invalid number of parameters given ('..numOfParams..')!';
    result.value = 0;  
    return result.value,result.message;
  end

  local cnt = 0;
  local sum = 0;
  for index, value in pairs(params) do
    if (index > 3 and (value ~= ParamValueMissing)) then
      sum = sum + value;
      cnt = cnt + 1;     
    end
  end
    
  result.message = 'OK';
  result.value = sum/cnt;  
  return result.value,result.message;
  
end




-- ***********************************************************************
--  FUNCTION : IPL_WIND_DIR
-- ***********************************************************************
--  The function returns the interpolate value.
-- ***********************************************************************

function IPL_WIND_DIR(numOfParams,params)

  if (DEBUG == 1) then
    print("IPL_WIND_DIR()");
    for index, value in pairs(params) do
      print(index.." : "..value);
    end
  end

  local result = {};
  
  if (numOfParams ~= 7 and numOfParams ~= 14) then   
    result.message = 'Invalid number of parameters given ('..numOfParams..')!';
    result.value = 0;  
    return result.value,result.message;
  end
    
  result.message = 'OK';
    
  if (numOfParams == 7) then   
    local n = params[1];
    local x = params[2];
    local y = params[3];
    local val_bl = params[4];
    local val_br = params[5];
    local val_tr = params[6];
    local val_tl = params[7];        
    result.value = interpolate_windDirection(x,y,val_bl,val_br,val_tr,val_tl);
  else  
    local n = params[1];
    local x = params[2];
    local y = params[3];
    local a_val_bl = params[4];
    local a_val_br = params[5];
    local a_val_tr = params[6];
    local a_val_tl = params[7];       
    local b_val_bl = params[11];
    local b_val_br = params[12];
    local b_val_tr = params[13];
    local b_val_tl = params[14];       
    result.value = interpolate_windDirectionWithSpeed(x,y,a_val_bl,a_val_br,a_val_tr,a_val_tl,b_val_bl,b_val_br,b_val_tr,b_val_tl);
  end
      
  return result.value,result.message;
  
end





-- ***********************************************************************
--  FUNCTION : IPL_ENSEMBLE
-- ***********************************************************************
--  The function returns the interpolate value.
-- ***********************************************************************

function IPL_ENSEMBLE(numOfParams,params)

  if (DEBUG == 1) then
    print("IPL_ENSEMBLE()");
    for index, value in pairs(params) do
      print(index.." : "..value);
    end
  end

  local result = {};
  
  result.message = 'OK';
  result.value = 0;
    
  return result.value,result.message;
  
end




-- ***********************************************************************
--  FUNCTION : IPL_WIND_VECTOR
-- ***********************************************************************
--  The function returns the interpolate value.
-- ***********************************************************************

function IPL_WIND_VECTOR(numOfParams,params)

  if (DEBUG == 1) then
    print("IPL_WIND_VECTOR()");
    for index, value in pairs(params) do
      print(index.." : "..value);
    end
  end

  local result = {};
  
  if (numOfParams ~= 16) then   
    result.message = 'Invalid number of parameters given ('..numOfParams..')!';
    result.value = 0;  
    return result.value,result.message;
  end
    
  result.message = 'OK';
    
  local n = params[2];
  local x = params[3];
  local y = params[4];
  local ws_bl = params[5];
  local ws_br = params[6];
  local ws_tr = params[7];
  local ws_tl = params[8];        
  local wd_bl = params[13];
  local wd_br = params[14];
  local wd_tr = params[15];
  local wd_tl = params[16];       
  result.value = interpolate_windVector2(x,y,wd_bl,wd_br,wd_tr,wd_tl,ws_bl,ws_br,ws_tr,ws_tl);
      
  return result.value,result.message;
  
end





-- ***********************************************************************
--  FUNCTION : IPL_LANDSCAPE
-- ***********************************************************************
--  The function returns the interpolate value.
-- ***********************************************************************

function IPL_LANDSCAPE(numOfParams,params)

  if (DEBUG == 1) then
    print("IPL_LANDSCAPE()");
    for index, value in pairs(params) do
      print(index.." : "..value);
    end
  end
 
  local result = {};
     
  result.message = 'OK';
    
  local height = params[1];
  local coverType = params[2];
  local n1 = ParamValueMissing;
  local x1 = ParamValueMissing;
  local y1 = ParamValueMissing;
  local val_bl = ParamValueMissing;
  local val_br = ParamValueMissing;
  local val_tr = ParamValueMissing;
  local val_tl = ParamValueMissing;
    
  local n2 = ParamValueMissing;
  local x2 = ParamValueMissing;
  local y2 = ParamValueMissing;
  local height_bl = ParamValueMissing;
  local height_br = ParamValueMissing;
  local height_tr = ParamValueMissing;
  local height_tl = ParamValueMissing;
    
  local n3 = ParamValueMissing;
  local x3 = ParamValueMissing;
  local y3 = ParamValueMissing;
  local lapserate_bl = ParamValueMissing;
  local lapserate_br = ParamValueMissing;
  local lapserate_tr = ParamValueMissing;
  local lapserate_tl = ParamValueMissing;
    
  local n4 = ParamValueMissing;
  local x4 = ParamValueMissing;
  local y4 = ParamValueMissing;    
  local land_bl = ParamValueMissing;
  local land_br = ParamValueMissing;
  local land_tr = ParamValueMissing;
  local land_tl = ParamValueMissing;

  local p = 3;
  if (p <= numOfParams and  params[p] == 7  and (p + 7) <= numOfParams) then    
    n1 = params[p+1];
    x1 = params[p+2];
    y1 = params[p+3];
    val_bl = params[p+4];
    val_br = params[p+5];
    val_tr = params[p+6];
    val_tl = params[p+7];
    p = p + 8;
  else
    p = p + 1;
  end
    
  if (p <= numOfParams and  params[p] == 7  and (p + 7) <= numOfParams) then    
    n2 = params[p+1];
    x2 = params[p+2];
    y2 = params[p+3];
    height_bl = params[p+4]; -- / 9.81;
    height_br = params[p+5]; -- / 9.81;
    height_tr = params[p+6]; -- / 9.81;
    height_tl = params[p+7]; -- / 9.81;
    p = p + 8;
  else
    p = p + 1;
  end
    
  if (p <= numOfParams and  params[p] == 7  and (p + 7) <= numOfParams) then    
    n3 = params[p+1];
    x3 = params[p+2];
    y3 = params[p+3];
    lapserate_bl = params[p+4];
    lapserate_br = params[p+5];
    lapserate_tr = params[p+6];
    lapserate_tl = params[p+7];
    p = p + 8;
  else
    p = p + 1;
  end
    
  if (p <= numOfParams and  params[p] == 7  and (p + 7) <= numOfParams) then    
    n4 = params[p+1];
    x4 = params[p+2];
    y4 = params[p+3];    
    land_bl = params[p+4];
    land_br = params[p+5];
    land_tr = params[p+6];
    land_tl = params[p+7];
  else
    p = p + 1;
  end
  
  if (height ~= ParamValueMissing) then        
    result.value = interpolate_landscape(height,coverType,x1,y1,
                      val_bl,val_br,val_tr,val_tl,height_bl,height_br,height_tr,height_tl,
                      lapserate_bl,lapserate_br,lapserate_tr,lapserate_tl,land_bl,land_br,land_tl,land_tr);
  else
    result.value =  interpolate_linear(x1,y1,val_bl,val_br,val_tr,val_tl);  
  end                                                             
      
  return result.value,result.message;
  
end





-- ***********************************************************************
--  FUNCTION : getQueryParamStr
-- ***********************************************************************
--  This method merges query parameters into a string. 
-- ***********************************************************************

function getQueryParamStr(parameterKey,parameterLevelId,parameterLevel,forecastType,forecastNumber,areaInterpolationMethod,anyTime,anyProducer)

  local p = "Q:"..parameterKey;
  p = p..","..parameterLevelId;
  p = p..","..parameterLevel;
  p = p..","..forecastType;
  p = p..","..forecastNumber;
  p = p..","..areaInterpolationMethod;
  p = p..","..anyTime;
  p = p..","..anyProducer;
    
  return p;

end




-- ***********************************************************************
--  FUNCTION : getQueryParamStr2
-- ***********************************************************************
--  This method merges query parameters into a string. 
-- ***********************************************************************

function getQueryParamStr2(parameterKey,parameterLevelId,parameterLevel,forecastType,forecastNumber,areaInterpolationMethod,anyTime,anyProducer)

  local p = "P:"..parameterKey;
  p = p..","..parameterLevelId;
  p = p..","..parameterLevel;
  p = p..","..forecastType;
  p = p..","..forecastNumber;
  p = p..","..areaInterpolationMethod;
  p = p..","..anyTime;
  p = p..","..anyProducer;
    
  return p;

end



-- ***********************************************************************
--  FUNCTION : mergeInstructionParameters
-- ***********************************************************************
  -- Merging the instructions parts to a single string.
-- ***********************************************************************

function mergeInstructionParameters(p)

  local paramStr = "";    
  for index, value in pairs(p) do
    paramStr = paramStr..value..";";
  end

  return paramStr;

end 



-- ***********************************************************************
--  FUNCTION : getAreaInterpolationInfo_ext_none
-- ***********************************************************************
--  This function returns "instructions" how to interpolate the current
--  parameter. 

-- ***********************************************************************

function getAreaInterpolationInfo_ext_none(qp)

  local p = {};

  -- This is the actual Lua interpolation function.  
  p[1] = "F:IPL_NONE";    
  
  -- It needs the values of the original parameter (all grid corners). 
  p[2] = getQueryParamStr(qp.parameterKey,qp.parameterLevelId,qp.parameterLevel,qp.forecastType,qp.forecastNumber,AreaInterpolationMethod.List,0,0);
  
  return mergeInstructionParameters(p);
  
end




-- ***********************************************************************
--  FUNCTION : getAreaInterpolationInfo_ext_liner
-- ***********************************************************************
--  This function returns "instructions" how to interpolate the current
--  parameter. 

-- ***********************************************************************

function getAreaInterpolationInfo_ext_linear(qp)

  local p = {};

  -- This is the actual Lua interpolation function.  
  p[1] = "F:IPL_LINEAR";    
  
  -- It needs the values of the original parameter (all grid corners). 
  p[2] = getQueryParamStr(qp.parameterKey,qp.parameterLevelId,qp.parameterLevel,qp.forecastType,qp.forecastNumber,AreaInterpolationMethod.List,0,0);
  
  return mergeInstructionParameters(p);

end





-- ***********************************************************************
--  FUNCTION : getAreaInterpolationInfo_ext_nerest
-- ***********************************************************************
--  This function returns "instructions" how to interpolate the current
--  parameter. 

-- ***********************************************************************

function getAreaInterpolationInfo_ext_nearest(qp)

  local p = {};

  -- This is the actual Lua interpolation function.  
  p[1] = "F:IPL_NEAREST";    
  
  -- It needs the values of the original parameter (all grid corners). 
  p[2] = getQueryParamStr(qp.parameterKey,qp.parameterLevelId,qp.parameterLevel,qp.forecastType,qp.forecastNumber,AreaInterpolationMethod.List,0,0);
  
  
  return mergeInstructionParameters(p);

end





-- ***********************************************************************
--  FUNCTION : getAreaInterpolationInfo_ext_landscape
-- ***********************************************************************
--  This function returns "instructions" how to interpolate the current
--  parameter. In practice, it returns the actual interpolation function
--  name (which must be registered as a type 1 function in some of the
--  LUA files). In addition it defines how the parameters are fetched
--  for the current interpolation function. The returned "instructions"
--  are divided into different parts by ';' character. Each part is
--  divided into two parts by ':' character. The part before ':' character
--  is a letter that defines the content of the second part:
--
--     F:<luaFunction>     => This function is called for the interpolation
--     V:<variableName>    => The variable name is replaced by its value
--     C:<constValue>      => The constant value is added as a parameter 
--     Q:<queryParameter>  => The request for fetching parameter value(s).

-- ***********************************************************************

function getAreaInterpolationInfo_ext_landscape(qp)

  local p = {};

  -- This is the actual Lua interpolation function.  
  p[1] = "F:IPL_LANDSCAPE";    
  
  -- It requires 'dem' ja 'coverType' information as parameters.
  p[2] = "V:dem";
  p[3] = "V:coverType";      

  -- It also needs the values of the original parameter (all grid corners). 
  p[4] = getQueryParamStr(qp.parameterKey,qp.parameterLevelId,qp.parameterLevel,qp.forecastType,qp.forecastNumber,AreaInterpolationMethod.List,0,0);
  
  -- It also needs values of the GeopHeight -parameter.
  p[5] = getQueryParamStr("GeopHeight","0","0",qp.forecastType,qp.forecastNumber,AreaInterpolationMethod.List,0,0);
   
  -- It also needs values of the LapseRate -parameter.
  p[6] = getQueryParamStr("LapseRate","0","0",qp.forecastType,qp.forecastNumber,AreaInterpolationMethod.List,0,1);
  
  -- It also needs values of the LandSeaMask -parameter.
  p[7] = getQueryParamStr("LandSeaMask","1","0",qp.forecastType,qp.forecastNumber,AreaInterpolationMethod.List,1,0);


  return mergeInstructionParameters(p);

end





-- ***********************************************************************
--  FUNCTION : getAreaInterpolationInfo_ext_windDirection
-- ***********************************************************************
--  This function returns "instructions" how to interpolate the current
--  parameter. 

-- ***********************************************************************

function getAreaInterpolationInfo_ext_windDirection(qp)

  local p = {};

  -- This is the actual Lua interpolation function.  
  p[1] = "F:IPL_WIND_DIR";    
 
  -- It needs the values of the original parameter (all grid corners). 
  p[2] = getQueryParamStr(qp.parameterKey,qp.parameterLevelId,qp.parameterLevel,qp.forecastType,qp.forecastNumber,AreaInterpolationMethod.List,0,0);
    
  -- It needs the values of the 'WindSpeedMS' parameter (all grid corners). 
  p[3] = getQueryParamStr("WindSpeedMS",qp.parameterLevelId,qp.parameterLevel,qp.forecastType,qp.forecastNumber,AreaInterpolationMethod.List,0,0);
  
  return mergeInstructionParameters(p);

end





-- ***********************************************************************
--  FUNCTION : getAreaInterpolationInfo_ext_ensemble
-- ***********************************************************************
--  This function returns "instructions" how to interpolate the current
--  parameter. 

-- ***********************************************************************

function getAreaInterpolationInfo_ext_ensemble(qp)

  print("ENSEMBLE INFO");
  local p = {};

  -- This is the actual Lua interpolation function.  
  p[1] = "F:IPL_ENSEMBLE";    
 
  for i=1,qp.forecastNumber do
    p[i+1] = getQueryParamStr2(qp.parameterKey,qp.parameterLevelId,qp.parameterLevel,3,i,AreaInterpolationMethod.Linear,0,0);
  end

  return mergeInstructionParameters(p);

end




-- ***********************************************************************
--  FUNCTION : getAreaInterpolationInfo_ext_windVector
-- ***********************************************************************
--  This function returns "instructions" how to interpolate the current
--  parameter. 

-- ***********************************************************************

function getAreaInterpolationInfo_ext_windVector(qp)

  local p = {};

  -- This is the actual Lua interpolation function.  
  p[1] = "F:IPL_WIND_VECTOR";    
 
  -- It needs the values of the original parameter (all grid corners). Assuming that the
  -- parameter is "WindSpeedMS" 
  p[2] = getQueryParamStr(qp.parameterKey,qp.parameterLevelId,qp.parameterLevel,qp.forecastType,qp.forecastNumber,AreaInterpolationMethod.List,0,0);
    
  -- It needs the values of the 'WindDirection' parameter (all grid corners). 
  p[3] = getQueryParamStr("WindDirection",qp.parameterLevelId,qp.parameterLevel,qp.forecastType,qp.forecastNumber,AreaInterpolationMethod.List,0,0);
  
  return mergeInstructionParameters(p);

end




-- ***********************************************************************
--  FUNCTION : getAreaInterpolationInfo
-- ***********************************************************************
--  This function returns "instructions" how to interpolate the current
--  parameter according to the current interpolation method.
-- ***********************************************************************

function getAreaInterpolationInfo(numOfParams,params)

  if (DEBUG == 1) then
    print("getAreaInterpolationInfo()");
    for index, value in pairs(params) do
      print(index.." : "..value);
    end
  end

  local qp = {};
  qp.producerName = params[1];
  qp.parameterName = params[2];
  qp.parameterKeyType = tonumber(params[3]);
  qp.parameterKey = params[4];
  qp.parameterLevelIdType = tonumber(params[5]);
  qp.parameterLevelId = tonumber(params[6]);
  qp.parameterLevel = tonumber(params[7]);
  qp.forecastType = tonumber(params[8]);
  qp.forecastNumber = tonumber(params[9]);
  qp.areaInterpolationMethod = tonumber(params[10]);

  if (DEBUG == 1) then
    for index, value in pairs(qp) do
      print(index.." : "..value);
    end
  end

  local instructions = "";
  
  if (qp.areaInterpolationMethod == AreaInterpolationMethod.ExtNone) then
    instructions = getAreaInterpolationInfo_ext_none(qp);
  end
  
  if (qp.areaInterpolationMethod == AreaInterpolationMethod.ExtLinear) then
    instructions = getAreaInterpolationInfo_ext_linear(qp);
  end
  
  if (qp.areaInterpolationMethod == AreaInterpolationMethod.ExtNearest) then
    instructions = getAreaInterpolationInfo_ext_nearest(qp);
  end
  
  if (qp.areaInterpolationMethod == AreaInterpolationMethod.ExtLandscape) then
    instructions = getAreaInterpolationInfo_ext_landscape(qp);
  end;
    
  if (qp.areaInterpolationMethod == AreaInterpolationMethod.ExtWindDirection) then
    instructions = getAreaInterpolationInfo_ext_windDirection(qp);
  end;
    
  if (qp.areaInterpolationMethod == AreaInterpolationMethod.ExtWindVector) then
    instructions = getAreaInterpolationInfo_ext_windVector(qp);
  end;

  if (qp.areaInterpolationMethod == AreaInterpolationMethod.ExtEnsemble) then 
    instructions = getAreaInterpolationInfo_ext_ensemble(qp);
  end;

  if (DEBUG == 1) then
    print("  "..instructions);
  end

  return instructions;

end





-- ***********************************************************************
--  FUNCTION : getAreaInterpolationInfo2
-- ***********************************************************************
--  This function returns "instructions" how to interpolate the current
--  parameter according to the current interpolation method.
-- ***********************************************************************

function getAreaInterpolationInfo2(producerName,parameterName,parameterKeyType,parameterKey,parameterLevelIdType,parameterLevelId,parameterLevel,forecastType,forecastNumber,areaInterpolationMethod)

  if (DEBUG == 1) then
    print("getAreaInterpolationInfo2("..producerName..","..parameterName..","..parameterKeyType..","..parameterKey..","..parameterLevelIdType..","..parameterLevelId..","..parameterLevel..","..forecastType..","..forecastNumber..","..areaInterpolationMethod..")");
  end

  local qp = {};
  qp.producerName = producerName;
  qp.parameterName = parameterName;
  qp.parameterKeyType = parameterKeyType;
  qp.parameterKey = parameterKey;
  qp.parameterLevelIdType = parameterLevelIdType;
  qp.parameterLevelId = parameterLevelId;
  qp.parameterLevel = parameterLevel;
  qp.forecastType = forecastType;
  qp.forecastNumber = forecastNumber;
  qp.areaInterpolationMethod = areaInterpolationMethod;

  -- if (DEBUG == 1) then
  --  for index, value in pairs(qp) do
  --    print(index.." : "..value);
  --  end
  -- end

  local instructions = "";
  
  if (areaInterpolationMethod == AreaInterpolationMethod.ExtNone) then
    instructions = getAreaInterpolationInfo_ext_none(qp);
  end
  
  if (areaInterpolationMethod == AreaInterpolationMethod.ExtLinear) then
    instructions = getAreaInterpolationInfo_ext_linear(qp);
  end
  
  if (areaInterpolationMethod == AreaInterpolationMethod.ExtNearest) then
    instructions = getAreaInterpolationInfo_ext_nearest(qp);
  end
  
  if (areaInterpolationMethod == AreaInterpolationMethod.ExtLandscape) then
    instructions = getAreaInterpolationInfo_ext_landscape(qp);
  end;
    
  if (areaInterpolationMethod == AreaInterpolationMethod.ExtWindDirection) then
    instructions = getAreaInterpolationInfo_ext_windDirection(qp);
  end;
    
  if (areaInterpolationMethod == AreaInterpolationMethod.ExtWindVector) then
    instructions = getAreaInterpolationInfo_ext_windVector(qp);
  end;
  
  if (areaInterpolationMethod == AreaInterpolationMethod.ExtEnsemble) then
    instructions = getAreaInterpolationInfo_ext_ensemble(qp);
  end;
    
  if (DEBUG == 1) then
    print("  "..instructions);
  end

  return instructions;

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
    functionNames = 'IPL_NONE,IPL_LINEAR,IPL_NEAREST,IPL_MAX,IPL_MIN,IPL_AVG,IPL_WIND_DIR,IPL_WIND_VECTOR,IPL_LANDSCAPE,IPL_ENSEMBLE';
  end
  
  if (type == 6) then 
    functionNames = 'getAreaInterpolationInfo';
  end
      
  return functionNames;

end



