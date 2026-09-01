import { readdirSync, readFileSync, writeFileSync } from 'fs';

const a = {};
const dir = readdirSync('.');

for (const file of dir) {
    const contents = readFileSync(file);
    const currentLimits = JSON.parse(contents);

    for (const featureSet of ['norm', 'normquant', 'normquant8k', 'normmobile']) {
        a[featureSet] = a[featureSet] || {};

        for (const fileName in currentLimits[featureSet]) {
            a[featureSet][fileName] = {
                control: currentLimits[featureSet][fileName].control,
                processed: currentLimits[featureSet][fileName].processed,
            };
        }
    }
}

writeFileSync(`./limitsNew.json`, JSON.stringify(a, null, 2), 'utf8');
