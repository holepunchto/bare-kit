const fs = require('fs')
const path = require('path')
const Bundle = require('bare-bundle')

const bundle = new Bundle()
  .write('/foo.js', 'console.log(require.asset(\'./bar.txt\'))\n', { main: true })
  .write('/bar.txt', 'hello world.txt\n', { asset: true })
  .toBuffer()

fs.writeFileSync(path.join(__dirname, 'assets.bundle'), bundle)
