--lua Script, ONBOARD T7 for kill switch purposes
--Lua Scripting examples, https://github.com/labjack/labjack_kipling/tree/master/ljswitchboard-module_manager/lib/switchboard_modules/lua_script_debugger

--[[ FROM LABJACK DOCUMENTATION https://support.labjack.com/docs/25-0-lua-scripting-t-series-datasheet
Notice: From a practical standpoint, Lua Scripts will start becoming too long 
and throwing out-of-ram errors after around 150 lines (depending on how long each line of code is).  
Once this limitation is encountered, there are a few tricks that can be used to implement additional features.  
They all involve making your code less readable but will assist in implementing additional features.

1. Remove any comments from your code as these consume precious RAM.
2. Edit the lua script file using an editor outside of Kipling and upload a minified version of the script instead of the full script file.  
The suggested tool to use for minifying Lua Scripts is here: https://mothereff.in/lua-minifier. 
Another Lua minifier is here: https://goonlinetools.com/lua-minifier/

can still combine with a host computer (pi, esp32, etc), but its prolly fine. kill switch logic should not be that long.
--]]

-- Analog input pins on DB37 port #s 37, 18, 36, 17, 35 (in order of AIN0-AIN4)
-- AIN0-AIN3 are duplicated connections on screw terminals
local pins = {"AIN0", "AIN1", "AIN2", "AIN3", "AIN4"} 

