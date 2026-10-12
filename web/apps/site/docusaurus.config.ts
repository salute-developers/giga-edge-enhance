import type { Config } from '@docusaurus/types';
import { themes as prismThemes } from 'prism-react-renderer';

function normalizeBaseUrl(value: string | undefined): string {
    if (!value || value === '/') {
        return '/';
    }

    return `/${value.replace(/^\/+|\/+$/g, '')}/`;
}

const baseUrl = normalizeBaseUrl(process.env.GES_SITE_BASE);

const config: Config = {
    title: 'GigaEdge Spench',
    tagline: 'Real-time speech enhancement SDKs and tools',
    favicon: 'img/favicon.svg',
    url: process.env.GES_SITE_URL ?? 'https://salute-developers.github.io',
    baseUrl,
    headTags: [
        {
            tagName: 'script',
            attributes: {
                src: `${baseUrl}coi-serviceworker.min.js`,
            },
        },
    ],
    trailingSlash: true,
    onBrokenLinks: 'throw',
    onBrokenAnchors: 'throw',
    markdown: {
        format: 'md',
        hooks: {
            onBrokenMarkdownLinks: 'warn',
        },
    },
    i18n: {
        defaultLocale: 'en',
        locales: ['en'],
    },
    presets: [
        [
            'classic',
            {
                blog: false,
                docs: {
                    path: '.generated-docs',
                    routeBasePath: 'docs',
                    sidebarPath: './sidebars.ts',
                    showLastUpdateAuthor: false,
                    showLastUpdateTime: false,
                },
                theme: {
                    customCss: './src/css/custom.css',
                },
            },
        ],
    ],
    plugins: [
        () => ({
            name: 'preserve-workspace-package-builds',
            configureWebpack: () => ({
                // Keep linked workspace packages under node_modules so their prebuilt
                // worker and worklet assets are not transpiled as website source.
                resolve: { symlinks: false },
            }),
        }),
    ],
    themeConfig: {
        colorMode: {
            defaultMode: 'dark',
            disableSwitch: false,
            respectPrefersColorScheme: true,
        },
        navbar: {
            title: 'GigaEdge Spench',
            logo: {
                alt: 'GigaEdge Spench',
                src: 'img/logo.svg',
            },
            items: [
                { to: '/', label: 'Demo', position: 'left' },
                { type: 'docSidebar', sidebarId: 'mainSidebar', label: 'Documentation', position: 'left' },
                {
                    type: 'dropdown',
                    label: 'SDK reference',
                    position: 'left',
                    items: [
                        { label: 'Core repository guide', to: '/docs/source/' },
                        { label: 'Web SDK', to: '/docs/source/web/packages/web-sdk/' },
                        { label: 'Android SDK', to: '/docs/source/android/' },
                        { label: 'iOS SDK', to: '/docs/source/ios/' },
                    ],
                },
            ],
        },
        footer: {
            style: 'dark',
            links: [
                {
                    title: 'SDKs',
                    items: [
                        { label: 'Core C/C++', to: '/docs/core' },
                        { label: 'Web', to: '/docs/web-sdk' },
                        { label: 'Android', to: '/docs/android' },
                        { label: 'iOS', to: '/docs/ios' },
                    ],
                },
                {
                    title: 'Develop',
                    items: [
                        { label: 'Build and test', to: '/docs/development/build-and-test' },
                        { label: 'Repository style', to: '/docs/development/style' },
                        { label: 'MIT License', to: '/docs/license' },
                    ],
                },
            ],
        },
        prism: {
            theme: prismThemes.github,
            darkTheme: prismThemes.dracula,
        },
        metadata: [
            { name: 'description', content: 'Try GigaEdge Spench speech enhancement in the browser and explore the SDK documentation.' },
        ],
    },
};

export default config;
