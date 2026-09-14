import { createReadStream } from 'node:fs';
import { stat } from 'node:fs/promises';
import { createServer } from 'node:http';
import { extname, resolve, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

const siteDirectory = resolve(fileURLToPath(new URL('..', import.meta.url)));
const buildDirectory = resolve(siteDirectory, 'build');
const port = Number(process.env.PORT ?? 5173);
const isolationHeadersEnabled = process.env.SPENCH_DISABLE_ISOLATION_HEADERS !== '1';

const contentTypes = new Map([
    ['.css', 'text/css; charset=utf-8'],
    ['.html', 'text/html; charset=utf-8'],
    ['.js', 'text/javascript; charset=utf-8'],
    ['.json', 'application/json; charset=utf-8'],
    ['.mp4', 'video/mp4'],
    ['.spench', 'application/octet-stream'],
    ['.svg', 'image/svg+xml'],
    ['.wasm', 'application/wasm'],
    ['.wav', 'audio/wav'],
    ['.webm', 'audio/webm'],
    ['.xml', 'application/xml; charset=utf-8'],
]);

function isInsideBuild(path) {
    return path === buildDirectory || path.startsWith(`${buildDirectory}${sep}`);
}

async function resolveRequestPath(pathname) {
    let path = resolve(buildDirectory, `.${decodeURIComponent(pathname)}`);

    if (!isInsideBuild(path)) {
        return null;
    }

    try {
        const details = await stat(path);
        if (details.isDirectory()) {
            path = resolve(path, 'index.html');
        }
        await stat(path);
        return path;
    } catch {
        return null;
    }
}

const server = createServer(async (request, response) => {
    const requestUrl = new URL(request.url ?? '/', `http://${request.headers.host ?? '127.0.0.1'}`);
    const path = await resolveRequestPath(requestUrl.pathname);

    if (isolationHeadersEnabled) {
        response.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');
        response.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
    }

    if (!path) {
        response.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' });
        response.end('Not found');
        return;
    }

    response.writeHead(200, { 'Content-Type': contentTypes.get(extname(path)) ?? 'application/octet-stream' });
    createReadStream(path).pipe(response);
});

server.listen(port, '127.0.0.1', () => {
    console.log(`Serving the static site at http://127.0.0.1:${port}/`);
});
