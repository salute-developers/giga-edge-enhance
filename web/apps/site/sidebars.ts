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
            link: { type: 'doc', id: 'core/index' },
            items: ['core/quickstart', 'core/configuration', 'core/cli'],
        },
        {
            type: 'category',
            label: 'Web SDK',
            link: { type: 'doc', id: 'web-sdk' },
            items: ['source/web/packages/web-sdk/README'],
        },
        {
            type: 'category',
            label: 'Android SDK',
            link: { type: 'doc', id: 'android' },
            items: [
                'source/android/docs/getting-started',
                'source/android/docs/models',
                'source/android/docs/threading',
                'source/android/docs/telemetry-and-performance',
                'source/android/docs/troubleshooting',
                'source/android/docs/api-reference',
                'source/android/docs/architecture',
                'source/android/docs/proguard-r8',
                'source/android/docs/demo-app',
            ],
        },
        {
            type: 'category',
            label: 'iOS SDK',
            link: { type: 'doc', id: 'ios' },
            items: [
                'source/ios/Sources/GigaEdgeSpenchAudioKit/Documentation.docc/QuickStart',
                'source/ios/Sources/GigaEdgeSpenchAudioKit/Documentation.docc/ModelBundles',
                'source/ios/Sources/GigaEdgeSpenchAudioKit/Documentation.docc/ThreadingAndLifecycle',
                'source/ios/Sources/GigaEdgeSpenchAudioKit/Documentation.docc/Telemetry',
                'source/ios/Sources/GigaEdgeSpenchAudioKit/Documentation.docc/ErrorHandling',
            ],
        },
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
