// TestPattern.swift - built-in moving color bars served when the
// controller application is not running (so the camera never looks dead).

import Foundation

enum TestPattern {

    // YUYV colors: (Y, U, V) pairs.
    private static let bars: [(y: UInt8, u: UInt8, v: UInt8)] = [
        (180, 128, 128), // light gray
        (165, 44, 172),  // yellow
        (135, 84, 255),  // cyan
        (112, 72, 58),   // green
        (84, 184, 198),  // magenta
        (65, 100, 212),  // red
        (35, 212, 114),  // blue
        (16, 128, 128)   // black
    ]

    static func draw(into destination: UnsafeMutableRawPointer,
                     width: Int, height: Int, frameIndex: Int) {
        let markerX = max(0, frameIndex % width)
        let yuyv = destination.assumingMemoryBound(to: UInt8.self)
        for row in 0..<height {
            let base = row * width * 2
            var x = 0
            while x < width {
                let bar = x * 8 / width
                let next = min(7, (x + 1) * 8 / width)
                yuyv[base + x * 2 + 0] = bars[bar].y
                yuyv[base + x * 2 + 1] = bars[bar].u
                if x + 1 < width {
                    yuyv[base + x * 2 + 2] = bars[next].y
                    yuyv[base + x * 2 + 3] = bars[bar].v
                }
                x += 2
            }
        }
        // Moving white marker.
        for row in 0..<height {
            let base = row * width * 2
            for dx in 0..<8 where markerX + dx < width {
                yuyv[base + (markerX + dx) * 2 + 0] = 235
                yuyv[base + (markerX + dx) * 2 + 1] = 128
                if markerX + dx + 1 < width {
                    yuyv[base + (markerX + dx) * 2 + 2] = 235
                    yuyv[base + (markerX + dx) * 2 + 3] = 128
                }
            }
        }
    }
}
