#!/usr/bin/env lua


--
-- ttyrant-test.lua: A test-set for lua-ttyrant binding.
--
-- Copyright (C)2010 by Valeriu Palos. All rights reserved.
-- This library is under the BSD license (see README file).
--


--
-- Import API.
--
local ttyrant = require'ttyrant'


--
-- Hash database tests.
--

-- ttyrant:open()
local th = assert(ttyrant:open{ host ='localhost', port = 1978 })

-- ttyrant:put()
assert(th:put('Key1', 'NIKA'))
assert(th:put('Key2', 'ICXC'))

-- ttyrant:put() - multiple keys at once given as table
assert(th:put{ martyr1 = 'Valeriu Gafencu',
               martyr2 = 'Radu Gyr',
               martyr3 = 'Virgil Maxim',
               martyr4 = 'Vasile Militaru',
               martyr5 = 'Costache Oprişan',
               martyr6 = 'Ioan Ianolide',
               martyr7 = 'Corneliu Zelea Codreanu' })

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
assert(th:out{ 'saint1', 'saint4', 'martyr6' })
assert(not th:get('saint1'))
assert(not th:get('saint4'))
assert(not th:get('martyr6'))

-- ttyrant:out() - multiple keys at once given as list
assert(th:out('martyr1', 'martyr4', 'saint6'))
assert(not th:get('martyr1'))
assert(not th:get('martyr4'))
assert(not th:get('saint6'))

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

-- ttyrant.table:open()
local tt = assert(ttyrant.table:open{ host ='localhost', port = 1979 })

-- ttyrant.table:put()
assert(tt:put('abc', { a = 1.23, b = 4.56, c = 7.89 }))
assert(tt:put('123', { 1.11, 2.22, 3.33 }))

-- ttyrant.table:get()
local vabc = assert(tt:get('abc'))
local v123 = assert(tt:get('123'))
assert(tonumber(vabc['a']) == 1.23)
assert(tonumber(v123['3']) == 3.33)

-- ttyrant.table:out()
assert(tt:out('123'))
assert(not tt:get('123'))

-- ttyrant.table:add()
assert(tt:add('abc',  3) == 3)
assert(tt:add('abc',  3) == 6)
assert(tt:add('abc', -1) == 5)
assert(tt:add('abc',  3) == 8)


--
-- Query tests.
--

-- ttyrant.query:new()
local qr = assert(ttyrant.query:new(tt))

-- ttyrant.query:add_condition()
assert(qr:add_condition('b', 'RDBQCNUMGE', '4.56'))
assert(qr:add_condition('c', 'RDBQCNUMLT', '7.90'))

-- ttyrant.query:search()
local result = assert(qr:search())
assert(#result == 1)

-- ttyrant.query:delete()
assert(qr:delete())


--
-- Success.
--
print('All tests completed successfully.')
