import { test, expect, Page, Download } from '@playwright/test';
import { join } from 'path';
import { stat, mkdir } from 'fs/promises';

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

async function waitForInvisible(page: Page, testId: string, timeout: number = 10000): Promise<void> {
    const selector = `[data-testid="${testId}"]`;

    await new Promise((r) => setTimeout(r, 100));
    const exists = await page.$(selector);
    if (!exists) return;

    // Wait until the element is detached from the DOM
    await page.waitForSelector(selector, { state: 'detached', timeout });
}

const runFeatureSwitchingTest = (description: string, bootstrapFeatures: Record<string, boolean | string>) => {
    test(description, async ({ page }) => {
        await page.goto('/');

        bootstrapFeatures = {
            normalizationToggle: false,
            aecToggle: false,
            denoiserNone: true,

            ...bootstrapFeatures,
        };

        for (const [testId, value] of Object.entries(bootstrapFeatures)) {
            await setFeature(page, testId, value);
        }

        await uploadFile(page, './tests/fixtures/sample.wav');

        async function setFeatureAndWaitForLoading(features: Record<string, boolean | string>) {
            for (const [testId, value] of Object.entries(features)) {
                await setFeature(page, testId, value);
            }
            await waitForInvisible(page, 'loadingFeature');

            await new Promise((r) => setTimeout(r, 1000));
        }

        await setFeatureAndWaitForLoading({ normalizationToggle: true });
        await setFeatureAndWaitForLoading({ denoiserQuality48: true });
        await setFeatureAndWaitForLoading({ denoiserNone: true, aecToggle: true });
        await setFeatureAndWaitForLoading({ denoiserQuality48: true });
        await setFeatureAndWaitForLoading({ normalizationToggle: false });
        await setFeatureAndWaitForLoading({ denoiserNone: true });
        await setFeatureAndWaitForLoading({ aecToggle: false });

        await expect(page.getByTestId('processingOkStatus')).toBeVisible({ timeout: 120_000 });
        const downloadPromise = waitForDownload(page);
        await page.getByTestId('downloadResult').click();
        const downloadPath = await downloadPromise;

        await test.info().attach(`${description} – processed audio`, {
            path: downloadPath,
        });

        const fstat = await stat(downloadPath);
        expect(fstat.size).toBeGreaterThan(0);

        const okLabel = page.getByTestId('processingOkStatus');
        await expect(okLabel).toBeVisible();
    });
};

runFeatureSwitchingTest('Feature switching test. MessageChannel transport.', {
    transportSelect: 'message',
});

runFeatureSwitchingTest('Feature switching test. SharedArrayBuffer transport.', {
    transportSelect: 'shared',
});

runFeatureSwitchingTest('Feature switching through the fast 8 kHz denoiser', {
    transportSelect: 'shared',
    sampleRateInput: '8000',
    denoiserFast8: true,
});
