#!/usr/bin/env lua


--
-- Import API.
--

require'ttyrant'


--
-- Hash database tests.
--

-- ttyrant:new()
local th = assert(ttyrant:new{ host ='localhost', port = 1978 })

-- ttyrant:put()
assert(th:put('Key1', 'NIKA'))
assert(th:put('Key2', 'ICXC'))

-- ttyrant:put() - multiple keys given as table
assert(th:put{ martyr1 = 'Valeriu Gafencu',
               martyr2 = 'Radu Gyr',
               martyr3 = 'Virgil Maxim',
               martyr4 = 'Vasile Militaru',
               martyr5 = 'Costache Oprişan',
               martyr6 = 'Ioan Ianolide',
               martyr7 = 'Corneliu Zelea Codreanu' })

-- ttyrant:put() - multiple keys given as list
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

-- ttyrant.table:get() - multiple keys given as table
local vmtr = assert(th:get{ 'martyr1', 'martyr2', 'martyr3', 'martyr4', 'martyr5', 'martyr6', 'martyr7' })
assert(vmtr.martyr1 == 'Valeriu Gafencu')
assert(vmtr.martyr2 == 'Radu Gyr')
assert(vmtr.martyr3 == 'Virgil Maxim')
assert(vmtr.martyr4 == 'Vasile Militaru')
assert(vmtr.martyr5 == 'Costache Oprişan')
assert(vmtr.martyr6 == 'Ioan Ianolide')
assert(vmtr.martyr7 == 'Corneliu Zelea Codreanu')

-- ttyrant.table:get() - multiple keys given as list
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

-- ttyrant:add()
assert(th:add('Key1',  3) == 3)
assert(th:add('Key1',  3) == 6)
assert(th:add('Key1', -1) == 5)
assert(th:add('Key1',  3) == 8)

-- ttyrant:close()
assert(th:close())


--
-- Table database tests.
--

-- ttyrant.table:new() - single key
local tt = assert(ttyrant.table:new{ host ='localhost', port = 1979 })

-- ttyrant.table:put()
assert(tt:put('abc', { a = 1.23, b = 4.56, c = 7.89 }))
assert(tt:put('123', { 1.11, 2.22, 3.33 }))

---- ttyrant.table:put() - multiple keys given as table
--assert(tt:put({ martyr1 = { name = 'Valeriu Gafencu' },
--                martyr2 = { name = 'Radu Gyr' },
--                martyr3 = { name = 'Virgil Maxim' },
--                martyr4 = { name = 'Vasile Militaru' },
--                martyr5 = { name = 'Costache Oprişan' },
--                martyr6 = { name = 'Ioan Ianolide' },
--                martyr7 = { name = 'Corneliu Zelea Codreanu' } }))

---- ttyrant.table:put() - multiple keys given as list
--assert(tt:put( 'saint1', { name = 'Gheorghe' },
--               'saint2', { name = 'Anastasia' },
--               'saint3', { name = 'Dimitrie' },
--               'saint4', { name = 'Antonie' },
--               'saint5', { name = 'Filfteia' },
--               'saint6', { name = 'Oprea' },
--               'saint7', { name = 'Ilie Lăcătuşu' } ))

-- ttyrant.table:get() - single key
local vabc = assert(tt:get('abc'))
local v123 = assert(tt:get('123'))
assert(tonumber(vabc['a']) == 1.23)
assert(tonumber(v123['3']) == 3.33)

---- ttyrant.table:get() - multiple keys given as table
--local vmtr = assert(tt:get({ 'martyr1', 'martyr2', 'martyr3', 'martyr4', 'martyr5', 'martyr6', 'martyr7' }))
--assert(type(vmtr.martyr1) ~= 'table')
--assert(type(vmtr.martyr2) ~= 'table')
--assert(type(vmtr.martyr3) ~= 'table')
--assert(type(vmtr.martyr4) ~= 'table')
--assert(type(vmtr.martyr5) ~= 'table')
--assert(type(vmtr.martyr6) ~= 'table')
--assert(type(vmtr.martyr7) ~= 'table')
--assert(vmtr.martyr1.name == 'Valeriu Gafencu')
--assert(vmtr.martyr2.name == 'Radu Gyr')
--assert(vmtr.martyr3.name == 'Virgil Maxim')
--assert(vmtr.martyr4.name == 'Vasile Militaru')
--assert(vmtr.martyr5.name == 'Costache Oprişan')
--assert(vmtr.martyr6.name == 'Ioan Ianolide')
--assert(vmtr.martyr7.name == 'Corneliu Zelea Codreanu')

---- ttyrant.table:get() - multiple keys given as list
--local vsnt = assert(tt:get('saint1', 'saint2', 'saint3', 'saint4', 'saint5', 'saint6', 'saint7'))
--assert(type(vsnt.saint1) ~= 'table')
--assert(type(vsnt.saint2) ~= 'table')
--assert(type(vsnt.saint3) ~= 'table')
--assert(type(vsnt.saint4) ~= 'table')
--assert(type(vsnt.saint5) ~= 'table')
--assert(type(vsnt.saint6) ~= 'table')
--assert(type(vsnt.saint7) ~= 'table')
--assert(vsnt.saint1.name == 'Gheorghe')
--assert(vsnt.saint2.name == 'Anastasia')
--assert(vsnt.saint3.name == 'Dimitrie')
--assert(vsnt.saint4.name == 'Antonie')
--assert(vsnt.saint5.name == 'Filofteia')
--assert(vsnt.saint6.name == 'Oprea')
--assert(vsnt.saint7.name == 'Ilie Lăcătuşu')

-- ttyrant.table:out()
assert(tt:out('123'))
assert(not tt:get('123'))

-- ttyrant.table:add()
assert(tt:add('abc',  3) == 3)
assert(tt:add('abc',  3) == 6)
assert(tt:add('abc', -1) == 5)
assert(tt:add('abc',  3) == 8)

-- ttyrant.table:close()
assert(tt:close())


--
-- Success.
--
print('All tests completed.')
