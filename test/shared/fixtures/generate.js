const fs = require('fs')
const path = require('path')
const Bundle = require('bare-bundle')

const bundle = new Bundle()
  .write(
    '/foo.js',
    "console.log(require.asset('./bar.txt'), require.asset('./dir/baz.txt'))\n",
    {
      main: true,
      imports: {
        './bar.txt': '/bar.txt',
        './dir/baz.txt': '/dir/baz.txt'
      }
    }
  )
  .write('/bar.txt', 'hello world\n', { asset: true })
  .write('/dir/baz.txt', 'hello world\n', { asset: true })

bundle.id = 'bundle-id'

fs.writeFileSync(path.join(__dirname, 'assets.bundle'), bundle.toBuffer())
