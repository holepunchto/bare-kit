const path = require('bare-path')
const fs = require('bare-fs')
const crypto = require('bare-crypto')
const { fileURLToPath, pathToFileURL } = require('bare-url')
const URL = require('bare-url')
const Bundle = require('bare-bundle')
const Module = require('bare-module')
const { startsWithWindowsDriveLetter } = require('bare-module-resolve')
const SystemLog = require('bare-system-logger')
const Console = require('bare-console')
const { Duplex } = require('bare-stream')
const unpack = require('bare-unpack')

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

exports.start = async function start(filename, source, assets) {
  if (assets !== null) assets = fileURLToPath(toURL(assets))

  const url = toURL(filename)

  if (source === null) source = protocol.readSync(url)
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

  return Module.loadSync(url, source, { protocol })
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
