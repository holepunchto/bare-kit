/* global Bare */
const IPC = require('bare-ipc')

const ports = IPC.open()

Bare.IPC = new IPC(ports[0])

module.exports = ports[1]
