--
-- gh-6036: verify that terms are locked when we're inside journal
-- write routine, because parallel appliers may ignore the fact that
-- the term is updated already but not yet written leading to data
-- inconsistency.
--
test_run = require('test_run').new()

SERVERS={"election_replica1", "election_replica2", "election_replica3"}
test_run:create_cluster(SERVERS, "replication", {args='1 nil manual 1'})
test_run:wait_fullmesh(SERVERS)

--
-- Create a synchro space on the master node and make
-- sure the write processed just fine.
test_run:switch("election_replica1")
box.ctl.promote()
s = box.schema.create_space('test', {is_sync = true})
_ = s:create_index('pk')
s:insert{1}

test_run:wait_lsn('election_replica2', 'election_replica1')
test_run:wait_lsn('election_replica3', 'election_replica1')

--
-- Drop connection between election_replica1 and election_replica2.
box.cfg({                                   \
    replication = {                         \
        "unix/:./election_replica1.sock",   \
        "unix/:./election_replica3.sock",   \
    },                                      \
})

--
-- Drop connection between election_replica2 and election_replica1.
test_run:switch("election_replica2")
box.cfg({                                   \
    replication = {                         \
        "unix/:./election_replica2.sock",   \
        "unix/:./election_replica3.sock",   \
    },                                      \
})

--
-- Here we have the following scheme
--
--              election_replica3 (will be delayed)
--              /                \
--    election_replica1    election_replica2

--
-- Initiate disk delay in a bit tricky way: the next write will
-- fall into forever sleep.
test_run:switch("election_replica3")
box.error.injection.set("ERRINJ_WAL_DELAY", true)
--
-- Make election_replica2 been a leader and start writting data,
-- the PROMOTE request get queued on election_replica3 and not
-- yet processed, same time INSERT won't complete either
-- waiting for PROMOTE completion first. Note that we
-- enter election_replica3 as well just to be sure the PROMOTE
-- reached it.
test_run:switch("election_replica2")
box.ctl.promote()
test_run:switch("election_replica3")
test_run:wait_cond(function()                   \
    return box.info.synchro.queue.latched       \
end)
test_run:switch("election_replica2")
box.space.test:insert{2}

--
-- The election_replica1 node has no clue that there is a new leader
-- and continue writing data with obsolete term. Since election_replica3
-- is delayed now the INSERT won't proceed yet but get queued.
test_run:switch("election_replica1")
box.space.test:insert{3}

--
-- Finally enable election_replica3 back. Make sure the data from new
-- election_replica2 leader get writing while old leader's data ignored.
test_run:switch("election_replica3")
box.error.injection.set('ERRINJ_WAL_DELAY', false)
test_run:wait_cond(function() return box.space.test:get{2} ~= nil end)
box.space.test:select{}

test_run:switch("default")
test_run:cmd('stop server election_replica1')
test_run:cmd('stop server election_replica2')
test_run:cmd('stop server election_replica3')

test_run:cmd('delete server election_replica1')
test_run:cmd('delete server election_replica2')
test_run:cmd('delete server election_replica3')
