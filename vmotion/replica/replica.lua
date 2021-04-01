-- файл экземпляра для реплики
box.cfg{
  listen = 3301,
  replication = {'replicator:password@172.17.0.2:3301',  -- URI мастера
                 'replicator:password@172.17.0.3:3301'}, -- URI реплики
  read_only = true
}
box.once("schema", function()
  box.schema.user.create('replicator', {password = 'password'})
  box.schema.user.grant('replicator', 'replication') -- настроить роль для репликации
  box.schema.space.create("test")
  box.space.test:create_index("primary")
  print('box.once executed on replica')
end)
