local worker1 = require "lworker"
worker1.start "print('hello world!')"
print("--------------------------1")
worker1.send("chan1", "ping")
print("--------------------------2")
local worker2 = require "lworker"
worker2.start "print('helle lua')"
worker2.recv("chan1", "pong");

