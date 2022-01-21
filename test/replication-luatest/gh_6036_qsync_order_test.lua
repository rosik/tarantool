local t = require('luatest')
local cluster = require('test.luatest_helpers.cluster')
local asserts = require('test.luatest_helpers.asserts')
local helpers = require('test.luatest_helpers')
local json = require('json')
local log = require('log')

local g = t.group('gh-6036', {{engine = 'memtx'}, {engine = 'vinyl'}})

g.before_each(function(cg)
    local engine = cg.params.engine

    cg.cluster = cluster:new({})

    local box_cfg = {
        replication = {
            helpers.instance_uri('r1'),
            helpers.instance_uri('r2'),
            helpers.instance_uri('r3'),
        },
        replication_timeout         = 0.1,
        replication_connect_quorum  = 1,
        election_mode               = 'manual',
        election_timeout            = 0.1,
        replication_synchro_quorum  = 1,
        replication_synchro_timeout = 0.1,
        log_level                   = 6,
    }

    cg.r1 = cg.cluster:build_server({alias = 'r1', engine = engine, box_cfg = box_cfg})
    cg.r2 = cg.cluster:build_server({alias = 'r2', engine = engine, box_cfg = box_cfg})
    cg.r3 = cg.cluster:build_server({alias = 'r3', engine = engine, box_cfg = box_cfg})

    cg.cluster:add_server(cg.r1)
    cg.cluster:add_server(cg.r2)
    cg.cluster:add_server(cg.r3)
    cg.cluster:start()
    -- test_run:wait_fullmesh(SERVERS)
end)


g.after_each(function(cg)
    cg.cluster.servers = nil
    cg.cluster:drop()
end)

g.test_qsync_order = function(cg)
    asserts:wait_fullmesh({cg.r1, cg.r2, cg.r3})
    --
    -- Create a synchro space on the master node and make
    -- sure the write processed just fine.
    cg.r1:exec(function()
        box.ctl.promote()
        local s = box.schema.create_space('test', {is_sync = true})
        s:create_index('pk')
        s:insert{1}
    end)

    local vclock = cg.r1:eval("return box.info.vclock")
    vclock[0] = nil
    helpers:wait_vclock(cg.r2, vclock)
    helpers:wait_vclock(cg.r3, vclock)

    --
    -- Drop connection between r1 and r2.
    local repl = json.encode({
        replication = {
            helpers.instance_uri("r1"),
            helpers.instance_uri("r3")
        }
    })
    cg.r1:eval(("box.cfg({replication = %s})"):format(repl.replication))

    --
    -- Drop connection between r2 and r1.
    repl = json.encode({
        replication = {
            helpers.instance_uri("r2"),
            helpers.instance_uri("r3")
        }
    })
    cg.r2:eval(("box.cfg({replication = %s})"):format(repl.replication))

    --
    -- Here we have the following scheme
    --
    --              r3 (will be delayed)
    --              /                \
    --             r1                r2

    --
    -- Initiate disk delay in a bit tricky way: the next write will
    -- fall into forever sleep.
    cg.r3:eval("box.error.injection.set('ERRINJ_WAL_DELAY', true)")

    -- Make election_replica2 been a leader and start writting data,
    -- the PROMOTE request get queued on election_replica3 and not
    -- yet processed, same time INSERT won't complete either
    -- waiting for PROMOTE completion first. Note that we
    -- enter election_replica3 as well just to be sure the PROMOTE
    -- reached it.
    cg.r2:eval("box.ctl.promote()")
    t.helpers.retrying({}, function()
        return cg.r2:eval("return box.info.synchro.queue.waiters") > 0
    end)
    --cg.r2:eval("box.space.test:insert{2}")

    --
    -- The election_replica1 node has no clue that there is a new leader
    -- and continue writing data with obsolete term. Since election_replica3
    -- is delayed now the INSERT won't proceed yet but get queued.
    --cg.r1:exec(function() box.space.test:insert{3} end)

    --
    -- Finally enable election_replica3 back. Make sure the data from new
    -- election_replica2 leader get writing while old leader's data ignored.
    cg.r3:eval("box.error.injection.set('ERRINJ_WAL_DELAY', false)")
    --t.helpers.retrying({}, function()
    --    return cg.r3:eval("return box.space.test:get{2}") ~= nil
    --end)

    -- --
    -- -- gh-6036: verify that terms are locked when we're inside journal
    -- -- write routine, because parallel appliers may ignore the fact that
    -- -- the term is updated already but not yet written leading to data
    -- -- inconsistency.
    -- --
    -- test_run = require('test_run').new()
    -- 
    -- SERVERS={"election_replica1", "election_replica2", "election_replica3"}
    -- test_run:create_cluster(SERVERS, "replication", {args='1 nil manual 1'})
    -- test_run:wait_fullmesh(SERVERS)
    -- 
    -- --
    -- -- Create a synchro space on the master node and make
    -- -- sure the write processed just fine.
    -- test_run:switch("election_replica1")
    -- box.ctl.promote()
    -- s = box.schema.create_space('test', {is_sync = true})
    -- _ = s:create_index('pk')
    -- s:insert{1}
    -- 
    -- test_run:wait_lsn('election_replica2', 'election_replica1')
    -- test_run:wait_lsn('election_replica3', 'election_replica1')
    -- 
    -- --
    -- -- Drop connection between election_replica1 and election_replica2.
    -- box.cfg({                                   \
    --     replication = {                         \
    --         "unix/:./election_replica1.sock",   \
    --         "unix/:./election_replica3.sock",   \
    --     },                                      \
    -- })
    -- 
    -- --
    -- -- Drop connection between election_replica2 and election_replica1.
    -- test_run:switch("election_replica2")
    -- box.cfg({                                   \
    --     replication = {                         \
    --         "unix/:./election_replica2.sock",   \
    --         "unix/:./election_replica3.sock",   \
    --     },                                      \
    -- })
    -- 
    -- --
    -- -- Here we have the following scheme
    -- --
    -- --              election_replica3 (will be delayed)
    -- --              /                \
    -- --    election_replica1    election_replica2
    -- 
    -- --
    -- -- Initiate disk delay in a bit tricky way: the next write will
    -- -- fall into forever sleep.
    -- test_run:switch("election_replica3")
    -- box.error.injection.set("ERRINJ_WAL_DELAY", true)
    -- --
    -- -- Make election_replica2 been a leader and start writting data,
    -- -- the PROMOTE request get queued on election_replica3 and not
    -- -- yet processed, same time INSERT won't complete either
    -- -- waiting for PROMOTE completion first. Note that we
    -- -- enter election_replica3 as well just to be sure the PROMOTE
    -- -- reached it.
    -- test_run:switch("election_replica2")
    -- box.ctl.promote()
    -- test_run:switch("election_replica3")
    -- test_run:wait_cond(function()                   \
    --     return box.info.synchro.queue.waiters > 0   \
    -- end)
    -- test_run:switch("election_replica2")
    -- box.space.test:insert{2}
    -- 
    -- --
    -- -- The election_replica1 node has no clue that there is a new leader
    -- -- and continue writing data with obsolete term. Since election_replica3
    -- -- is delayed now the INSERT won't proceed yet but get queued.
    -- test_run:switch("election_replica1")
    -- box.space.test:insert{3}
    -- 
    -- --
    -- -- Finally enable election_replica3 back. Make sure the data from new
    -- -- election_replica2 leader get writing while old leader's data ignored.
    -- test_run:switch("election_replica3")
    -- box.error.injection.set('ERRINJ_WAL_DELAY', false)
    -- test_run:wait_cond(function() return box.space.test:get{2} ~= nil end)
    -- box.space.test:select{}
    -- 
    -- test_run:switch("default")
    -- test_run:cmd('stop server election_replica1')
    -- test_run:cmd('stop server election_replica2')
    -- test_run:cmd('stop server election_replica3')
    -- 
    -- test_run:cmd('delete server election_replica1')
    -- test_run:cmd('delete server election_replica2')
    -- test_run:cmd('delete server election_replica3')
end
