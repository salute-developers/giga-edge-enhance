import { cp, mkdir, readFile, rm, writeFile } from 'node:fs/promises';
import { dirname, relative, resolve, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

const scriptDirectory = dirname(fileURLToPath(import.meta.url));
const siteDirectory = resolve(scriptDirectory, '..');
const repositoryRoot = resolve(siteDirectory, '../../..');
const generatedDirectory = resolve(siteDirectory, '.generated-docs');
const sourceDirectory = resolve(generatedDirectory, 'source');
const coiServiceWorkerSource = resolve(siteDirectory, '../../node_modules/coi-serviceworker/coi-serviceworker.min.js');
const coiServiceWorkerDestination = resolve(siteDirectory, 'static/coi-serviceworker.min.js');

const repositoryDocumentation = [
    'README.md',
    'CHANGELOG.md',
    'SECURITY.md',
    'STYLE.md',
    'android/README.md',
    'android/docs/README.md',
    'android/docs/api-reference.md',
    'android/docs/architecture.md',
    'android/docs/demo-app.md',
    'android/docs/getting-started.md',
    'android/docs/models.md',
    'android/docs/proguard-r8.md',
    'android/docs/telemetry-and-performance.md',
    'android/docs/threading.md',
    'android/docs/troubleshooting.md',
    'ios/README.md',
    'ios/Sources/SpenchAudioKit/Documentation.docc/ErrorHandling.md',
    'ios/Sources/SpenchAudioKit/Documentation.docc/ModelBundles.md',
    'ios/Sources/SpenchAudioKit/Documentation.docc/QuickStart.md',
    'ios/Sources/SpenchAudioKit/Documentation.docc/SpenchAudioKit.md',
    'ios/Sources/SpenchAudioKit/Documentation.docc/Telemetry.md',
    'ios/Sources/SpenchAudioKit/Documentation.docc/ThreadingAndLifecycle.md',
    'web/README.md',
    'web/apps/spench-site/README.md',
    'web/packages/web-sdk/README.md',
    'web/packages/web-sdk/wasm/README.md',
];

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

for (const repositoryPath of repositoryDocumentation) {
    const sourcePath = resolve(repositoryRoot, repositoryPath);
    const destinationPath = resolve(sourceDirectory, relative(repositoryRoot, sourcePath));
    const content = normalizeForDocusaurus(await readFile(sourcePath, 'utf8'), sourcePath);
    await mkdir(dirname(destinationPath), { recursive: true });
    await writeFile(destinationPath, content);
}

console.log(`Staged repository documentation in ${relative(repositoryRoot, generatedDirectory)}.`);
