const EventEmitter = require('bare-events')
const IPC = require('bare-ipc')
const RPC = require('bare-rpc')

const ports = IPC.open()

class BareKit extends EventEmitter {
  constructor () {
    super()

    const ipc = this.IPC = new IPC(ports[0])

    this.RPC = class extends RPC {
      constructor (onrequest) {
        super(ipc, onrequest)
      }
    }
  }

  [Symbol.for('bare.inspect')] () {
    return {
      __proto__: { constructor: BareKit }
    }
  }
}

exports.BareKit = new BareKit()

exports.port = ports[1]

exports.push = function push (payload, reply) {
  if (exports.BareKit.emit('push', Buffer.from(payload), reply) === false) {
    reply(null, null)
  }
}

Object.defineProperty(global, 'BareKit', {
  value: exports.BareKit,
  enumerable: true,
  writable: false,
  configurable: true
})
