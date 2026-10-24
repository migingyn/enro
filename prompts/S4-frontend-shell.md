Do task S4 (frontend shell) from TASKS.md in the migingyn/enro repo. Read AGENTS.md, PRD.md (sections 5, 9, 11, 12, 15) and contracts/README.md first.

You own ONLY frontend/** EXCEPT frontend/src/api/types.ts, which is a frozen contract: import from it, never edit it. Do not touch any file outside frontend/. Another session is building the backend at the same time.

Goal: a thin, working shell that designers will restyle later. Keep visuals plain and clean (Tailwind, neutral palette, no design system). Correct structure and data wiring matter more than looks.

Build:
- Vite + React 18 + TypeScript (strict) + Tailwind + React Router. Files per PRD section 9: package.json, vite.config.ts, tailwind.config.ts, tsconfig.json, index.html, src/main.tsx, src/App.tsx.
- Vite dev server on 5173 with a proxy for /api, /oauth, /audio, /health and /ws (websocket, ws: true) to http://localhost:8080.
- Mock mode: when VITE_USE_MOCKS is "true" (the default; add frontend/.env.example with VITE_USE_MOCKS=true and VITE_API_BASE=http://localhost:8080), every API call resolves to the fixture in ../contracts/examples/<name>.json. Import fixtures through a Vite alias "@contracts" -> ../contracts/examples (set server.fs.allow so Vite can read it). Type every fixture with the types in src/api/types.ts.
- src/api/client.ts: one typed function per section 11 route (health, onboardingSession, onboardingComplete, confirmContact, getPlan, getConsequences, getPunctuality, postLocation, demoTrigger, demoTrip, simLoad). Each switches between mock fixture and fetch().
- src/hooks/useDashboardSocket.ts: connects to /ws/dashboard and returns the typed DashboardEvent list. In mock mode it replays the six ws_*.json fixtures on a timer so the dashboard looks alive.
- src/hooks/useVoiceSession.ts: STUB. Returns {state, captions, start, stop}. In mock mode it walks through the onboarding_session questions with fake captions. Mark it with // STUB and // TODO(T17): OpenAI Realtime over WebRTC per PRD 8.
- Pages (src/pages/): Landing (motto "Ontime everytime." and "It knows you'll be late before you do", Start button), Onboarding (VoiceOrb + live captions, uses useVoiceSession, ends at Done), Done ("You're set"), Dashboard (judges only: PlanTimeline, ReasoningLines, ReflexFeed, PunctualityCard, WakeLadder, Transcript, TripReplay, LoadSimPanel, plus buttons that call demoTrigger for wake_ladder / leave_reminder / email_drop), CallSimulator (connects to /ws/sim-call/:call_id; shows ring, answer/decline buttons, the agent's "say" text read with window.speechSynthesis, a text box that sends "speech"; in mock mode use the simcall_*.json fixtures), Beacon (asks for geolocation and POSTs /api/location every 15 s).
- Components (src/components/): VoiceOrb, PlanTimeline, ReasoningLines, ReflexFeed, PunctualityCard, WakeLadder, Transcript, TripReplay, LoadSimPanel. Each takes typed props and renders the fixture data sensibly (timestamps shown in America/Los_Angeles).
- Every component or page that is placeholder-level gets a comment `// TODO(T18)`, `T19`, `T20`, `T21` or `T17` matching its owner in TASKS.md.
- Routes: / Landing, /onboarding, /done, /dashboard, /call/:callId, /beacon.

Check before shipping: `npm install`, `npm run build` (tsc + vite build) passes with zero type errors, and `npm run dev` serves every route without console errors in mock mode. Commit package-lock.json. Do not commit node_modules or dist.

Then ship with /shipit (branch S4-frontend-shell, commit prefix "S4:"). If the gh CLI isn't available in this environment, push the branch and open the PR however you can, then report the PR URL.
