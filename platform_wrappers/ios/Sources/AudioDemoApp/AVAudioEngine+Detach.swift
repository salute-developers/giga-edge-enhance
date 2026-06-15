import AVFoundation

extension AVAudioEngine {
    /// Disconnects every input/output connection of `node` and detaches it
    /// from the engine if it is currently attached. Idempotent.
    func disconnectAndDetachIfNeeded(_ node: AVAudioNode) {
        disconnectNodeInput(node)
        disconnectNodeOutput(node)
        if node.engine === self {
            detach(node)
        }
    }
}
