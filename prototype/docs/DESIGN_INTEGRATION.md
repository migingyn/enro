# Designer landing integration

Based on shiyokohahsi's commit `8f18509f92745027af7a64629c26cdae90d483d1`
on `design-landing-pages`: "Design: add static landing, approve events, and
brand guidelines pages." Original files are preserved in `design/landing/`.

The running TypeScript prototype serves:

- `/`: the designer's sky background, radar animation, wordmark, and typography,
  with a working "Plan my day" entry point instead of the simulated waitlist.
- `/approve.html`: the designer's event card layout, connected to existing
  account, event, attendance, and travel planning APIs. No sample events or
  made up route times are inserted into the live app. Plans remain disarmed.
- `/console.html`: the existing phone, sensor, service, and plan controls,
  with a matching color and type stylesheet. Old console hash links redirect here.

The event screen can sync connected Google accounts, add a real event, select
events, and calculate plans using the existing Routes integration. Partial
route failures preserve successful results; retries reuse matching plans.
Calendar and email content is inserted with textContent rather than HTML.

Google Maps Routes is now connected through the approved free trial and a
server only key restricted to Routes API. Both requested Google accounts are
connected. Known UCSD classroom codes show their resolved building, and online
events stay visible with travel selection disabled. Without a configured Routes
key, the calculate button explains its setup dependency. Google consent is only started by clicking an account's Connect
button. Opening the landing or event page does not start consent or place calls.

This imports the designer's work into the local TypeScript prototype. It does
not merge the separate C++ team backend or change the remote repository.

Validation: TypeScript check and focused event planning/destination tests pass. All
local page links and assets resolve. Chrome rendered the landing and live event
screen, including a 400px mobile viewport; the event page logged no JavaScript
console messages. Archivo and Inter are served locally with their font licenses.
Live Google Routes verified a 9-minute drive and 55-minute walk from the saved
home landmark to Warren Lecture Hall for a real upcoming event. The driving
plan was saved disarmed and displayed through the event screen. Walking was
verified without saving a second plan for the same event. No calls were placed
or armed by this integration pass.
