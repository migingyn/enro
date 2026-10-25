# Enro film

The public demo opens with a 56 second silent MP4 and a short explanation. The native video controls support pause, seeking, and fullscreen. Reduced motion and data saver preferences disable autoplay. A transcript and visual descriptions cover the story without playback.

The structure takes inspiration from [Matthew Kim's Gist](https://mykm.dev/projects/gist): one visible transformation, a large embedded film, and the evidence behind the result. The project chat's useful lesson was to show event changes, plan updates, calls, and movement in one observable sequence. This is original Enro artwork. No Gist video, code, audio, or graphics are reused.

## Story

| Time | Scene |
| :--- | :--- |
| 0 to 6 seconds | A call at the right time, from Crossroads to Price Center |
| 6 to 14 seconds | A sample email moves the appointment from 6:30 PM to 6:20 PM |
| 14 to 23 seconds | Preparation moves to 5:05 PM and departure to 5:50 PM, using 45 minutes to prepare, 20 minutes of travel, and a 10 minute arrival buffer |
| 23 to 32 seconds | The phone is still at departure time; the call explains the changed appointment |
| 32 to 40 seconds | Seven counts plus a spoken reply fail the revised rule while Activity is stationary |
| 40 to 48 seconds | Eight fresh counts, walking Activity, and a spoken reply pass the experimental rule |
| 48 to 56 seconds | Optional location shows the phone outside the starting area; route and arrival remain unconfirmed |

Every frame is marked as a scripted walkthrough. It uses sample inputs and makes no real calls. The shaking and walking outcomes are checked against the existing demo model when rendering. The revised movement rule still needs physical validation. The film does not replace that test or claim an integrated backend.

## Rebuild

The checked in MP4 is H.264, 1600 × 900, 30 fps, with fast start metadata and no audio track. Vercel serves the existing file and does not need the rendering tools.

To regenerate, install `@napi-rs/canvas@0.1.100` in a temporary tools directory and have `ffmpeg` on PATH. Point `CANVAS_MODULES` at that directory's `node_modules`.

```sh
CANVAS_MODULES=/path/to/node_modules node prototype/scripts/render-demo-film.mjs --preview
CANVAS_MODULES=/path/to/node_modules node prototype/scripts/render-demo-film.mjs
node scripts/build_demo.mjs
```

Preview PNGs go to the ignored `.data/film-review/`. The MP4, poster, description track, and scenario manifest go to `prototype/public/demo/film/`. The renderer uses the existing local fonts. No keys or private state are read.

The old demo room is preserved at `/demo/lab.html`. Its original relative asset paths still work. The public build explicitly marks both pages as public so the lab never polls a local operator API when testing the generated site on localhost.
