const fs = require('fs')
const path = require('path')
const Bundle = require('bare-bundle')

const bundle = new Bundle()
  .write('/foo.js', 'console.log(require.asset(\'./bar.txt\'))\n', { main: true })
  .write('/bar.txt', 'hello world.txt\n', { asset: true })

bundle.id = 'bundle-id'

fs.writeFileSync(path.join(__dirname, 'assets.bundle'), bundle.toBuffer())
