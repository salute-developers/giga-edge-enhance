export class FrameTimeCounter {
    frameTimeCounter = 0;
    frameCounter = 0;
    rafId = 0;

    getAvgFrameTime(): number {
        return this.frameTimeCounter / this.frameCounter;
    }

    appendFrameTime(deltaTime: number) {
        this.frameCounter++;
        this.frameTimeCounter += deltaTime;
    }

    startMonitoring() {
        let prevTimer = performance.now();

        const callback = (currentTime: DOMHighResTimeStamp) => {
            const deltaTime = currentTime - prevTimer;
            this.appendFrameTime(deltaTime);

            prevTimer = currentTime;
            this.rafId = requestAnimationFrame(callback);
        };

        this.rafId = requestAnimationFrame(callback);
    }

    stopMonitoring() {
        cancelAnimationFrame(this.rafId);
    }

    clear() {
        this.frameTimeCounter = 0;
        this.frameCounter = 0;
    }
}
