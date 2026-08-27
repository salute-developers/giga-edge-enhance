# Spench web

## Getting started

* Install NVM <https://github.com/nvm-sh/nvm#installing-and-updating>
* `nvm use`

`tools/spench build web` installs and uses the version in `.nvmrc` automatically
inside Docker. With `--host`, it uses NVM when available and otherwise leaves
the host Node.js version unchanged.
* From this directory, run `npm ci`.

## Build the SDK

`tools/spench build web` builds both release WASM modules, stages their validated
assets, runs SDK tests, and builds the SDK package.

To run the stages separately:

```bash
tools/spench build wasm-release
tools/spench build wasm-dsp-release
cd platform_wrappers/web
npm run stage:wasm:release
npm run build:web-sdk
```

The staged assets are generated under `packages/web-sdk/generated/wasm/` and
are not committed. `npm pack` requires a validated release manifest.

## Настройка Git LFS

В репозитории включен [Git LFS](https://git-lfs.github.com)

Для того чтобы установить расширение себе, необходимо выполнить команды:

```bash
brew install git-lfs
git lfs install
```

После этого вам станут доступны команды `git lfs`, чтобы заменить ссылки файлами введите:

```bash
git lfs checkout
```

## Корректное добавление в LFS файлов по маске

Перейдите в директорию со своим сервисом и выполните команду:

```bash
git lfs track 'cypress/**/*.png'
git lfs track 'cypress/**/*.jpg'
```

Обязательно указывайте маску пути в одинарных кавычках. После выполнения команды git-lfs обновит пути в `.gitattributes` и заменит бинарные файлы на поинтеры. Все это необходимо зафиксировать коммитом.
