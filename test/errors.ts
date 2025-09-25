import noobs from '../index.js';
const assert = require('assert');

async function test() {
  let errMsg = '';
  try {
    noobs.GetSourceSettings('Test Source');
  } catch (e: any) {
    errMsg = e.message;
  }
  assert.equal(errMsg, 'Obs not initialized');
  console.log('Test Done');
}

console.log('Starting test...');
test();
console.log('Test now running async');
