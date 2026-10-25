# SIC: three ideas designed around the actual brief

October 24, 2026. Recommendation and proposed demos; no applications, calls, bookings, or implementation tests were performed.

## Verified event and judging evidence

[Official event](https://luma.com/compiled-im3b): c0mpiled-16: UCSD, with Startup Incubator Club and Transpose Platform. Theme: Invisible Interfaces. Tracks cover physical dispatch from live information, screen-free experiences, agents that work independently and recognize failure/limits, and ambient consumer AI. The official schedule gives hacking 1:45–6:30 PM Pacific.

The Hacker Guide in [the team's shared document, Tab 11](https://docs.google.com/document/d/1h76F7lYYbtquW_43lZpmp_dveMFyaB3AZ0_zXf7E6KA/edit?tab=t.bot260rv2uqv) lists **scalability, innovation, and real-world impact** as judging domains. No numerical weights are given. It requires one track, a fresh build, and an explanation of AI coding-tool use. Three teams win overall. This is the guide reproduced in the team document; the scoring domains are not independently displayed on Luma.

The guide and Luma disagree on the start time and finalist schedule. They agree on the **6:30 PM submission deadline**, also confirmed by the organizer's [October 24 update](https://mail.google.com/mail/u/?authuser=kpyo%40ucsd.edu#all/1a16f276f5595d01). The separate tab called Hackathon Guide links to another event; do not use that event's rules. No per-team pitch duration or mandatory sponsor integration was established. The demo timings below are our proposed sequences.

[Organizer precedent](https://www.compiled.sh/articles/gstax-x-gbrain-hackathon-1) favors specific products, close prompt fit, pressure-testing ideas, and clear pitches. That is useful preparation evidence, not an additional official rubric.

## Recommendation

**Choose Apply Once for this team if a 25-minute spike proves receipt recovery.** Kevin's existing application-workflow experience supplies realistic failure cases, but existing project code cannot be entered under the shared guide. Build one small implementation from scratch. This is the most ambitious software option and best aligned with that experience.

Known has the strongest human story if real telephone access is ready. Backfill has the clearest small-business outcome if a test calendar and voice calling already work. Neither telephony nor calendar integration was verified in this assessment. At approximately 2:53 PM Pacific, there were about 3 hours 37 minutes until submission, so integration readiness should override small differences in subjective scores.

## 1. Apply Once — a job search worker that survives failure

**Track:** Unsupervised. **User:** A student who wants selected, suitable applications completed while occupied elsewhere.

**Promise:** Give it approved openings, a profile, and limits; return to completed work, receipts, and a clear account of unfinished items.

**Core mechanism:** A persistent work queue; answers tied to profile evidence; bounded retry and spend policies; outcome reconciliation after submission; duplicate prevention. The model handles varying wording and context. Code enforces limits and state transitions. A screen lets judges inspect evidence; the user does not supervise each step.

### Judge-visible demonstration

**First 30 seconds:** Issue a bounded assignment using a fictional applicant and three clearly labeled test openings. Show one browser application completing. A genuine server-side test receipt appears.

**Next 45–60 seconds:** Let the judge choose a failure. The strongest option accepts the next application on the server but drops its response. The agent observes uncertainty, checks the receipt/history, and records completion without creating a second submission. The test server's ledger confirms one accepted application. This is proof about the controlled service, not every employer portal.

**Closing 30 seconds:** A required qualification lacks supporting evidence. The agent preserves/defer that application, continues other eligible work, and stops at its job/time/cost limit. Show observed totals: confirmed, deferred, uncertain, duplicate submissions, interventions, duration, and measured or clearly estimated spend. Do not script a claimed success count.

**Memorable moment:** The browser says the submission failed; the agent establishes what actually happened before acting again.

### Rubric fit

- **Scalability:** Reusable queue and reconciliation layer, with separately tested adapters for supported forms. Report cost per confirmed result and the real support boundary.
- **Innovation:** Demonstrated recovery from ambiguous outcomes, evidence-based answers, and correct stopping. Basic auto-apply already exists in [Simplify](https://help.simplify.jobs/en/help/articles/1784339-get-started-with-autopilot); novelty is a product hypothesis, not a proven market first.
- **Impact:** Completed, appropriate work with fewer repeated fields and avoidable duplicates. A receipt does not prove interview or hiring benefit.

### Feasibility and cut list

**Required:** Browser automation, one authorized model runtime, one fictional profile, a controlled application service with real writes and readable receipts, persistent storage. Runtime/account access is unconfirmed. Kevin's workflow knowledge is an advantage; broad employer compatibility is unproven.

**Riskiest dependency:** Distinguishing a lost response from a rejected submission without blind retries.

**25-minute spike:** Build the narrow application/receipt path, accept a submission, drop the response, and reconcile using an independently read receipt. Pass if five repetitions produce exactly one accepted application each, and an inaccessible receipt remains uncertain without resubmission. This is a proposed test, not a passed one.

**Minimum scope:** One form family, three supplied openings, one profile, persistent queue, two genuine failure scenarios, receipt view, stop limits. Leave a background run active during development and report its actual duration; a compressed stage demo does not establish hours of autonomy.

**If the spike fails:** Narrow to draft preparation and deduplication and disclose the reduced claim. That is materially weaker; choose another candidate if its integration spike succeeds.

**Cut:** Broad discovery/scraping, multiple ATS platforms, recruiter messaging, cover-letter styling, account creation, and elaborate dashboards. Unsupported required facts remain deferred; approved defaults and supported deductions remain distinct from confirmed facts. No real application action is authorized by this ideas brief.

## 2. Known — family verification through an ordinary phone call

**Track:** No App. **User:** A family member who receives an urgent impersonation claim and needs help checking it without navigating software.

**Promise:** Call one familiar helper number, explain the claim, and hear what an independently contacted family member says.

**Core mechanism:** Spoken intake, a previously enrolled family contact, an outbound verification call to that stored number, structured confirmation/denial/no-answer state, and a spoken result. Caller-supplied contact details cannot replace the enrolled contact. This follows the [FTC's known-number callback guidance](https://consumer.ftc.gov/consumer-alerts/2023/03/scammers-use-ai-enhance-their-family-emergency-schemes).

### Judge-visible demonstration

**First 30 seconds:** A teammate playing the recipient phones the dedicated helper: “Someone says my grandson needs money urgently. Can you check?” Their screen stays unused after dialing. The assistant identifies the enrolled person and starts verification.

**Next 30–45 seconds:** The consenting teammate playing the relative receives an actual call on another phone and denies making the request. The recipient hears the discrepancy and can request a trusted-family connection. A judge can change the claimed urgency or wording live.

**Closing 20–30 seconds:** Demonstrate an unanswered callback. The result stays unverified. Show call-event timestamps and actual outcomes on an observer display.

**Memorable moment:** A second physical phone rings; the independent response changes what the recipient hears.

### Rubric fit

- **Scalability:** Telephone access and enrolled-contact configuration can be replicated; unit economics depend on actual call duration and completion rates. Language expansion remains future work.
- **Innovation:** A complete spoken verification workflow, including uncertainty and escalation. Do not claim a new voice-clone detector or universal scam screening.
- **Impact:** Makes an established protective action easier to perform. Demonstrate successful verification or honest uncertainty; do not invent prevented-loss or fraud-accuracy statistics.

### Feasibility and cut list

**Required:** A provisioned inbound number, outbound calling, two or three consenting test participants, pre-enrolled test contact, conversational audio. None verified here.

**Riskiest dependency:** Reliable independent calling and delivery of the result.

**25-minute spike:** Complete an actual inbound request, outbound trusted callback, and spoken result three times without developer intervention. Also test timeout. Pass only if no-answer remains unverified and a changed caller-supplied number is not used.

**Minimum scope:** Dedicated hotline, one relative, confirmation/denial/timeout, spoken result. This does not intercept calls automatically on arbitrary existing mobile numbers.

**If the spike fails:** Use separate sequential calls rather than live bridging if that still proves the interaction. If real telephony cannot be established, switch candidates; browser audio alone does not prove the proposed telephone product.

**Cut:** Carrier interception, custom voice cloning, biometric identification, family graph visualization, payments, broad fraud detection, and native mobile apps.

## 3. Backfill — fill a canceled salon appointment by voice

**Track:** No App. **User:** A salon owner who cannot stop serving customers to call a waitlist.

**Promise:** Say which slot opened; the agent finds a compatible customer and returns a confirmed booking.

**Core mechanism:** Voice intent becomes a calendar mutation, eligibility matching, consented waitlist calling, temporary reservation, atomic booking, and spoken confirmation. Model language handling is constrained by deterministic availability and service-duration checks.

### Judge-visible demonstration

**First 30 seconds:** The owner says, “My three o'clock haircut canceled. Fill it.” A connected test calendar really opens the slot; the first test customer receives an offer.

**Next 30–45 seconds:** The first customer cannot arrive in time for the full service. The agent declines that mismatch and contacts a second consenting participant. Let the judge play that customer and answer naturally.

**Closing 30 seconds:** The customer accepts. The system creates one booking and speaks confirmation. Replay a stale acceptance; it cannot overwrite the booking or reserve the slot twice. Judges inspect the calendar event and booking identifier.

**Memorable moment:** The owner continues a hands-busy task while two real calls resolve an empty appointment.

### Rubric fit

- **Scalability:** Repeatable service-duration, availability, reservation, and contact rules across shops. Demonstrate how configuration changes, not an unsupported worldwide rollout claim.
- **Innovation:** Complete conversational rebooking and conflict handling. [Square already provides waitlists and opening notifications](https://squareup.com/help/us/en/article/7923-waitlist-with-square-appointments), so notifications alone are weak differentiation.
- **Impact:** A confirmed occupied slot is directly measurable. State any service value as the demo shop's assumed price, not cash actually earned.

### Feasibility and cut list

**Required:** A real connected test calendar, callable voice service, pre-enrolled consenting test customers, booking-state storage. Access unconfirmed.

**Riskiest dependency:** Completing a real call-to-calendar loop while preventing conflicting confirmations.

**20-minute spike:** A spoken cancellation causes a call; acceptance creates a calendar event; a replayed acceptance creates no duplicate. Pass if the recorded event matches service duration, customer, and slot. Proposed, not executed.

**Minimum scope:** One salon, one service, two customers, one calendar, sequential offers, expiry, reservation, confirmation. No end-user screen requirement.

**If the spike fails:** Keep one calendar and sequential calls; cut calendar-platform breadth and rescheduling. If only text replies work, reassess entry under Unsupervised instead of claiming No App.

**Cut:** Payments, full CRM, speculative revenue dashboards, many booking platforms, bulk outreach, native apps.

## Comparison and decision

Subjective scores, 1–5, for the narrowly scoped versions. These are neither organizer weights nor probabilities. Feasibility assumes access can be established early.

| Criterion | Apply Once | Known | Backfill |
| --- | --- | --- | --- |
| Demo clarity | 5: ambiguous outcome resolved visibly | 5: physical callback and changed result | 5: empty slot becomes booked |
| Usefulness | 5: recurring student burden | 5: consequential family interaction | 5: identifiable business loss |
| Technical substance | 4: state, semantics, recovery | 3: multi-call workflow | 4: scheduling, reservations, conversations |
| Distinctive behavior | 4: judge-selected failure, truthful recovery | 4: spoken independent verification | 3: familiar category, stronger completion |
| Track fit | 5: limits, stuck states, independent progress | 5: entirely spoken user path | 5: hands-busy owner and spoken customer path |
| Feasibility in remaining time | 3: fresh browser implementation | 3: phone access gates progress | 3: phone and calendar access gate progress |

Matthew's transferable method is visible here: one specific user, one important transformation, proof behind the claim, and a ruthless cut list. SIC's awards are overall, so do not add integrations just to accumulate sponsor names.

Select Apply Once if its recovery spike works. If it fails, promote whichever phone candidate already has a proven end-to-end integration; Backfill has the clearer buyer, while Known has the stronger emotional demonstration. A failed spike should change the choice before visual polish consumes the build window.
