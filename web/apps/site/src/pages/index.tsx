import BrowserOnly from '@docusaurus/BrowserOnly';
import Head from '@docusaurus/Head';
import Layout from '@theme/Layout';

function DemoFallback() {
    return (
        <main className="demoShell demoFallback" aria-busy="true">
            <p className="eyebrow">Browser audio lab</p>
            <h1>Audio processing workbench</h1>
            <p>The interactive workbench is preparing the audio engine.</p>
        </main>
    );
}

export default function DemoPage() {
    return (
        <Layout>
            <Head>
                <title>GigaDenoiser web audio demo</title>
                <meta
                    name="description"
                    content="Process an audio file or microphone signal with the GigaDenoiser Web SDK in your browser."
                />
            </Head>
            <BrowserOnly fallback={<DemoFallback />}>
                {() => {
                    // Docusaurus recommends a deferred CommonJS import for browser-only modules.
                    // eslint-disable-next-line @typescript-eslint/no-require-imports
                    const DemoApp = require('../App').default;
                    return <DemoApp />;
                }}
            </BrowserOnly>
        </Layout>
    );
}
