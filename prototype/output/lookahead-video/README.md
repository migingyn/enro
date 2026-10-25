# Enro lookahead video overlay

Two silent, 21-second, 60 fps H.264 MP4 exports:

- `enro-lookahead-card.mp4` — 900 × 1020. Put this above your footage and resize or position it freely. On a 1920 × 1080 video, x=990 and y=30 fills the right half with a small margin.
- `enro-lookahead-right-half-green.mp4` — 1920 × 1080. The card is already positioned at x=990, y=30. Add it above the demo footage and use your editor's chroma key / remove color tool on the green background. Keep its scale at 100% for this placement.

The export has no audio, so it will not replace the demo's narration. MP4 does not carry the card's transparent corners in these files; use the green-screen version when you want to remove everything outside the rounded card.

## Timing

- 0–7 seconds: Still lying down, arrival 9:14 AM, 14 minutes late.
- 7–14 seconds: Getting ready, arrival 8:55 AM, 5 minutes early.
- 14–21 seconds: Heading out, arrival 8:35 AM, 25 minutes early.

Blue impulses cross individual connections in 0.23–0.32 seconds, with short trails and impact rings. The geometry stays stable so the graph remains readable. Each scene uses the same 8:05 AM reference time and 9:00 AM commitment.

Accelerometer, gyroscope, step, and GPS labels are staged illustration cues. The forecast uses the existing 2,048-sample local model and selected preparation/travel assumptions. This footage does not record or classify live sensor data. The on-card Demo scenario and Simulated signals labels remain visible.

## Re-render

The renderer is `scripts/render-lookahead-video.mjs`. It requires Node, FFmpeg on PATH, the project's Inter fonts, and `@napi-rs/canvas`. Set `CANVAS_MODULES` to a node_modules directory containing that package if it is not installed in the project. Run from the repository root:

```sh
node scripts/render-lookahead-video.mjs --preview
node scripts/render-lookahead-video.mjs
```

Set `VIDEO_OUTPUT` to change the output directory. The default is `output/lookahead-video`.
