import React, { useMemo } from 'react';
// @ts-expect-error jstat does not publish TypeScript declarations.
import jStat from 'jstat';

type Props = {
    result: number[];
};

type Metric = {
    label: string;
    value: string;
};

function milliseconds(value: number): string {
    return `${value.toFixed(2)} ms`;
}

const Statistics: React.FC<Props> = React.memo(({ result }) => {
    const metrics = useMemo(() => {
        const mean = jStat.mean(result);
        const error99 = jStat.normalci(mean, 0.01, result)[1] - mean;
        const quantiles = jStat.quantiles(result, [0.01, 0.05, 0.25, 0.75, 0.95, 0.99]);

        const summary: Metric[] = [
            { label: 'Mean', value: milliseconds(mean) },
            { label: 'Median', value: milliseconds(jStat.median(result)) },
            { label: '95th percentile', value: milliseconds(quantiles[4]) },
            { label: 'Maximum', value: milliseconds(jStat.max(result)) },
        ];
        const details: Metric[] = [
            { label: 'Samples', value: String(result.length) },
            { label: '99% CI error', value: `±${milliseconds(error99)}` },
            { label: 'Standard deviation', value: milliseconds(jStat.stdev(result)) },
            { label: 'Minimum', value: milliseconds(jStat.min(result)) },
            { label: '1st percentile', value: milliseconds(quantiles[0]) },
            { label: '5th percentile', value: milliseconds(quantiles[1]) },
            { label: '25th percentile', value: milliseconds(quantiles[2]) },
            { label: '75th percentile', value: milliseconds(quantiles[3]) },
            { label: '99th percentile', value: milliseconds(quantiles[5]) },
        ];

        return { summary, details };
    }, [result]);

    return (
        <section className="statistics" aria-labelledby="statistics-title" data-testid="statistics">
            <header className="statisticsHeader">
                <div>
                    <p className="panelKicker">Latest run</p>
                    <h3 id="statistics-title">Processing performance</h3>
                </div>
                <span>{result.length} chunks</span>
            </header>

            <dl className="statisticsSummary">
                {metrics.summary.map((metric) => (
                    <div className="statisticCard" key={metric.label}>
                        <dt>{metric.label}</dt>
                        <dd>{metric.value}</dd>
                    </div>
                ))}
            </dl>

            <details className="statisticsDetails">
                <summary>Full distribution</summary>
                <dl>
                    {metrics.details.map((metric) => (
                        <div key={metric.label}>
                            <dt>{metric.label}</dt>
                            <dd>{metric.value}</dd>
                        </div>
                    ))}
                </dl>
            </details>
        </section>
    );
});

export default Statistics;
