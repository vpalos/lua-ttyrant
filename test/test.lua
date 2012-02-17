#!/usr/bin/env lua


--
-- A test-set for Lua-TTyrant.
--
-- Copyright (C)2010 by Valeriu Palos. All rights reserved.
-- This library is under the MIT license (see doc/LICENSE).
--


--
-- Import API.
--
local ttyrant = require'ttyrant'


--
-- Assertion wrapper.
--
local _assert = assert
local _testno = 1
local assert = function(...)
    io.stdout:write(("Test %d: "):format(_testno))
    io.stdout:flush()
    local result = _assert(...)
    _testno = _testno + 1
    print("ok.")
    return result
end


--
-- Hash database tests.
--

-- ttyrant:open()
local th = assert(ttyrant.hash:open('localhost', 1978))

-- ttyrant:vanish()
assert(th:vanish())

-- ttyrant:sync()
assert(th:sync())

-- ttyrant:put()
assert(th:put('Key1', 'NIKA'))
assert(th:put('Key2', 'ICXC'))

-- ttyrant:put() - multiple keys at once given as table
assert(th:put{ martyr1 = 'Valeriu Gafencu',
               martyr2 = 'Radu Gyr',
               martyr3 = 'Virgil Maxim',
               martyr4 = 'Vasile Militaru',
               martyr5 = 'Costache Oprişan' })

-- ttyrant:putnr()
assert(th:putnr('martyr6', 'Ianolide Ioan'))

-- ttyrant:putkeep()
assert(th:putkeep('martyr7', 'Corneliu Zelea'))
assert(not th:putkeep('martyr7', 'Corneliu Zelea Codreanu'))

-- ttyrant:putcat()
assert(th:putcat('martyr7', ' Codreanu'))

-- ttyrant:putshl()
assert(th:putshl('martyr6', ' Ianolide', 13))

-- ttyrant:vsiz()
assert(th:vsiz('martyr6', 13))

-- ttyrant:put() - multiple keys at once given as list
assert(th:put( 'saint1', 'Gheorghe',
               'saint2', 'Anastasia',
               'saint3', 'Dimitrie',
               'saint4', 'Antonie',
               'saint5', 'Filofteia',
               'saint6', 'Oprea',
               'saint7', 'Ilie Lăcătuşu' ))

-- ttyrant:get()
assert(th:get('Key1') == 'NIKA')
assert(th:get('Key2') == 'ICXC')

-- ttyrant:get() - multiple keys at once given as table
local vmtr = assert(th:get{ 'martyr1', 'martyr2', 'martyr3', 'martyr4', 'martyr5', 'martyr6', 'martyr7' })
assert(vmtr.martyr1 == 'Valeriu Gafencu')
assert(vmtr.martyr2 == 'Radu Gyr')
assert(vmtr.martyr3 == 'Virgil Maxim')
assert(vmtr.martyr4 == 'Vasile Militaru')
assert(vmtr.martyr5 == 'Costache Oprişan')
assert(vmtr.martyr6 == 'Ioan Ianolide')
assert(vmtr.martyr7 == 'Corneliu Zelea Codreanu')

-- ttyrant:get() - multiple keys at once given as list
local vsnt = assert(th:get('saint1', 'saint2', 'saint3', 'saint4', 'saint5', 'saint6', 'saint7'))
assert(vsnt.saint1 == 'Gheorghe')
assert(vsnt.saint2 == 'Anastasia')
assert(vsnt.saint3 == 'Dimitrie')
assert(vsnt.saint4 == 'Antonie')
assert(vsnt.saint5 == 'Filofteia')
assert(vsnt.saint6 == 'Oprea')
assert(vsnt.saint7 == 'Ilie Lăcătuşu')

-- ttyrant:out()
assert(th:out('Key1'))
assert(not th:get('Key1'))

-- ttyrant:out() - multiple keys at once given as table
assert(th:out{ 'saint1', 'saint4', 'martyr6', 'martyr7' })
assert(not th:get('saint1'))
assert(not th:get('saint4'))
assert(not th:get('martyr6'))
assert(not th:get('martyr7'))

-- ttyrant:out() - multiple keys at once given as list
assert(th:out('martyr1', 'martyr4', 'saint6', 'saint7'))
assert(not th:get('martyr1'))
assert(not th:get('martyr4'))
assert(not th:get('saint6'))
assert(not th:get('saint7'))

-- ttyrant:increment()
assert(th:increment('Key1',  3) == 3)
assert(th:increment('Key1',  3) == 6)
assert(th:increment('Key1', -1) == 5)
assert(th:increment('Key1',  3) == 8)

-- ttyrant:rnum()
assert(th:rnum() == 8)

-- ttyrant:stat()
assert(tonumber(th:stat()['rnum']) == 8)

-- ttyrant:size()
assert(th:size() > 0)

-- ttyrant:copy()
assert(th:copy("/tmp/test.tch-backup"))

-- ttyrant:keys()
local keys = {
    ["Key2"] = true,
    ["martyr2"] = true,
    ["martyr5"] = true,
    ["martyr3"] = true,
    ["saint2"] = true,
    ["saint3"] = true,
    ["saint5"] = true,
    ["Key1"] = true
}
for key in th:iterator() do
    assert(keys[key] ~= nil)
    keys[key] = nil
end

-- ttyrant:fwmkeys()
local keys = {
    ["saint2"] = true,
    ["saint3"] = true,
    ["saint5"] = true,
}
local test = th:fwmkeys("saint", 4)
for _, key in ipairs(test) do
    assert(keys[key] ~= nil)
    keys[key] = nil
end

-- ttyrant:close()
assert(th:close())


--
-- Table database tests.
--

-- ttyrant.table:open()
local tt = assert(ttyrant.table:open('localhost:1979'))

-- ttyrant.table:vanish()
assert(tt:vanish())

-- ttyrant.table:sync()
assert(tt:sync())

-- ttyrant.table:put()
assert(tt:put('abc', { a = 1.23, b = 4.56, c = 7.89 }))
assert(tt:put('abc1', { a = 11.23, b = 41.56, c = 71.89 }))
assert(tt:put('abc2', { a = 12.23, b = 42.56, c = 72.89 }))
assert(tt:put('abc3', { a = 13.23, b = 43.56, c = 73.89 }))
assert(tt:put('abc4', { a = 14.23, b = 44.56, c = 74.89 }))

-- ttyrant.table:putkeep()
assert(tt:putkeep('123', { [1] = 1.11, [2] = 2.22, [3] = 3.33 }))
assert(not tt:putkeep('123', { "---" }))

-- ttyrant.table:putcat()
assert(tt:putcat('abc', { d = 9.10 }))
assert(tt:putcat('123', { [4] = 4.44 }))

-- ttyrant.table:get()
local vabc = assert(tt:get('abc'))
local v123 = assert(tt:get('123'))
assert(tonumber(vabc['a']) == 1.23)
assert(tonumber(vabc['d']) == 9.10)
assert(tonumber(v123['3']) == 3.33)
assert(tonumber(v123['4']) == 4.44)

-- ttyrant.table:out()
assert(tt:out('123'))
assert(not tt:get('123'))

-- ttyrant.table:out() - multiple keys at once given as table
assert(tt:out{'abc1', 'abc2'})
assert(not tt:get('abc1'))
assert(not tt:get('abc2'))

-- ttyrant.table:out() - multiple keys at once given as list
assert(tt:out('abc3', 'abc4'))
assert(not tt:get('abc3'))
assert(not tt:get('abc4'))

-- ttyrant.table:increment()
assert(tt:increment('abc',  3) == 3)
assert(tt:increment('abc',  3) == 6)
assert(tt:increment('abc', -1) == 5)
assert(tt:increment('abc',  3) == 8)

-- ttyrant.table:copy()
assert(tt:copy("/tmp/test.tct-backup"))

-- ttyrant.table:setindex()
assert(tt:setindex("a", "decimal"))
assert(tt:setindex("a", "decimal", false))
assert(not tt:setindex("a", "decimal", true))

-- ttyrant:keys()
assert(tt:put('abc1', { a = 11.23, b = 41.56, c = 71.89 }))
assert(tt:put('abc2', { a = 12.23, b = 42.56, c = 72.89 }))
assert(tt:put('abc3', { a = 13.23, b = 43.56, c = 73.89 }))
assert(tt:put('abc4', { a = 14.23, b = 44.56, c = 74.89 }))
local keys = {
    ["abc"] = true,
    ["abc1"] = true,
    ["abc2"] = true,
    ["abc3"] = true,
    ["abc4"] = true,
}
for key in tt:iterator() do
    assert(keys[key] ~= nil)
    keys[key] = nil
end

-- ttyrant:fwmkeys()
local keys = {
    ["abc"] = true,
    ["abc1"] = true,
    ["abc2"] = true,
}
local test = th:fwmkeys("abc", 3)
for _, key in ipairs(test) do
    assert(keys[key] ~= nil)
    keys[key] = nil
end


-- leaving table db open for query tests...


--
-- Query tests.
--

-- ttyrant.query:new()
local qr = assert(ttyrant.query:new(tt))

-- ttyrant.query:addcond()
assert(qr:addcond('b', 'RDBQCNUMGE', '4.56'))   -- official rule name
assert(qr:addcond('c', 'numlt', '7.90'))        -- without 'RDBQC' prefix, case-insensitive

-- ttyrant.query:search()
local result = assert(qr:search())
assert(#result == 1)

-- ttyrant.query:delete()
assert(qr:delete())

-- ttyrant.query:limit()
-- ttyrant.query:order()
assert(tt:put('student1', { grade = '1' }))
assert(tt:put('student2', { grade = '10' }))
assert(tt:put('student3', { grade = '100' }))
assert(tt:put('student4', { grade = '999' }))
assert(tt:put('student5', { grade = '43.7', flowers = 'roses' }))
local qr = assert(ttyrant.query:new(tt))
assert(qr:addcond('grade', 'numge', '0'))
assert(qr:setlimit(3))
assert(qr:setorder('grade', 'numdesc'))
local result = assert(qr:search())
assert(#result == 3)
assert(result[1] == 'student4')
assert(result[2] == 'student3')
assert(result[3] == 'student5')
assert(qr:delete())

-- ttyrant.query:searchget()
-- ttyrant.query:searchout()
-- ttyrant.query:searchcount()
local qr = assert(ttyrant.query:new(tt))
assert(qr:addcond('grade', 'numeq', '43.7'))
local result = assert(qr:searchget())
assert(result['student5']['flowers'] == 'roses')
assert(qr:searchcount() == 1);
assert(qr:searchout());
assert(qr:searchcount() == 0);
assert(qr:delete())

-- ttyrant.table:rnum()
assert(tt:rnum() == 9)

-- ttyrant.table:size()
assert(tt:size() > 0)

-- ttyrant.table:close()
assert(tt:close())


--
-- Success.
--
print('All tests completed successfully.')
