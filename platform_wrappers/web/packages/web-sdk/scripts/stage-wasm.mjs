import { createHash } from 'node:crypto';
import { access, copyFile, mkdir, readFile, rename, rm, writeFile } from 'node:fs/promises';
import { dirname, relative, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { execFileSync } from 'node:child_process';

const scriptDir = dirname(fileURLToPath(import.meta.url));
const rootDir = resolve(scriptDir, '../../../../..');
const sdkDir = resolve(scriptDir, '..');
const destinationDir = resolve(sdkDir, 'generated/wasm');
const flavor = process.argv[2];
const verifyOnly = flavor === '--verify';
const expectedFlavor = verifyOnly ? process.argv[3] : undefined;

function gitRevision() {
    try {
        return execFileSync('git', ['rev-parse', 'HEAD'], { cwd: rootDir, encoding: 'utf8' }).trim();
    } catch {
        return 'unknown';
    }
}

async function hashFile(filePath) {
    const contents = await readFile(filePath);

    return createHash('sha256').update(contents).digest('hex');
}

async function assertFile(filePath) {
    try {
        await access(filePath);
    } catch {
        throw new Error(`Required WASM artifact is missing: ${filePath}`);
    }
}

async function resolveBuildDirectory(environmentVariable, presetName, moduleName) {
    if (process.env[environmentVariable]) {
        return resolve(process.env[environmentVariable]);
    }

    const hostBuildDir = resolve(rootDir, `out/build/${presetName}`);
    const containerBuildDir = resolve(rootDir, `out/build/container-clang-llvm-tools/${presetName}`);

    for (const candidate of [hostBuildDir, containerBuildDir]) {
        try {
            await access(resolve(candidate, `platform_wrappers/web/packages/web-sdk/wasm/${moduleName}.js`));
            return candidate;
        } catch {
            // Try the next known build location.
        }
    }

    return hostBuildDir;
}

async function verifyStagedAssets() {
    const manifestPath = resolve(destinationDir, 'manifest.json');
    await assertFile(manifestPath);

    const manifest = JSON.parse(await readFile(manifestPath, 'utf8'));
    if (manifest.schemaVersion !== 1 || !['debug', 'release', 'asan'].includes(manifest.flavor)) {
        throw new Error(`Invalid WASM manifest: ${manifestPath}`);
    }
    if (expectedFlavor && manifest.flavor !== expectedFlavor) {
        throw new Error(`Expected ${expectedFlavor} WASM assets, found ${manifest.flavor}.`);
    }

    for (const artifact of manifest.artifacts) {
        for (const file of artifact.files) {
            const filePath = resolve(destinationDir, file.name);
            await assertFile(filePath);
            if ((await hashFile(filePath)) !== file.sha256) {
                throw new Error(`Staged WASM artifact hash mismatch: ${filePath}`);
            }
        }
    }

    return manifest;
}

async function stageArtifact(tempDir, moduleName, buildDir) {
    const artifactDir = resolve(buildDir, 'platform_wrappers/web/packages/web-sdk/wasm');
    const requiredFiles = [`${moduleName}.js`, `${moduleName}.wasm`, `${moduleName}.d.ts`];
    const optionalFiles = [`${moduleName}.js.map`, `${moduleName}.wasm.map`, `${moduleName}.symbols`];
    const files = [];

    for (const fileName of requiredFiles) {
        const sourcePath = resolve(artifactDir, fileName);
        await assertFile(sourcePath);
        const destinationPath = resolve(tempDir, fileName);
        await copyFile(sourcePath, destinationPath);
        files.push({ name: fileName, sha256: await hashFile(destinationPath) });
    }

    for (const fileName of optionalFiles) {
        const sourcePath = resolve(artifactDir, fileName);
        try {
            await access(sourcePath);
        } catch {
            continue;
        }
        const destinationPath = resolve(tempDir, fileName);
        await copyFile(sourcePath, destinationPath);
        files.push({ name: fileName, sha256: await hashFile(destinationPath) });
    }

    return { moduleName, buildDirectory: relative(rootDir, buildDir), files };
}

if (verifyOnly) {
    const manifest = await verifyStagedAssets();
    process.stdout.write(`Validated ${manifest.flavor} WASM assets.\n`);
} else {
    if (!['debug', 'release', 'asan'].includes(flavor)) {
        throw new Error('Usage: stage-wasm.mjs <debug|release|asan> | --verify [debug|release|asan]');
    }

    const fullBuildDir = await resolveBuildDirectory('SPENCH_WASM_FULL_BUILD_DIR', `wasm-${flavor}`, 'spench');
    const dspBuildDir = await resolveBuildDirectory('SPENCH_WASM_DSP_BUILD_DIR', `wasm-dsp-${flavor}`, 'spench.dsp');
    const tempDir = `${destinationDir}.tmp-${process.pid}`;

    await rm(tempDir, { recursive: true, force: true });
    await mkdir(tempDir, { recursive: true });

    try {
        const artifacts = [
            await stageArtifact(tempDir, 'spench', fullBuildDir),
            await stageArtifact(tempDir, 'spench.dsp', dspBuildDir),
        ];
        const dockerfile = await readFile(resolve(rootDir, 'dockerfiles/Dockerfile.web'), 'utf8');
        const emsdkVersion = dockerfile.match(/^ARG EMSDK_VERSION=(.+)$/m)?.[1] || 'unknown';
        const manifest = {
            schemaVersion: 1,
            flavor,
            gitRevision: gitRevision(),
            emsdkVersion,
            artifacts,
        };

        await writeFile(resolve(tempDir, 'manifest.json'), `${JSON.stringify(manifest, null, 2)}\n`);
        await rm(destinationDir, { recursive: true, force: true });
        await mkdir(dirname(destinationDir), { recursive: true });
        await rename(tempDir, destinationDir);
    } catch (error) {
        await rm(tempDir, { recursive: true, force: true });
        throw error;
    }

    await verifyStagedAssets();
    process.stdout.write(`Staged ${flavor} WASM assets.\n`);
}
