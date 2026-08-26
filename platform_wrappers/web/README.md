# Spench web

## Getting started

* Install NVM <https://github.com/nvm-sh/nvm#installing-and-updating>
* `nvm use`

`tools/spench build web` installs and uses the version in `.nvmrc` automatically
inside Docker. With `--host`, it uses NVM when available and otherwise leaves
the host Node.js version unchanged.
* `npm ci`

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
