import type { SidebarsConfig } from '@docusaurus/plugin-content-docs';

const sidebars: SidebarsConfig = {
    mainSidebar: [
        'overview',
        'getting-started',
        'distribution',
        'capabilities',
        {
            type: 'category',
            label: 'Core SDK',
            items: ['core/index', 'core/quickstart', 'core/configuration', 'core/cli'],
        },
        'web-sdk',
        'android',
        'ios',
        'models',
        'troubleshooting',
        'license',
        {
            type: 'category',
            label: 'Development',
            items: ['development/build-and-test', 'development/ci-and-releases', 'development/style'],
        },
    ],
};

export default sidebars;
