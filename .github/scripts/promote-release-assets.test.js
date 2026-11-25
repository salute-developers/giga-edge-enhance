const assert = require('node:assert/strict');
const test = require('node:test');
const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');

const { ARTIFACTS, asBuffer, assertReleaseSource, findReleaseByTag, promoteReleaseAssets } = require('./promote-release-assets');

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

test('release requires all four tested Python platform products', () => {
    const pythonProducts = ARTIFACTS.filter(([name]) => name.startsWith('gdn-python-'));
    assert.deepEqual(pythonProducts.map(([name]) => name).sort(), [
        'gdn-python-linux',
        'gdn-python-macos-arm64',
        'gdn-python-macos-x86_64',
        'gdn-python-windows-x86_64',
    ]);
});

test('missing Python products prevent release uploads and publication', async () => {
    const directory = await fs.mkdtemp(path.join(os.tmpdir(), 'enhance-release-'));
    const run = { id: 42, conclusion: 'success', event: 'push', head_branch: 'master', head_sha: 'abc123' };
    let releaseWrites = 0;
    const github = { rest: {
        actions: {
            getWorkflowRun: async () => ({ data: run }),
            listWorkflowRunArtifacts: async () => ({ data: { artifacts: ARTIFACTS
                .filter(([name]) => !name.startsWith('gdn-python-'))
                .map(([name], id) => ({ id, name: `${name}-abc123`, expired: false })) } }),
            downloadArtifact: async () => ({ data: Buffer.from('fixture') }),
        },
        repos: {
            getCommit: async () => ({ data: { sha: 'abc123' } }),
            uploadReleaseAsset: async () => { releaseWrites++; },
            updateRelease: async () => { releaseWrites++; },
        },
    } };
    try {
        await assert.rejects(promoteReleaseAssets({ github, context: { repo: { owner: 'test', repo: 'test' }, payload: {} },
            core: { info() {} }, ciRunId: '42', releaseTag: 'v2.0.0', outputDirectory: directory }),
        /Required CI artifact gdn-python-linux-abc123 was not found/);
        assert.equal(releaseWrites, 0);
    } finally {
        await fs.rm(directory, { recursive: true, force: true });
    }
});

test('release lookup includes matching draft releases', () => {
    const draft = { id: 42, tag_name: 'v2.0.0', draft: true, assets: [] };

    assert.equal(findReleaseByTag([draft], 'v2.0.0'), draft);
    assert.throws(() => findReleaseByTag([draft], 'v2.0.1'), /Release v2\.0\.1 was not found/);
});
