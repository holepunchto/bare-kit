const path = require('bare-path')
const fs = require('bare-fs')
const crypto = require('bare-crypto')
const { fileURLToPath, pathToFileURL } = require('bare-url')
const EventEmitter = require('bare-events')
const URL = require('bare-url')
const Bundle = require('bare-bundle')
const Module = require('bare-module')
const { startsWithWindowsDriveLetter } = require('bare-module-resolve')
const SystemLog = require('bare-system-logger')
const Console = require('bare-console')
const { Duplex } = require('bare-stream')
const unpack = require('bare-unpack')

global.console = new Console(new SystemLog())

let ipc = null

// A Duplex over the native in-process queue. The native side provides
// non-blocking read/write plus ref/unref; a single signal (fed back from the
// worklet loop) means re-check both reads and a pending write.
class IPCStream extends Duplex {
  constructor(native) {
    super()

    this._native = native
    this._reading = false
    this._pendingWrite = null
  }

  _read() {
    this._reading = true
    this._drain()
  }

  _drain() {
    if (!this._reading) return

    let chunk
    while ((chunk = this._native.read()) !== undefined) {
      if (chunk === null) {
        this._reading = false
        this.push(null)
        return
      }

      if (this.push(Buffer.from(chunk)) === false) {
        this._reading = false
        return
      }
    }
  }

  _write(chunk, encoding, cb) {
    this._send(Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk, encoding), cb)
  }

  _send(chunk, cb) {
    const n = this._native.write(chunk)

    if (n < 0) this._pendingWrite = { chunk, cb }
    else if (n < chunk.byteLength) this._pendingWrite = { chunk: chunk.subarray(n), cb }
    else cb(null)
  }

  _retry() {
    if (this._pendingWrite === null) return

    const { chunk, cb } = this._pendingWrite
    this._pendingWrite = null

    this._send(chunk, cb)
  }

  // end() closes our end of the queue so the host read reaches EOF. streamx
  // only calls _final once outstanding writes have drained, so ordering holds.
  _final(cb) {
    this._native.close()
    cb(null)
  }

  _onsignal() {
    this._retry()
    this._drain()
  }

  ref() {
    this._native.ref()
    return this
  }

  unref() {
    this._native.unref()
    return this
  }
}

class BareKit extends EventEmitter {
  constructor() {
    super()

    this.IPC = null
  }

  [Symbol.for('bare.inspect')]() {
    return {
      __proto__: { constructor: BareKit }
    }
  }
}

exports.BareKit = new BareKit()

Object.defineProperty(global, 'BareKit', {
  value: exports.BareKit,
  enumerable: true,
  writable: false,
  configurable: true
})

// Called by the host runtime with the native queue endpoint before `start`.
// Returns the signal callback the worklet loop invokes when the queue changes.
exports.openIPC = function openIPC(native) {
  ipc = new IPCStream(native)

  Bare.IPC = ipc
  exports.BareKit.IPC = ipc

  Bare.on('suspend', () => ipc.unref()).on('resume', () => ipc.ref())

  return () => ipc._onsignal()
}

exports.push = function push(payload, reply) {
  if (exports.BareKit.emit('push', Buffer.from(payload), replyOnce) === false) {
    replyOnce(null, null)
  }

  function replyOnce(err, payload, encoding) {
    if (err) {
      reply(String(err), null)
    } else {
      reply(null, typeof payload === 'string' ? Buffer.from(payload, encoding) : payload)
    }

    reply = noop
  }
}

exports.start = async function start(filename, source, assets) {
  if (assets !== null) {
    let url

    if (startsWithWindowsDriveLetter(assets)) {
      url = null
    } else {
      url = URL.parse(assets)
    }

    if (url === null) url = pathToFileURL(assets)

    assets = fileURLToPath(url)
  }

  let url

  if (startsWithWindowsDriveLetter(filename)) {
    url = null
  } else {
    url = URL.parse(filename)
  }

  if (url === null) url = pathToFileURL(filename)

  if (source === null) source = Module.protocol.read(url)
  else source = Buffer.from(source)

  if (assets !== null && path.extname(url.href) === '.bundle') {
    const bundle = Bundle.from(source)

    if (bundle.id !== null && bundle.assets.length > 0) {
      const id = crypto.createHash('blake2b256').update(bundle.id).digest('hex')

      const root = path.join(assets, id)

      const tmp = fs.existsSync(root) ? null : path.join(assets, 'tmp')

      if (tmp !== null) {
        fs.rmSync(tmp, { recursive: true, force: true })
        fs.mkdirSync(tmp, { recursive: true })
      }

      source = await unpack(bundle, { files: false, assets: true }, (key) => {
        if (tmp !== null) {
          const target = path.join(tmp, key)

          fs.mkdirSync(path.dirname(target), { recursive: true })
          fs.writeFileSync(target, bundle.read(key))
        }

        return pathToFileURL(path.join(root, key)).href
      })

      if (tmp !== null) fs.renameSync(tmp, root)
    }
  }

  return Module.load(url, source)
}

function noop() {}
