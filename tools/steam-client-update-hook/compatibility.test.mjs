import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { verifyPinnedBuild, PINNED_FILES } from './compatibility.mjs';
import { SUPPORTED_BUILD } from './backend.mjs';

async function fixture(t) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(),'ugm-update-pins-'));
  t.after(()=>fs.rm(root,{recursive:true,force:true}));
  const pins = {};
  for (const name of Object.keys(PINNED_FILES)) {
    await fs.mkdir(path.dirname(path.join(root,name)),{recursive:true});
    const content = 'fixture '+name;
    await fs.writeFile(path.join(root,name),content);
    pins[name] = createHash('sha256').update(content).digest('hex');
  }
  return {root,pins};
}
test('Only the complete known UI/native/bootstrap file set yields a verified build', async t => {
  const {root,pins}=await fixture(t);
  assert.equal(await verifyPinnedBuild(root,pins),SUPPORTED_BUILD);
  await assert.rejects(verifyPinnedBuild(root),/huella no compatible/);
});
test('Every changed native/UI/bootstrap file is rejected, even if the other files match', async t => {
  for (const name of Object.keys(PINNED_FILES)) {
    const {root,pins}=await fixture(t);
    await fs.appendFile(path.join(root,name),'changed');
    await assert.rejects(verifyPinnedBuild(root,pins),/huella no compatible/);
  }
});
test('Missing client file fails closed, not a version-zero bypass', async t => {
  const {root,pins}=await fixture(t);
  await fs.rename(path.join(root,'linux64/steamclient.so'),path.join(root,'linux64/moved.so'));
  await assert.rejects(verifyPinnedBuild(root,pins),{code:'ENOENT'});
});
