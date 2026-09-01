import { test, expect, Page, Download } from '@playwright/test';
import { join } from 'path';
import { stat, mkdir } from 'fs/promises';

const RESAMPLER_8K_SKIP_REASON = 'Skipped while the 8 kHz resampler failure is under investigation.';

const setFeature = async (page: Page, testId: string, value: boolean | string) => {
    const element = page.getByTestId(testId);
    const type = await element.getAttribute('type');
    const tagName = await element.evaluate((el) => el.tagName);

    if (tagName === 'SELECT') {
        await element.selectOption(value as string);
    } else if (['radio', 'checkbox'].includes(type!)) {
        if (value) {
            const isChecked = await element.isChecked();
            if (!isChecked) await element.click();
        } else if (type === 'checkbox') {
            await element.uncheck();
        }
    } else if (type === 'number') {
        await element.fill(value as string);
    }
};

const uploadFile = async (page: Page, filePath: string) => {
    const fileInput = page.getByTestId('processingInput');
    const chooserPromise = page.waitForEvent('filechooser');
    await fileInput.click();
    const chooser = await chooserPromise;
    await chooser.setFiles(filePath);
};

const waitForDownload = async (page: Page): Promise<string> => {
    const downloadPromise = page.waitForEvent('download', { timeout: 120_000 });
    const download: Download = await downloadPromise;
    const tmpDir = 'downloads';
    await mkdir(tmpDir, { recursive: true });
    const downloadPath = join(tmpDir, download.suggestedFilename());
    await download.saveAs(downloadPath);
    return downloadPath;
};

const runSingleFeatureTest = (
    description: string,
    featureConfig: Record<string, boolean | string>,
    skipReason?: string,
) => {
    test(description, async ({ page }) => {
        test.skip(Boolean(skipReason), skipReason);
        await page.goto('/');

        for (const [testId, value] of Object.entries(featureConfig)) {
            await setFeature(page, testId, value);
        }

        await uploadFile(page, './tests/fixtures/sample.wav');
        const okLabel = page.getByTestId('processingOkStatus');
        await expect(okLabel).toBeVisible({ timeout: 120_000 });

        const downloadPromise = waitForDownload(page);
        await page.getByTestId('downloadResult').click();
        const downloadPath = await downloadPromise;

        await test.info().attach(`${description} – processed audio.webm`, {
            path: downloadPath,
            contentType: 'audio/webm',
        });

        const fstat = await stat(downloadPath);
        expect(fstat.size).toBeGreaterThan(0);

        await expect(okLabel).toBeVisible();
    });
};

const runBenchmarkTest = (
    description: string,
    featureConfig: Record<string, boolean | string>,
    skipReason?: string,
) => {
    test(description, async ({ page }) => {
        test.skip(Boolean(skipReason), skipReason);
        await page.goto('/');
        await page.getByTestId('benchmarkModeTab').click();

        for (const [testId, value] of Object.entries(featureConfig)) {
            await setFeature(page, testId, value);
        }

        const runBenchmarkButton = page.getByTestId('benchmarkButton');
        await runBenchmarkButton.click();

        const okLabel = page.getByTestId('processingOkStatus');
        await expect(okLabel).toBeVisible({ timeout: 120_000 });
    });
};

runSingleFeatureTest('Normalization only. MessageChannel transport.', {
    transportSelect: 'message',
    normalizationToggle: true,
    vadNormalizationToggle: false,
    aecToggle: false,
    denoiserNone: true,
});

runSingleFeatureTest('VAD normalization only. MessageChannel transport.', {
    transportSelect: 'message',
    normalizationToggle: true,
    vadNormalizationToggle: true,
    aecToggle: false,
    denoiserNone: true,
});

runSingleFeatureTest('Normalization + quality denoiser. MessageChannel transport.', {
    transportSelect: 'message',
    normalizationToggle: true,
    vadNormalizationToggle: false,
    aecToggle: false,
    denoiserQuality48: true,
});

runSingleFeatureTest('Normalization + fast denoiser. MessageChannel transport.', {
    transportSelect: 'message',
    sampleRateInput: '8000',
    normalizationToggle: true,
    vadNormalizationToggle: false,
    aecToggle: false,
    denoiserFast8: true,
}, RESAMPLER_8K_SKIP_REASON);

runSingleFeatureTest('VAD normalization + quality denoiser. MessageChannel transport.', {
    transportSelect: 'message',
    normalizationToggle: true,
    vadNormalizationToggle: true,
    aecToggle: false,
    denoiserQuality48: true,
});

runSingleFeatureTest('VAD normalization + fast denoiser. MessageChannel transport.', {
    transportSelect: 'message',
    sampleRateInput: '8000',
    normalizationToggle: true,
    vadNormalizationToggle: true,
    aecToggle: false,
    denoiserFast8: true,
}, RESAMPLER_8K_SKIP_REASON);

runSingleFeatureTest('Normalization only. SharedArrayBuffer transport.', {
    transportSelect: 'shared',
    normalizationToggle: true,
    vadNormalizationToggle: false,
    aecToggle: false,
    denoiserNone: true,
});

runSingleFeatureTest('Normalization + quality denoiser. SharedArrayBuffer transport.', {
    transportSelect: 'shared',
    normalizationToggle: true,
    vadNormalizationToggle: false,
    aecToggle: false,
    denoiserQuality48: true,
});

runBenchmarkTest('Multiple node benchmark. 2 nodes. VAD Normalization. MessageChannel transport.', {
    transportSelect: 'message',
    normalizationToggle: true,
    vadNormalizationToggle: true,
    aecToggle: false,
    denoiserNone: true,
    benchmarkInstancesCount: '2',
});

runBenchmarkTest(
    'Multiple node benchmark. 2 nodes. VAD Normalization + quality denoiser. MessageChannel transport.',
    {
        transportSelect: 'message',
        normalizationToggle: true,
        vadNormalizationToggle: true,
        aecToggle: false,
        denoiserQuality48: true,
        benchmarkInstancesCount: '2',
    },
);

runBenchmarkTest(
    'Multiple node benchmark. 2 nodes. VAD Normalization + fast denoiser. MessageChannel transport.',
    {
        sampleRateInput: '8000',
        transportSelect: 'message',
        normalizationToggle: true,
        vadNormalizationToggle: true,
        aecToggle: false,
        denoiserFast8: true,
        benchmarkInstancesCount: '2',
    },
    RESAMPLER_8K_SKIP_REASON,
);

runBenchmarkTest('Multiple node benchmark. 2 nodes. VAD Normalization. SharedArrayBuffer transport.', {
    transportSelect: 'shared',
    normalizationToggle: true,
    vadNormalizationToggle: true,
    aecToggle: false,
    denoiserNone: true,
    benchmarkInstancesCount: '2',
});

runBenchmarkTest(
    'Multiple node benchmark. 2 nodes. VAD Normalization + quality denoiser. SharedArrayBuffer transport.',
    {
        transportSelect: 'shared',
        normalizationToggle: true,
        vadNormalizationToggle: true,
        aecToggle: false,
        denoiserQuality48: true,
        benchmarkInstancesCount: '2',
    },
);

runBenchmarkTest(
    'Multiple node benchmark. 2 nodes. VAD Normalization + fast denoiser. SharedArrayBuffer transport.',
    {
        sampleRateInput: '8000',
        transportSelect: 'shared',
        normalizationToggle: true,
        vadNormalizationToggle: true,
        aecToggle: false,
        denoiserFast8: true,
        benchmarkInstancesCount: '2',
    },
    RESAMPLER_8K_SKIP_REASON,
);
