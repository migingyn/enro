# Enro lookahead video overlay

Put a small, animated arrival forecast in the bottom-right corner of the demo footage. The visual connection to HealthLink is its clustered colored dots, curved connections, blue firing paths, and soft pulses. The source reference is the sibling project's `health-link-demo-publish-20261004/frontend/src/components/SimulationGraph.vue`. Here, the clusters represent starting delay, preparation, travel, and arrival. Make the changing arrival time the reveal; let the graph provide quiet motion around it.

The overlay is a standalone video asset in `public/demo/lookahead/`: `index.html`, `style.css`, `overlay.js`, and `model.js`. It uses browser-local calculations and no application API, private runtime, credentials, or live activity data.

## What the demo shows

Each change calculates 2,048 possible arrival times from illustrative triangular duration ranges. Starting delay, preparation, travel, and the final walk all contribute. A fixed random seed keeps comparisons repeatable; the displayed median, lateness probability, and histogram come from these actual samples.

The activity state is staged by selecting a scene. “Still lying down” is a chosen scenario, not a sensor reading or a working lying-down classifier. Graph pulses illustrate dependencies; they do not represent autonomous agents. The probabilities describe these assumptions and are not calibrated predictions. Keep the visible **Demo scenario** badge in the recording.

Every scene uses **8:05 AM now** and a **9:00 AM commitment**. These are alternative readiness states at the same instant, not successive moments on a running clock.

| Scene | Default median arrival | What changes |
| --- | --- | --- |
| Still lying down | 9:14 AM | Starting delay and preparation remain |
| Getting ready | 8:55 AM | Less delay and preparation remain |
| Heading out | 8:35 AM | Only departure, travel, and the final walk remain |

The travel assumption has a 26-minute mode in all three scenes. The sliders add **0–20 minutes of extra traffic** and **0–15 minutes of additional delay**. They recalculate the forecast immediately.

## Open and record

With the existing development server running, open [the overlay studio](http://localhost:3333/demo/lookahead/).

- **1 / 2 / 3:** select lying down, getting ready, or heading out.
- **Space:** play or stop the 18-second comparison, six seconds per scene.
- **M:** pause or resume graph motion. Reduced-motion system settings keep motion reduced.
- **R:** return to the first scene and zero both sliders.

Click the background before using shortcuts if a slider has focus. Space retains its normal activation behavior when a button or link is focused; the number, M, and R shortcuts still work after clicking a scene. Selecting a scene or changing a slider stops automatic playback.

Use [the overlay-only view](http://localhost:3333/demo/lookahead/?capture=1) to crop the card or add it as a browser source. Use [the green-screen corner view](http://localhost:3333/demo/lookahead/?corner=1&chroma=1) for compositing over footage. The corner position follows the browser viewport: set the recording or browser-source viewport to **1920 × 1080** for a 16:9 video. The capture links in the studio preserve the selected scene and slider values. Keyboard shortcuts also work in capture views.

For standalone static hosting, serve the project's `public` directory. No application server is needed for the overlay itself. For example, from the project directory:

```sh
python3 -m http.server 3334 --directory public
```

Then open [the standalone studio](http://localhost:3334/demo/lookahead/), using the same capture query parameters as above.

## A 20-second shot

Record three alternative morning states and label the sequence as a demo comparison. Reset the sliders, start the corner recording, and press Space for the built-in 18-second sequence; hold the last frame for two more seconds.

| Time | Footage and overlay |
| --- | --- |
| 0–6 s | Person resting; the graph wakes up and predicts 9:14 AM. Establish the 9:00 commitment. |
| 6–12 s | Cut to the getting-ready alternative. The arrival estimate moves to 8:55 AM. |
| 12–18 s | Cut to the heading-out alternative. The forecast settles at 8:35 AM. |
| 18–20 s | Hold the final forecast so the viewer can read it, then return to the main product demo. |

Voiceover: “This demo uses simulated activity states. Enro projects possible arrival times from starting delay, preparation, and travel. Change the state, and the forecast changes.”

## Verification

`node --test public/demo/lookahead/model.test.mjs` passes all five model tests: default arrival ranges and sample totals, deterministic comparisons, monotonic traffic and delay, readiness ordering, and input validation. `node --check public/demo/lookahead/overlay.js` passes.

Browser checks completed October 24, 2026: all three scene outputs; a 10-minute traffic adjustment moving the preparing forecast from 8:55 to 9:05; number shortcuts after clicking a scene; motion pause/resume; reset; the complete 18-second sequence; overlay-only view; and green-screen corner view with keyboard control. Desktop visuals inspected at 1280 × 720. Reduced-motion handling was reviewed in source; a system reduced-motion browser run and a mobile-device run were not performed. No video was exported, no live activity classifier was added, and no phone or sensor integration was tested by this overlay work.

## MP4 export — October 24, 2026

Created `output/lookahead-video/enro-lookahead-card.mp4` (900 × 1020) and `output/lookahead-video/enro-lookahead-right-half-green.mp4` (1920 × 1080, card at x=990/y=30). Both are silent H.264/yuv420p, 60 fps, exactly 21 seconds and 1,260 frames. Full FFmpeg decode checks passed for both; decoded frames from all three scenes and the right-half placement were visually reviewed. The earlier no-video note above describes the initial browser-only pass and is superseded by these exports.

The video adds fast blue particle trails and impact rings, plus staged accelerometer, gyroscope, steps, and GPS labels. The existing model produces the arrival metrics; signal labels are illustrative cues, not live classifier results. Each scene lasts seven seconds. Keep the visible Demo scenario and Simulated signals labels. Rendering source: `scripts/render-lookahead-video.mjs`. Editing directions and regeneration notes: `output/lookahead-video/README.md`.
