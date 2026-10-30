const path = require('bare-path')
const fs = require('bare-fs')
const crypto = require('bare-crypto')
const { pathToFileURL } = require('bare-url')
const URL = require('bare-url')
const Module = require('bare-module')
const { startsWithWindowsDriveLetter } = require('bare-module-resolve')
const SystemLog = require('bare-system-logger')
const Console = require('bare-console')
const { Duplex } = require('bare-stream')

const { protocol } = module

global.console = new Console(new SystemLog())

// A signal from the queue means re-check both reads and a pending write.
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

    if (n === null) cb(IPC_CLOSED())
    else if (n < 0) this._pendingWrite = { chunk, cb }
    else if (n < chunk.byteLength) this._pendingWrite = { chunk: chunk.subarray(n), cb }
    else cb(null)
  }

  _retry() {
    if (this._pendingWrite === null) return

    const { chunk, cb } = this._pendingWrite
    this._pendingWrite = null

    this._send(chunk, cb)
  }

  // A half-close: the host reads EOF but may keep writing. streamx only calls
  // _final once pending writes have drained.
  _final(cb) {
    this._native.end()
    cb(null)
  }

  // Lets the worklet exit once both ends are done, as the queue holds the loop
  // open until then.
  _destroy(err, cb) {
    this._native.unref()
    cb(null)
  }

  _onsignal() {
    this._retry()
    this._drain()
  }

  ref() {
    if (!this.destroying) this._native.ref()
    return this
  }

  unref() {
    this._native.unref()
    return this
  }
}

// Returns the callback the worklet loop invokes when the queue signals.
exports.openIPC = function openIPC(native) {
  const ipc = new IPCStream(native)

  Bare.IPC = ipc

  Bare.on('suspend', () => ipc.unref()).on('resume', () => ipc.ref())

  return () => ipc._onsignal()
}

exports.start = function start(filename, source, resources, readResource) {
  if (resources !== null && isRelative(filename)) {
    if (readResource !== null) {
      extract(resources, filename, readResource)

      if (source === null) source = readResource(filename)
    }

    filename = path.join(resources, filename)
  }

  const url = toURL(filename)

  if (source === null) source = protocol.readSync(url)
  else source = Buffer.from(source)

  return Module.loadSync(url, source, { protocol })
}

// The listing is extracted last, so that its presence means that every
// resource it lists has been.
function extract(resources, filename, readResource) {
  const name = filename.slice(0, filename.length - path.extname(filename).length)

  const listing = path.join(resources, name + '.assets')

  if (fs.existsSync(listing)) return

  const keys = readResource(name + '.assets')

  if (keys === null) return

  for (const key of Buffer.from(keys).toString().split('\n')) {
    if (key === '') continue

    const data = readResource(name + key)

    if (data === null) throw new Error(`Resource '${name + key}' is listed but missing`)

    write(path.join(resources, name + key), data)
  }

  write(listing, keys)
}

// Worklets may extract the same resources at once, so each file is moved into
// place whole.
function write(target, data) {
  const tmp = target + '.' + crypto.randomBytes(8).toString('hex')

  fs.mkdirSync(path.dirname(target), { recursive: true })
  fs.writeFileSync(tmp, Buffer.from(data))
  fs.renameSync(tmp, target)
}

function isRelative(filename) {
  return !path.isAbsolute(filename) && URL.parse(filename) === null
}

function toURL(pathOrURL) {
  if (startsWithWindowsDriveLetter(pathOrURL)) return pathToFileURL(pathOrURL)

  return URL.parse(pathOrURL) || pathToFileURL(pathOrURL)
}

function IPC_CLOSED() {
  const err = new Error('IPC stream closed')
  err.code = 'IPC_CLOSED'
  return err
}
