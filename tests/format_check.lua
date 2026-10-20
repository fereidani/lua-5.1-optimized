-- Emit every interesting string.format result; compared byte-for-byte against
-- the same script run on stock Lua 5.1.5.
local out = {}
local function e(...) out[#out+1] = (pcall(string.format, ...)) and string.format(...) or "ERR" end
local specs_n = {"%d","%i","%5d","%-5d","%05d","%+d","% d","%.3d","%x","%X","%o","%u","%c",
                 "%g","%G","%e","%E","%f","%.14g","%.3f","%10.4f","%-10.2e","%#x"}
local nums = {0,-0.0,1,-1,7,-7,42,1e13,99999999999999,100000000000000,-99999999999999,
              123456789,2^31,2^53,0.5,-0.5,1/3,3.14159265358979,1e-5,1e300,65,97,255}
for _,s in ipairs(specs_n) do for _,n in ipairs(nums) do e(s,n) end end
for i=-2000,2000 do e("%d",i); e("%.14g",i); e("%5d",i) end
local strs = {"","a","abc","hello world",("x"):rep(99),("y"):rep(100),("z"):rep(300),
              "with\0embedded","tab\there","utf\195\169"}
for _,s in ipairs(strs) do
  e("%s",s); e("%10s",s); e("%-10s",s); e("%.3s",s); e("%q",s)
end
e("%s%d%s", "a", 5, "b")
e("literal %% and %s", "x")
e("%d%%", 50)
math.randomseed(999)
for i=1,20000 do
  local n = math.random(-1e9,1e9)
  e("%d",n); e("%.14g",n); e("%.14g", n/7); e("%s", tostring(n))
end
print(table.concat(out,"\1"))
