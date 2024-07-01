const IPC = require('bare-ipc')
const RPC = require('bare-rpc')

const ports = IPC.open()

class BareKit {
  constructor () {
    this.IPC = new IPC(ports[0])
  }

  get RPC () {
    return RPC
  }

  [Symbol.for('bare.inspect')] () {
    return {
      __proto__: { constructor: BareKit }
    }
  }
}

Object.defineProperty(global, 'BareKit', {
  value: new BareKit(),
  enumerable: true,
  writable: false,
  configurable: true
})

module.exports = ports[1]
