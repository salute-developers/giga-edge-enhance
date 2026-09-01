import { test, expect } from '@playwright/test';

test('do not fail on running multiple nodes', async ({ page }) => {
    test.setTimeout(4 * 60_000);

    await page.goto('/');
    await page.getByText('Audio node stress test').click();
    const checkButton = page.getByTestId('startMultinormCheckButton');

    await expect(checkButton).toHaveRole('button');
    await checkButton.click();

    await expect(page.getByTestId('multinormOkStatus')).toBeVisible({ timeout: 240_000 });
});
