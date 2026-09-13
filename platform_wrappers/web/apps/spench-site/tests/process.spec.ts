import { test, expect, Page, Download } from '@playwright/test';
import { join } from 'path';
import { mkdir } from 'fs/promises';
import { execSync } from 'child_process';
import { existsSync, readdirSync, readFileSync, writeFileSync } from 'fs';

const sourcesDir = './tests/mos/sources';
const limitsPath = './tests/mos/limits.json';
const candidatesPath = './tests/mos/candidates.json';
type MosResult = { control: number; processed: number };
type MosBaselines = Record<string, Record<string, Record<string, MosResult>>>;

const limits = JSON.parse(readFileSync(limitsPath, 'utf8')) as MosBaselines;
const candidates = (existsSync(candidatesPath) ? JSON.parse(readFileSync(candidatesPath, 'utf8')) : {}) as MosBaselines;
const MAX_REGRESSION_FRACTION = 0.05;
test.describe.configure({ mode: 'serial' });

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

const waitForDownload = async (page: Page, name: string): Promise<string> => {
    const downloadPromise = page.waitForEvent('download', { timeout: 4 * 60_000 });
    const download: Download = await downloadPromise;
    const tmpDir = 'downloads';
    await mkdir(tmpDir, { recursive: true });
    const downloadPath = join(tmpDir, name + download.suggestedFilename());
    await download.saveAs(downloadPath);
    return downloadPath;
};

const runNisqa = (filePath: string): number => {
    try {
        const output = execSync(`./nisqa.py ${filePath}`, { encoding: 'utf8' });
        const match = output.match(/[+-]?\d+(?:\.\d+)?/);
        if (!match) throw new Error(`No MOS number found in nisqa output for ${filePath}. output: ${output}`);
        return parseFloat(match[0]);
    } catch (err) {
        // Wrap any execSync failure – this will fail the current test
        throw new Error(`nisqa failed for ${filePath}: ${err instanceof Error ? err.message : err}`);
    }
};

const processFile = async (browserName: string, featureSetCodename: string, fileName: string, page: Page) => {
    const sourcePath = join(sourcesDir, fileName);

    // Upload the file, wait for processing, then download the retained result.
    await uploadFile(page, sourcePath);
    await expect(page.getByTestId('processingOkStatus')).toBeVisible({ timeout: 4 * 60_000 });

    const downloadPromise = waitForDownload(page, featureSetCodename);
    await page.getByTestId('downloadResult').click();
    const downloadPath = await downloadPromise;

    // 3️⃣ Measure MOS for both source & processed files
    const sourceMos = runNisqa(sourcePath);
    const processedMos = runNisqa(downloadPath);

    expect(Number.isFinite(sourceMos)).toBe(true);
    expect(Number.isFinite(processedMos)).toBe(true);
    console.log(`MOS Result: original ${sourceMos}, processed ${processedMos}`);

    if (process.env.MOS_WRITE_CANDIDATES === '1') {
        candidates[browserName] = candidates[browserName] || {};
        candidates[browserName][featureSetCodename] = candidates[browserName][featureSetCodename] || {};
        candidates[browserName][featureSetCodename][fileName] = { control: sourceMos, processed: processedMos };
        writeFileSync(candidatesPath, `${JSON.stringify(candidates, null, 2)}\n`);
        return;
    }

    const baseline = limits[browserName]?.[featureSetCodename]?.[fileName];
    if (!baseline || !Number.isFinite(baseline.control) || !Number.isFinite(baseline.processed)) {
        throw new Error(`Missing approved MOS baseline for ${browserName}/${featureSetCodename}/${fileName}`);
    }

    expect(sourceMos).toBeGreaterThanOrEqual(baseline.control * (1 - MAX_REGRESSION_FRACTION));
    expect(processedMos).toBeGreaterThanOrEqual(baseline.processed * (1 - MAX_REGRESSION_FRACTION));
};

const runSingleFeatureTest = (
    featureSetCodename: string,
    fileName: string,
    featureConfig: Record<string, boolean | string>,
) => {
    test(`Process ${featureSetCodename} for file: ${fileName}`, async ({ page, browserName }) => {
        test.slow();
        test.setTimeout(4 * 60_000);

        await page.goto('/');
        test.skip(
            !(await page.evaluate(() => typeof MediaRecorder !== 'undefined')),
            'This Playwright browser build does not expose MediaRecorder.',
        );

        for (const [testId, value] of Object.entries(featureConfig)) {
            await setFeature(page, testId, value);
        }

        await processFile(browserName, featureSetCodename, fileName, page);
    });
};

const sourceFiles = readdirSync(sourcesDir).filter((fileName) => fileName.endsWith('.wav')).sort();

for (const fileName of sourceFiles) {
    runSingleFeatureTest('norm', fileName, {
        transportSelect: 'shared',
        normalizationToggle: true,
        aecToggle: false,
        denoiserNone: true,
    });

    runSingleFeatureTest('normquality48', fileName, {
        transportSelect: 'shared',
        normalizationToggle: true,
        aecToggle: false,
        denoiserQuality48: true,
    });

    runSingleFeatureTest('normfast8', fileName, {
        transportSelect: 'shared',
        sampleRateInput: '8000',
        normalizationToggle: true,
        aecToggle: false,
        denoiserFast8: true,
    });
}
