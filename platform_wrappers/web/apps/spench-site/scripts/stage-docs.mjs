import { cp, mkdir, readdir, readFile, rm, writeFile } from 'node:fs/promises';
import { dirname, relative, resolve, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

const scriptDirectory = dirname(fileURLToPath(import.meta.url));
const siteDirectory = resolve(scriptDirectory, '..');
const repositoryRoot = resolve(siteDirectory, '../../../..');
const generatedDirectory = resolve(siteDirectory, '.generated-docs');
const sourceDirectory = resolve(generatedDirectory, 'source');
const coiServiceWorkerSource = resolve(siteDirectory, '../../node_modules/coi-serviceworker/coi-serviceworker.min.js');
const coiServiceWorkerDestination = resolve(siteDirectory, 'static/coi-serviceworker.min.js');

const excludedDirectories = new Set([
    '.build',
    '.cache',
    '.cpm',
    '.cxx',
    '.docusaurus',
    '.git',
    '.generated-docs',
    '.gradle',
    '.swiftpm',
    '.venv',
    'DerivedData',
    'build',
    'node_modules',
    'out',
    'playwright-report',
    'test-results',
]);

function isInside(candidate, parent) {
    const path = relative(parent, candidate);
    return path === '' || (!path.startsWith(`..${sep}`) && path !== '..');
}

async function collectMarkdownFiles(directory, files = []) {
    for (const entry of await readdir(directory, { withFileTypes: true })) {
        const path = resolve(directory, entry.name);

        if (entry.isDirectory()) {
            if (!excludedDirectories.has(entry.name) && !isInside(path, resolve(siteDirectory, 'docs'))) {
                await collectMarkdownFiles(path, files);
            }
            continue;
        }

        if (entry.isFile() && entry.name.endsWith('.md') && entry.name !== 'AGENTS.md') {
            files.push(path);
        }
    }

    return files;
}

function normalizeForDocusaurus(content, sourcePath) {
    if (!sourcePath.includes(`${sep}Documentation.docc${sep}`)) {
        return content;
    }

    return content.replace(/<doc:([A-Za-z0-9_-]+)>/g, (_, article) => `[${article}](./${article}.md)`);
}

await rm(generatedDirectory, { recursive: true, force: true });
await mkdir(sourceDirectory, { recursive: true });
await cp(resolve(siteDirectory, 'docs'), generatedDirectory, { recursive: true });
await cp(resolve(repositoryRoot, 'spench_logo.svg'), resolve(sourceDirectory, 'spench_logo.svg'));
await cp(resolve(repositoryRoot, 'LICENSE'), resolve(sourceDirectory, 'LICENSE.md'));
await cp(coiServiceWorkerSource, coiServiceWorkerDestination);

for (const sourcePath of await collectMarkdownFiles(repositoryRoot)) {
    const destinationPath = resolve(sourceDirectory, relative(repositoryRoot, sourcePath));
    const content = normalizeForDocusaurus(await readFile(sourcePath, 'utf8'), sourcePath);
    await mkdir(dirname(destinationPath), { recursive: true });
    await writeFile(destinationPath, content);
}

console.log(`Staged repository documentation in ${relative(repositoryRoot, generatedDirectory)}.`);
