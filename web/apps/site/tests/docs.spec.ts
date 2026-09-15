import { expect, test } from '@playwright/test';

test('documentation navigation exposes every SDK', async ({ page }) => {
    await page.goto('/docs/');

    await expect(page.getByRole('heading', { name: 'GigaDenoiser documentation' })).toBeVisible();
    await expect(page.getByRole('link', { name: 'Core C and C++ SDK' })).toBeVisible();
    await expect(page.getByRole('link', { name: 'Web SDK', exact: true }).first()).toBeVisible();
    await expect(page.getByRole('link', { name: 'Android SDK' }).first()).toBeVisible();
    await expect(page.getByRole('link', { name: 'iOS SDK' }).first()).toBeVisible();
    await expect(page.getByText(/Last updated/)).toHaveCount(0);

    await page.getByRole('link', { name: 'License', exact: true }).click();
    await expect(page.getByRole('heading', { name: 'MIT License' })).toBeVisible();
});

test('repository Markdown and DocC articles are published', async ({ page }) => {
    await page.goto('/docs/source/');
    await expect(page.getByRole('heading', { name: 'GigaDenoiser', exact: true })).toBeVisible();

    await page.goto('/docs/source/ios/Sources/GigaDenoiserAudioKit/Documentation.docc/QuickStart/');
    await expect(page.getByRole('heading', { name: 'Quick Start' })).toBeVisible();

    const repositoryInstructions = await page.goto('/docs/source/AGENTS/');
    expect(repositoryInstructions?.status()).toBe(404);
});

test('demo retains a usable mobile layout', async ({ page }) => {
    await page.setViewportSize({ width: 390, height: 844 });
    await page.goto('/');

    await expect(page.getByRole('heading', { name: 'Audio processing workbench' })).toBeVisible();
    await expect(page.getByTestId('fileModeTab')).toBeVisible();
    await expect(page.getByTestId('processingInput')).toBeAttached();
    expect(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth)).toBe(true);
});
