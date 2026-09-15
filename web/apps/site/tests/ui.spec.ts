import { expect, test } from '@playwright/test';

test('demo navigation unmounts engineering tools without crashing', async ({ page }) => {
    const pageErrors: string[] = [];
    page.on('pageerror', (error) => pageErrors.push(error.message));

    await page.goto('/');
    await page.locator('summary').filter({ hasText: 'WebRTC video workload' }).click();
    await expect(page.getByTestId('webrtcWorkload')).toBeVisible();

    await page.getByRole('link', { name: 'Documentation' }).click();
    await expect(page.getByRole('heading', { name: 'GigaDenoiser documentation' })).toBeVisible();

    await page.goto('/');
    await page.locator('summary').filter({ hasText: 'WebRTC video workload' }).click();
    await page.getByRole('button', { name: 'SDK reference' }).click();
    await page.getByRole('link', { name: 'Web SDK', exact: true }).click();
    await expect(page.getByRole('heading', { name: 'GigaDenoiser Web SDK' })).toBeVisible();
    expect(pageErrors).toEqual([]);
});

test('runner tabs have one visible active state', async ({ page }) => {
    await page.goto('/');

    const fileTab = page.getByTestId('fileModeTab');
    const microphoneTab = page.getByTestId('liveModeTab');
    await microphoneTab.click();

    await expect(fileTab).toHaveAttribute('aria-selected', 'false');
    await expect(microphoneTab).toHaveAttribute('aria-selected', 'true');

    const fileBackground = await fileTab.evaluate((element) => getComputedStyle(element).backgroundColor);
    const microphoneBackground = await microphoneTab.evaluate((element) => getComputedStyle(element).backgroundColor);
    expect(fileBackground).not.toBe(microphoneBackground);
});

test('processed statistics use the product metric layout', async ({ page, browserName }) => {
    test.skip(browserName === 'firefox', 'The short fixture does not produce a Firefox raw-timing batch.');
    await page.goto('/');
    test.skip(
        !(await page.evaluate(() => typeof MediaRecorder !== 'undefined')),
        'This Playwright browser build does not expose MediaRecorder.',
    );

    const chooserPromise = page.waitForEvent('filechooser');
    await page.getByTestId('processingInput').click();
    const chooser = await chooserPromise;
    await chooser.setFiles('./tests/fixtures/sample.wav');

    await expect(page.getByTestId('processingOkStatus')).toBeVisible({ timeout: 120_000 });
    const statistics = page.getByTestId('statistics');
    await expect(statistics).toBeVisible();
    await expect(statistics.locator('.statisticCard')).toHaveCount(4);
    await expect(statistics.getByText('95th percentile')).toBeVisible();
    await expect(statistics.locator('textarea')).toHaveCount(0);
});

test('WebRTC workload form is grouped and stays inside its panel', async ({ page }) => {
    await page.goto('/');
    await page.locator('summary').filter({ hasText: 'WebRTC video workload' }).click();

    const workload = page.getByTestId('webrtcWorkload');
    await expect(workload.getByRole('group', { name: 'Workload configuration' })).toBeVisible();
    await expect(workload.getByRole('button', { name: 'Start workload' })).toBeVisible();
    await expect(workload.getByLabel('Codec')).toHaveValue('vp8');

    const layout = await workload.evaluate((element) => ({
        clientWidth: element.clientWidth,
        scrollWidth: element.scrollWidth,
    }));
    expect(layout.scrollWidth).toBeLessThanOrEqual(layout.clientWidth);
});

test('shared-memory transport is available in an isolated deployment', async ({ page }) => {
    await page.goto('/');

    expect(await page.evaluate(() => window.crossOriginIsolated)).toBe(true);
    await expect(page.getByTestId('transportSelect')).toBeEnabled();
    await expect(page.getByTestId('transportSelect')).toHaveValue('shared');
    await expect(page.getByText('Shared memory', { exact: true })).toBeVisible();
});
