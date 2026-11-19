const crypto = require('node:crypto');
const fs = require('node:fs/promises');
const path = require('node:path');

const ARTIFACTS = [
    ['gdn-package-native-release-onnx', 'gigaedge-enhance-linux-onnx'],
    ['gdn-package-native-release-tflite', 'gigaedge-enhance-linux-tflite'],
    ['gdn-install-macos-arm64-release-onnx', 'gigaedge-enhance-macos-arm64-onnx'],
    ['gdn-install-macos-arm64-release-tflite', 'gigaedge-enhance-macos-arm64-tflite'],
    ['gdn-install-macos-x86_64-release-onnx', 'gigaedge-enhance-macos-x86_64-onnx'],
    ['gdn-install-macos-x86_64-release-tflite', 'gigaedge-enhance-macos-x86_64-tflite'],
    ['gdn-install-windows-x64-release-onnx', 'gigaedge-enhance-windows-x64-onnx'],
    ['gdn-install-windows-x64-release-tflite', 'gigaedge-enhance-windows-x64-tflite'],
    ['gdn-android', 'gigaedge-enhance-android'],
    ['gdn-ios', 'gigaedge-enhance-ios'],
    ['gdn-web-sdk', 'gigaedge-enhance-web-sdk'],
];

function asBuffer(data) {
    if (Buffer.isBuffer(data)) {
        return data;
    }
    if (data instanceof ArrayBuffer || ArrayBuffer.isView(data)) {
        return Buffer.from(data.buffer ?? data, data.byteOffset ?? 0, data.byteLength);
    }
    if (typeof data === 'string') {
        return Buffer.from(data, 'binary');
    }
    throw new TypeError('GitHub returned an unsupported binary response.');
}

function assertReleaseSource(run, tagSha, defaultBranch) {
    if (run.conclusion !== 'success' || run.event !== 'push' || run.head_branch !== defaultBranch) {
        throw new Error(`CI run ${run.id} is not a successful ${defaultBranch} push run.`);
    }
    if (run.head_sha !== tagSha) {
        throw new Error(`CI run ${run.id} tested ${run.head_sha}, but the release tag points to ${tagSha}.`);
    }
}

function findReleaseByTag(releases, releaseTag) {
    const release = releases.find(item => item.tag_name === releaseTag);
    if (!release) {
        throw new Error(`Release ${releaseTag} was not found.`);
    }
    return release;
}

async function promoteReleaseAssets({ github, context, core, ciRunId, releaseTag, outputDirectory = 'release-assets' }) {
    if (!/^\d+$/.test(ciRunId)) {
        throw new Error(`Invalid CI run ID: ${ciRunId}`);
    }
    if (!/^v\d+\.\d+\.\d+(?:[-+][0-9A-Za-z.-]+)?$/.test(releaseTag)) {
        throw new Error(`Invalid release tag: ${releaseTag}`);
    }

    const { owner, repo } = context.repo;
    const defaultBranch = context.payload.repository?.default_branch ?? 'master';
    const runId = Number(ciRunId);
    const [{ data: run }, { data: tagCommit }] = await Promise.all([
        github.rest.actions.getWorkflowRun({ owner, repo, run_id: runId }),
        github.rest.repos.getCommit({ owner, repo, ref: releaseTag }),
    ]);
    assertReleaseSource(run, tagCommit.sha, defaultBranch);

    const { data: artifactResponse } = await github.rest.actions.listWorkflowRunArtifacts({
        owner,
        repo,
        run_id: runId,
        per_page: 100,
    });
    await fs.mkdir(outputDirectory, { recursive: true });

    const files = [];
    for (const [source, destination] of ARTIFACTS) {
        const expectedName = `${source}-${run.head_sha}`;
        const artifact = artifactResponse.artifacts.find(item => item.name === expectedName && !item.expired);
        if (!artifact) {
            throw new Error(`Required CI artifact ${expectedName} was not found.`);
        }

        const response = await github.rest.actions.downloadArtifact({
            owner,
            repo,
            artifact_id: artifact.id,
            archive_format: 'zip',
        });
        const filename = `${destination}-${releaseTag}.zip`;
        const data = asBuffer(response.data);
        await fs.writeFile(path.join(outputDirectory, filename), data);
        files.push({ filename, data });
        core.info(`Downloaded ${expectedName} as ${filename}.`);
    }

    const checksumContents = files
        .map(({ filename, data }) => `${crypto.createHash('sha256').update(data).digest('hex')}  ${filename}`)
        .sort()
        .join('\n') + '\n';
    const checksumFile = { filename: 'SHA256SUMS', data: Buffer.from(checksumContents) };
    await fs.writeFile(path.join(outputDirectory, checksumFile.filename), checksumFile.data);
    files.push(checksumFile);

    const releases = await github.paginate(github.rest.repos.listReleases, { owner, repo, per_page: 100 });
    const release = findReleaseByTag(releases, releaseTag);
    for (const { filename, data } of files) {
        const existing = release.assets.find(asset => asset.name === filename);
        if (existing) {
            const response = await github.request('GET /repos/{owner}/{repo}/releases/assets/{asset_id}', {
                owner,
                repo,
                asset_id: existing.id,
                headers: { accept: 'application/octet-stream' },
            });
            if (!asBuffer(response.data).equals(data)) {
                throw new Error(`Release asset ${filename} already exists with different contents.`);
            }
            core.info(`Release asset ${filename} already exists with identical contents.`);
            continue;
        }

        await github.rest.repos.uploadReleaseAsset({
            owner,
            repo,
            release_id: release.id,
            name: filename,
            data,
            headers: {
                'content-type': 'application/octet-stream',
                'content-length': data.length,
            },
        });
        core.info(`Uploaded release asset ${filename}.`);
    }

    await github.rest.repos.updateRelease({ owner, repo, release_id: release.id, draft: false });
    core.info(`Published ${releaseTag}.`);
}

module.exports = { ARTIFACTS, asBuffer, assertReleaseSource, findReleaseByTag, promoteReleaseAssets };
