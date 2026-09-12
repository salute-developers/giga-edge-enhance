import { cp, mkdir, readFile, rm, writeFile } from 'node:fs/promises';
import { dirname, relative, resolve, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

const scriptDirectory = dirname(fileURLToPath(import.meta.url));
const siteDirectory = resolve(scriptDirectory, '..');
const repositoryRoot = resolve(siteDirectory, '../../../..');
const generatedDirectory = resolve(siteDirectory, '.generated-docs');
const sourceDirectory = resolve(generatedDirectory, 'source');
const coiServiceWorkerSource = resolve(siteDirectory, '../../node_modules/coi-serviceworker/coi-serviceworker.min.js');
const coiServiceWorkerDestination = resolve(siteDirectory, 'static/coi-serviceworker.min.js');

const repositoryDocumentation = [
    'README.md',
    'CHANGELOG.md',
    'STYLE.md',
    'platform_wrappers/android/README.md',
    'platform_wrappers/android/docs/README.md',
    'platform_wrappers/android/docs/api-reference.md',
    'platform_wrappers/android/docs/architecture.md',
    'platform_wrappers/android/docs/demo-app.md',
    'platform_wrappers/android/docs/getting-started.md',
    'platform_wrappers/android/docs/models.md',
    'platform_wrappers/android/docs/proguard-r8.md',
    'platform_wrappers/android/docs/telemetry-and-performance.md',
    'platform_wrappers/android/docs/threading.md',
    'platform_wrappers/android/docs/troubleshooting.md',
    'platform_wrappers/ios/README.md',
    'platform_wrappers/ios/Sources/SpenchAudioKit/Documentation.docc/ErrorHandling.md',
    'platform_wrappers/ios/Sources/SpenchAudioKit/Documentation.docc/ModelBundles.md',
    'platform_wrappers/ios/Sources/SpenchAudioKit/Documentation.docc/QuickStart.md',
    'platform_wrappers/ios/Sources/SpenchAudioKit/Documentation.docc/SpenchAudioKit.md',
    'platform_wrappers/ios/Sources/SpenchAudioKit/Documentation.docc/Telemetry.md',
    'platform_wrappers/ios/Sources/SpenchAudioKit/Documentation.docc/ThreadingAndLifecycle.md',
    'platform_wrappers/web/README.md',
    'platform_wrappers/web/apps/spench-site/README.md',
    'platform_wrappers/web/packages/web-sdk/README.md',
    'platform_wrappers/web/packages/web-sdk/wasm/README.md',
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
