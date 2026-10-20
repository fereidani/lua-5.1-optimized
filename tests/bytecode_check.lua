-- Malformed-but-verifier-accepted bytecode that hits a runtime_check in the
-- VM. Stock skips the failing instruction and carries on; with threaded
-- dispatch a bare `break` there used to leave luaV_execute without unwinding
-- the call. Compared byte-for-byte against stock Lua 5.1.5.
local f = function() local t = {1}; return "done" end
local s = string.dump(f)
-- NEWTABLE 0 1 0, as a little-endian word; the dump is native-endian, so on a
-- big-endian host this pattern is absent and the check is skipped
local i = s:find(string.char(0x0A, 0x00, 0x80, 0x00), 1, true)
if not i then print("skipped: not little-endian"); return end
-- replace it with LOADNIL 0 0, so the following SETLIST sees a nil, not a table
s = s:sub(1, i - 1) .. string.char(3, 0, 0, 0) .. s:sub(i + 4)
local g = assert(loadstring(s))
print("setlist on nil:", pcall(g))
print("after:", select("#", pcall(g)))
