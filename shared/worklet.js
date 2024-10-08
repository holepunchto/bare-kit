/* global Bare */
const path = require('bare-path')
const fs = require('bare-fs')
const crypto = require('bare-crypto')
const { fileURLToPath, pathToFileURL } = require('bare-url')
const EventEmitter = require('bare-events')
const IPC = require('bare-ipc')
const RPC = require('bare-rpc')
const URL = require('bare-url')
const Bundle = require('bare-bundle')
const Module = require('bare-module')
const { startsWithWindowsDriveLetter } = require('bare-module-resolve')

const isWindows = Bare.platform === 'win32'

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
  if (exports.BareKit.emit('push', Buffer.from(payload), replyOnce) === false) {
    replyOnce(null, null)
  }

  function replyOnce (err, payload, encoding) {
    reply(err, typeof payload === 'string' ? Buffer.from(payload, encoding) : payload)

    reply = noop
  }
}

exports.start = function start (filename, source, assets) {
  if (assets) assets = path.resolve(assets)

  const protocol = Module.protocol.extend({ asset })

  let url

  if (startsWithWindowsDriveLetter(filename)) {
    url = null
  } else {
    url = URL.parse(filename)
  }

  if (url === null) url = URL.pathToFileURL(filename)

  if (source === null) source = protocol.read(url)

  if (assets && path.extname(url.href) === '.bundle') {
    const bundle = Bundle.from(source)

    if (bundle.id === null) assets = null
    else {
      const id = crypto.createHash('blake2b256').update(bundle.id).digest('hex')

      const root = path.join(assets, id)

      if (fs.existsSync(root)) assets = root
      else {
        const tmp = path.join(assets, 'tmp')

        fs.rmSync(tmp, { recursive: true, force: true })

        fs.mkdirSync(tmp, { recursive: true })

        for (const asset of bundle.assets) {
          fs.writeFileSync(path.join(tmp, asset), bundle.read(asset))
        }

        fs.renameSync(tmp, root)

        assets = root
      }
    }
  }

  const root = urlToPath(url)

  return Module.load(url, source, { protocol })

  function asset (context, url) {
    if (assets) url = pathToFileURL(path.join(assets, path.relative(root, urlToPath(url))))

    return url
  }
}

Object.defineProperty(global, 'BareKit', {
  value: exports.BareKit,
  enumerable: true,
  writable: false,
  configurable: true
})

function urlToPath (url) {
  if (url.protocol === 'file:') return fileURLToPath(url)

  if (isWindows) {
    if (/%2f|%5c/i.test(url.pathname)) {
      throw new Error('The URL path must not include encoded \\ or / characters')
    }
  } else {
    if (/%2f/i.test(url.pathname)) {
      throw new Error('The URL path must not include encoded / characters')
    }
  }

  return decodeURIComponent(url.pathname)
}

function noop () {}
