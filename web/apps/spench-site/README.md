# Spench website

This workspace builds the static Docusaurus website used for the interactive Web SDK workbench and
repository documentation. The demo consumes the packed `@gigaedge/spench-web-sdk` package, including
its validated WebAssembly, workers, worklets, models, and type definitions.

## Development

Use the Node.js version in `web/.nvmrc`, then install from the web workspace root:

```bash
cd web
nvm use
npm ci
npm start --workspace=@gigaedge/spench-site
```

The development server runs at `http://127.0.0.1:5173/`. The SDK watcher and Docusaurus server run in
parallel.

## Production build

The supported repository-level command builds and stages the WebAssembly dependencies before the
website:

```bash
tools/spench build web
```

To build only the website after validated Web SDK assets already exist:

```bash
cd web
npm run build --workspace=@gigaedge/spench-site
```

Static files are written to `apps/spench-site/build/`.

The documentation staging step copies user-facing Markdown into an ignored generated directory so
Docusaurus can compile one coherent site without moving platform READMEs or iOS DocC articles from
their canonical locations.

## Browser tests

```bash
cd web
npm run pw:test --workspace=@gigaedge/spench-site
```

Playwright starts the production build automatically unless `PUBLIC_INDEX_URL` points to an existing
deployment. The suite covers the denoiser in processing, benchmark, MOS, and feature-switching scenarios.

## GitHub Pages

Set `SPENCH_SITE_BASE` to the repository path, including surrounding slashes, when building a project
site. For example:

```bash
SPENCH_SITE_BASE=/spench/ npm run build --workspace=@gigaedge/spench-site
```

GitHub Pages does not expose COOP/COEP response-header configuration. The built site therefore
registers a same-origin COI service worker and may reload once on first visit before enabling
SharedArrayBuffer. Browsers without service-worker isolation fall back to MessageChannel.
