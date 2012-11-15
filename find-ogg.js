
/**
 * This script is part of the build process.
 * We need to figure out the location of "node-ogg", so that we
 * can dynamically link to its `ogg.node` file.
 * It's a node script rather than a simple bash script so that it will
 * work on Windows.
 */

var path = require('path');

// we depend on node's module lookup algorithm
console.log(path.dirname(require.resolve('ogg')));
