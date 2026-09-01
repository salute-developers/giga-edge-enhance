import { readdirSync, readFileSync, writeFileSync } from 'fs';

const shared = JSON.parse(readFileSync('./limits.json'));
const message = JSON.parse(readFileSync('./limitsNew.json'));

for (const featureSet in shared) {
    for (const [fileName, { processed }] of Object.entries(shared[featureSet])) {
        const processedNew = message[featureSet][fileName].processed;

        if (Math.abs(processed - processedNew) / processed > 0.2) {
            console.log(`${featureSet} ${fileName} ${processed} ${processedNew}`);
        }
    }
}
