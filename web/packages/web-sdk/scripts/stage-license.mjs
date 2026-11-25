// Copies the repository-root LICENSE.txt into the package so the published tarball carries the
// project terms without keeping a second copy of the file under version control.
import { access, copyFile } from 'node:fs/promises';
import { dirname, relative, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const scriptDir = dirname(fileURLToPath(import.meta.url));
const rootDir = resolve(scriptDir, '../../../..');
const sdkDir = resolve(scriptDir, '..');
const source = resolve(rootDir, 'LICENSE.txt');
const destination = resolve(sdkDir, 'LICENSE');

try {
    await access(source);
} catch {
    throw new Error(`Required license file is missing: ${source}`);
}

await copyFile(source, destination);
process.stdout.write(`Staged ${relative(rootDir, source)} to ${relative(rootDir, destination)}.\n`);
