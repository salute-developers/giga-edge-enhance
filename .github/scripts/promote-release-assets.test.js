const assert = require('node:assert/strict');
const test = require('node:test');

const { ARTIFACTS, asBuffer, assertReleaseSource, findReleaseByTag } = require('./promote-release-assets');

test('release source requires a successful default-branch push at the tag commit', () => {
    const run = {
        id: 42,
        conclusion: 'success',
        event: 'push',
        head_branch: 'master',
        head_sha: 'abc123',
    };

    assert.doesNotThrow(() => assertReleaseSource(run, 'abc123', 'master'));
    assert.throws(() => assertReleaseSource({ ...run, conclusion: 'failure' }, 'abc123', 'master'));
    assert.throws(() => assertReleaseSource({ ...run, event: 'pull_request' }, 'abc123', 'master'));
    assert.throws(() => assertReleaseSource({ ...run, head_branch: 'feature' }, 'abc123', 'master'));
    assert.throws(() => assertReleaseSource(run, 'different', 'master'));
});

test('binary API responses are normalized without changing bytes', () => {
    const expected = Buffer.from([0, 1, 127, 128, 255]);
    assert.deepEqual(asBuffer(expected), expected);
    assert.deepEqual(asBuffer(expected.buffer.slice(expected.byteOffset, expected.byteOffset + expected.byteLength)), expected);
    assert.deepEqual(asBuffer(new Uint8Array(expected)), expected);
});

test('release artifact names are unique', () => {
    assert.equal(new Set(ARTIFACTS.map(([, destination]) => destination)).size, ARTIFACTS.length);
});

test('release lookup includes matching draft releases', () => {
    const draft = { id: 42, tag_name: 'v2.0.0', draft: true, assets: [] };

    assert.equal(findReleaseByTag([draft], 'v2.0.0'), draft);
    assert.throws(() => findReleaseByTag([draft], 'v2.0.1'), /Release v2\.0\.1 was not found/);
});
