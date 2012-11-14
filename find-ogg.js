
/**
 * This script is part of the build process.
 * We need to figure out the location of "node-ogg", so that we
 * can dynamically link to its `ogg.node` file.
 * It's a node script rather than a simple bash script so that it will
 * work on Windows.
 */

var fs = require('fs');
var path = require('path');
var module_root_dir = process.argv[2] || process.cwd();

try {
  // first try to depend on node's module lookup algorithm
  console.log(path.dirname(require.resolve('ogg')));
  process.exit(0);
} catch (e) {
  // ignore
}

// next check if a "node-ogg" dir exists in the parent dir.
// this would imply a git clone, i.e. local development of the module.
var devPath = path.join(module_root_dir, '..', 'node-ogg');
try {
  var stat = fs.statSync(devPath);
  if (stat.isDirectory()) {
    console.log(devPath);
    process.exit(0);
  }
} catch (e) {
  if (e.code != 'ENOENT') throw err;
}

// fail :(
throw new Error('could not find "node-ogg" installation!');
