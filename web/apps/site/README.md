# GigaDenoiser website

This workspace builds the static Docusaurus website used for the interactive Web SDK workbench and
repository documentation. The demo consumes the packed `@gigaedge/denoiser-web-sdk` package, including
its validated WebAssembly, workers, worklets, models, and type definitions.

## Development

Use the Node.js version in `web/.nvmrc`, then install from the web workspace root:

```bash
cd web
nvm use
npm ci
npm start --workspace=@gigaedge/denoiser-site
```

The development server runs at `http://127.0.0.1:5173/`. The SDK watcher and Docusaurus server run in
parallel.

## Production build

The supported repository-level command builds and stages the WebAssembly dependencies before the
website:

```bash
tools/build web
```

To build only the website after validated Web SDK assets already exist:

```bash
cd web
npm run build --workspace=@gigaedge/denoiser-site
```

Static files are written to `apps/site/build/`.

The documentation staging step copies an explicit set of user-facing Markdown into an ignored
generated directory so Docusaurus can compile one coherent site without moving platform READMEs or
iOS DocC articles from their canonical locations. Generated, vendored, cached, and internal agent
files are not staged.

## Browser tests

```bash
cd web
npm run pw:test --workspace=@gigaedge/denoiser-site
```

Playwright starts the production build automatically unless `PUBLIC_INDEX_URL` points to an existing
deployment. The suite covers the denoiser in processing, benchmark, MOS, and feature-switching scenarios.

## GitHub Pages

Set `GDN_SITE_BASE` to the repository path, including surrounding slashes, when building a project
site. For example:

```bash
GDN_SITE_BASE=/gigadenoiser/ npm run build --workspace=@gigaedge/denoiser-site
```

GitHub Pages does not expose COOP/COEP response-header configuration. The built site therefore
registers a same-origin COI service worker and may reload once on first visit before enabling
SharedArrayBuffer. Browsers without service-worker isolation fall back to MessageChannel.
