const path = require('bare-path')
const fs = require('bare-fs')
const crypto = require('bare-crypto')
const { fileURLToPath, pathToFileURL } = require('bare-url')
const EventEmitter = require('bare-events')
const URL = require('bare-url')
const Bundle = require('bare-bundle')
const Module = require('bare-module')
const { startsWithWindowsDriveLetter } = require('bare-module-resolve')
const { SystemLog } = require('bare-logger')
const Console = require('bare-console')
const IPC = require('bare-ipc')
const unpack = require('bare-unpack')

const ports = IPC.open()

global.console = new Console(new SystemLog())

class BareKit extends EventEmitter {
  constructor() {
    super()

    this.IPC = new IPC(ports[0])
  }

  [Symbol.for('bare.inspect')]() {
    return {
      __proto__: { constructor: BareKit }
    }
  }
}

exports.BareKit = new BareKit()
exports.port = ports[1]

Object.defineProperty(global, 'BareKit', {
  value: exports.BareKit,
  enumerable: true,
  writable: false,
  configurable: true
})

exports.push = function push(payload, reply) {
  if (exports.BareKit.emit('push', Buffer.from(payload), replyOnce) === false) {
    replyOnce(null, null)
  }

  function replyOnce(err, payload, encoding) {
    reply(
      err,
      typeof payload === 'string' ? Buffer.from(payload, encoding) : payload
    )

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
  else source = Buffer.concat([Buffer.from(source)])

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
