-- on replica get current tuples number on "replica - master - replica"
print(#box.space.test:select() .. " " .. c:eval([[ return #box.space.test:select() ]]) .. " " .. #box.space.test:select())
