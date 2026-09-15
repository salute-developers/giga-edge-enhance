import Foundation
import os

/// `os_unfair_lock` wrapper sized for the GigaDenoiser processor and telemetry
/// fast paths. The lock is heap-allocated so its address stays stable for
/// the lifetime of the wrapper, which is required by `os_unfair_lock_lock`.
final class UnfairLock: @unchecked Sendable {
    private let pointer: UnsafeMutablePointer<os_unfair_lock_s>

    init() {
        pointer = UnsafeMutablePointer<os_unfair_lock_s>.allocate(capacity: 1)
        pointer.initialize(to: os_unfair_lock_s())
    }

    deinit {
        pointer.deinitialize(count: 1)
        pointer.deallocate()
    }

    @inline(__always)
    func lock() {
        os_unfair_lock_lock(pointer)
    }

    @inline(__always)
    func unlock() {
        os_unfair_lock_unlock(pointer)
    }

    @inline(__always)
    func withLock<T>(_ body: () throws -> T) rethrows -> T {
        os_unfair_lock_lock(pointer)
        defer { os_unfair_lock_unlock(pointer) }
        return try body()
    }
}
