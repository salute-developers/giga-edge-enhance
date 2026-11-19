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
    'CONTRIBUTING.md',
    'SECURITY.md',
    'SUPPORT.md',
    'STYLE.md',
    'tools/README.md',
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
    'ios/Sources/EnhanceAudioKit/Documentation.docc/ErrorHandling.md',
    'ios/Sources/EnhanceAudioKit/Documentation.docc/ModelBundles.md',
    'ios/Sources/EnhanceAudioKit/Documentation.docc/QuickStart.md',
    'ios/Sources/EnhanceAudioKit/Documentation.docc/EnhanceAudioKit.md',
    'ios/Sources/EnhanceAudioKit/Documentation.docc/Telemetry.md',
    'ios/Sources/EnhanceAudioKit/Documentation.docc/ThreadingAndLifecycle.md',
    'web/README.md',
    'web/apps/site/README.md',
    'web/packages/web-sdk/README.md',
    'web/packages/web-sdk/wasm/README.md',
];

function normalizeForDocusaurus(content, sourcePath) {
    const toolsLink = sourcePath.endsWith(`${sep}CONTRIBUTING.md`) ? '../tools/' : './tools/';
    const normalizedLinks = content
        .replace(/\(tools\/README\.md\)/g, `(${toolsLink})`)
        .replace(/\(web\/apps\/site\/docs\//g, '(../');
    if (!sourcePath.includes(`${sep}Documentation.docc${sep}`)) {
        return normalizedLinks;
    }

    return normalizedLinks.replace(/<doc:([A-Za-z0-9_-]+)>/g, (_, article) => `[${article}](./${article}.md)`);
}

await rm(generatedDirectory, { recursive: true, force: true });
await mkdir(sourceDirectory, { recursive: true });
await cp(resolve(siteDirectory, 'docs'), generatedDirectory, { recursive: true });
await cp(resolve(repositoryRoot, 'LICENSE'), resolve(sourceDirectory, 'LICENSE.md'));
// README.md references this file from the repository root. The staged copy
// resolves that same relative URL from .generated-docs/source/.
await cp(resolve(repositoryRoot, 'enhance_logo.svg'), resolve(sourceDirectory, 'enhance_logo.svg'));
await cp(coiServiceWorkerSource, coiServiceWorkerDestination);

for (const repositoryPath of repositoryDocumentation) {
    const sourcePath = resolve(repositoryRoot, repositoryPath);
    const destinationPath = resolve(sourceDirectory, relative(repositoryRoot, sourcePath));
    const content = normalizeForDocusaurus(await readFile(sourcePath, 'utf8'), sourcePath);
    await mkdir(dirname(destinationPath), { recursive: true });
    await writeFile(destinationPath, content);
}

console.log(`Staged repository documentation in ${relative(repositoryRoot, generatedDirectory)}.`);
