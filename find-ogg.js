
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
var dirs = [
  // an "ogg" dir in the node_modules dir (direct dependency)
  path.join(module_root_dir, 'node_modules', 'ogg'),
  // an "ogg" dir in the parent dir (node_modules, shared dependency)
  path.join(module_root_dir, '..', 'ogg'),
  // a "node-ogg" git clone in the parent dir (developemnt, etc.)
  path.join(module_root_dir, '..', 'node-ogg')
];
dirs.forEach(function (dir) {
  try {
    var stat = fs.statSync(dir);
    if (stat.isDirectory()) {
      console.log(dir);
      process.exit(0);
    }
  } catch (e) {
    if (e.code != 'ENOENT') throw err;
  }
});
throw new Error('could not find "node-ogg" installation!');
