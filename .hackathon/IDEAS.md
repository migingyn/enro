# SIC idea comparison — October 24, 2026

Latest follow-up: [three ideas applied to past-winner research](SIC_SHORTLIST.md). It conditionally recommends the failure-recovery job agent for this team, alongside the narrowed verification hotline and voice appointment filler. The earlier comparison below is preserved as background; no feasibility test has passed yet.

## Recommendation

Choose the trusted-callback phone gatekeeper from **Scam prevention Track 2:** for the **No App** track, conditional on proving the real phone flow early. The strongest behavior is a spoken family-verification workflow: a caller claims to be a relative, the system contacts the relative through a previously saved number, and the recipient hears the verified response or an explicit inconclusive result. Voice similarity, a CAPTCHA, and suspicious keywords are not identity proof.

The appointment cancellation filler is the strongest lower-complexity alternative, especially if calendar and messaging access already work. The café restocker is a strong third candidate if real supplier test access exists. These are judgments about this hackathon, not probabilities of winning or claims of market novelty.

This review did not build software, provision accounts, place calls, contact customers, alter the Google Doc, or execute feasibility tests. Team skills, hardware, credentials, and remaining build time are not confirmed.

## Event evidence

- [Official SIC / c0mpiled-16 event](https://luma.com/compiled-im3b): Invisible Interfaces; four tracks; teams of 1–4. Current schedule says hacking 1:45–6:30 PM Pacific, a 4h45 window.
- No App explicitly excludes an experience requiring users to look at a screen. SMS alone is not sufficient for this track.
- Unsupervised explicitly asks for prolonged autonomous work and recognition of loops, overspending, being stuck, and when to stop.
- [Document Tab 11](https://docs.google.com/document/d/1h76F7lYYbtquW_43lZpmp_dveMFyaB3AZ0_zXf7E6KA/edit?tab=t.bot260rv2uqv) lists scalability, innovation, and real-world impact as judging domains; this rubric was not separately published on Luma. Tab 11 gives a 1:30 start, so the published sources differ by 15 minutes.
- Tab 11 says existing projects and starter code are not allowed; the old job-agent repository proposed in Tab 1 would therefore require a fresh implementation.
- The tab named Hackathon Guide links to [InnovateLA's Dual-Use Builder's Kit](https://www.innovatelosangeles.com/dual-use/kit), a different event. Its 48-hour guidance, sponsor perks, and judging system are not SIC rules.

## Coverage of all 13 tabs

| Tab | Contents and assessment |
| --- | --- |
| Tab 11 | Current event guide; used for the document's rubric and build rules. |
| Tab 13 | Detailed calendar-aware AI wake-up call, conversational snooze, personalities. Clear demo and feasible narrow scope, but weaker innovation and impact than the leading choices. |
| Tab 1 | Tracks plus scooter dispatch, cleaner dispatch, scam screening, companion AI, job agent, family calls, solar. Cleaner coordination is tangible; dispatch needs a real changing job/feed and actual assignment. Job automation cannot simply reuse existing code under the guide. |
| Expanded ideas | GLOW household task memory; surplus food/time/capacity routing; movement-based accessibility and safety alerts. Physical GLOW could be distinctive with ready hardware. A virtual home weakens proof of an ambient physical interface. Accessibility ideas have meaningful need but calibration and reliability are untested. |
| Track Solar idea | Surplus-aware charging, charger marketplace, battery delivery, flexible computing. Real compute jobs under changing simulated solar are the most feasible version without hardware; physical energy-control claims remain unproven without devices. Direct trading and charger marketplaces add infrastructure dependencies. |
| Automation track 3 | Café restocking with budget, supplier fallback, verified order, and stop conditions; also apartment tours and group coordination. Strong closed workflow, but supplier access is the critical risk. |
| Scam prevention Track 2: | Voice gatekeeper, CAPTCHA ideas, safe word, known-number verification, family escalation. Best impact/demo/track combination after narrowing to trusted callbacks and handling uncertainty honestly. |
| Fun ideas | House ghost, ship pen pal, commute commentator. Ghost offers a memorable physical/voice demo if a bulb is ready. Bus rerouting has clearer utility than ship commentary but requires reliable route and live data. A talking ship alone does not coordinate physical movement for Dispatch. |
| Track 3 unsupervised market | Seller agent with minimum price, buyer negotiation, unknown-condition escalation, and single-buyer commitment. Excellent interactive demo, but marketplace access and end-to-end publishing are substantial dependencies. |
| Zoom Agent | Transcript-to-SMS action items. Easy to explain, but insufficient distinctive behavior; text requires screen interaction and a one-shot summary does not establish Unsupervised fit. |
| Small Business Owner Agent | Canceled slot triggers texts to waitlist. Clear business value and a small demonstrable workflow. Needs booking confirmation, expiry and conflict handling beyond existing notification products. |
| Hackathon Guide | Link to another event's builder resources, not an additional idea or SIC authority. |
| ai alarm clock | Short version of Tab 13. The student employment statistic is unverified and not needed for the recommendation. |

## Serious-candidate scores

Scores are subjective 1–5 assessments for a narrow hackathon prototype. Feasibility is provisional; no access or live tests passed. They are not an official rubric or an arithmetic prediction of a winner.

| Criterion | Trusted-callback gatekeeper | Appointment filler | Café restocker |
| --- | --- | --- | --- |
| Demo clarity | 5: caller claim → real callback → spoken result | 4: cancellation → response → booked slot | 4: stockout → fallback supplier → confirmation |
| Usefulness | 5: protects a consequential family interaction | 5: recovers otherwise empty business capacity | 5: prevents avoidable stockouts |
| Technical substance | 4: multi-call state, verification and uncertainty | 4: event handling, matching, expiry and reservation locking | 4: constraints, supplier fallback, verification and budgets |
| Distinctive behavior | 3: family workflow must stand apart from existing screening | 2: basic cancellation notifications already exist | 3: real exception handling is stronger than a purchasing chatbot |
| Rubric / track fit | 5: entirely spoken interaction and clear impact | 4: choose Unsupervised; show recovery and stopping | 4: voice No App path or substantial Unsupervised behavior |
| Feasibility | 3: real phone routing is the gating dependency | 4: narrow workflow, conditional on calendar/messaging access | 2: supplier interfaces and accepted-order proof may block |

## Candidate 1: trusted-callback phone gatekeeper

**User and problem:** An older adult receives an urgent call from someone claiming to be a family member and should not have to operate an app to check the claim.

**Live result:** When a caller says they are Jake and need urgent money, the system calls the saved Jake contact and tells the recipient what that independent contact actually confirmed, without treating the incoming voice or number as proof.

**30-second storyboard, timing untested:**

1. A teammate calls the project's real test number and makes the family claim.
2. The assistant asks who is calling and why, then initiates verification using a saved contact.
3. A second consenting teammate receives that actual call and says they did not make the first call.
4. The recipient hears the discrepancy and a spoken choice to end the interaction or contact family. The original claim is not presented as verified.

Follow with a legitimate confirmation and a no-answer case; no answer must mean unverified, not fraud. One short scripted demo does not establish real-world fraud-detection accuracy.

**Inputs and permissions:** Two or three consenting test participants, preloaded family contact, inbound/outbound voice service access and microphone/speaker audio. These are required, not confirmed. Use a dedicated test number first; deployment to an existing personal number depends on carrier routing and must not be assumed.

**Riskiest dependency:** Reliable inbound and outbound call orchestration with acceptable latency.

**Proposed spike:** Spend the first 30 build minutes on a real inbound call, a callback to the stored test contact, and a spoken result. Pass if three consecutive attempts complete without a developer manually advancing the flow; separately test a timeout remaining unverified. No spike was run for this evaluation.

**Smallest scope:** One relative, one claim type, saved contact, spoken confirmation/denial/timeout, basic call state and recipient choice.

**Reduced scope:** End the incoming conversation and initiate a separate trusted callback rather than attempting live call bridging. This still tests independent verification. A browser transcript classifier is not equivalent proof of a phone gatekeeper.

**Cut:** Whisper analysis, voice CAPTCHA as authentication, custom voice-cloning detection, support for every carrier, complex family graph, lengthy dashboard, multi-language expansion and vague promises to block all scams.

**Competition and distinction:** [Google's June 2026 announcement](https://blog.google/security/android-fake-call-detection/) describes device verification for impersonated contacts when both use Phone by Google. The proposed opportunity is a spoken family-verification workflow that does not depend on both participants using that app; cross-phone operation still requires testing. Do not claim that independent verification or scam screening is new. [FTC guidance](https://consumer.ftc.gov/consumer-alerts/2023/03/scammers-use-ai-enhance-their-family-emergency-schemes) recommends calling a known number rather than trusting a familiar voice.

## Candidate 2: appointment cancellation filler

**User and problem:** A small business loses a booked appointment and has customers who would accept that opening.

**Live result:** A cancellation triggers eligible waitlist outreach; a customer's reply confirms exactly one booking, and stale replies cannot double-book the slot.

**30-second storyboard:** Cancel a clearly labeled test appointment; notify a consenting test customer; the customer accepts; show a real new booking in the connected test calendar. Then demonstrate a second acceptance being rejected or offered an alternative.

**Inputs and permissions:** A test business calendar, consented waitlist with availability, working messaging or phone access. No actual customer list or service access is confirmed.

**Riskiest dependency:** Real event delivery and customer-reply ingestion. Messaging provisioning can still consume the available time.

**Proposed spike:** 20–30 minutes to cancel a test event, receive one real reply, and write a booking back to the test calendar. Pass when duplicate/late acceptance cannot create a second booking. Not executed.

**Smallest scope:** One shop, one service length, three test customers, one booking source, offer expiry, confirmed booking and a bounded contact policy.

**Reduced scope:** One calendar integration and sequential offers instead of integrations with many booking platforms. A local test calendar can prove scheduling logic but must not be represented as a live commercial-platform integration.

**Cut:** Full CRM, payments, business analytics, many booking systems and indiscriminate bulk outreach.

**Track:** Unsupervised if it persists through declines/timeouts and stops on confirmation, exhaustion or budget limits. If entering No App, replace required SMS interaction with an entirely spoken customer and owner flow.

**Competition:** [Square already supports waitlists and notifications when openings occur](https://squareup.com/help/us/en/article/7923-waitlist-with-square-appointments). The stronger product is a completed conversational rebooking workflow with conflict/expiry handling, not simply sending a cancellation text.

## Candidate 3: café restocking agent

**User and problem:** A café worker needs a specific supply before opening while keeping total cost within an approved limit.

**Live result:** A spoken request for two cases of oat milk under a budget produces an accepted test order from an available supplier; when no supplier meets the constraints the system stops and explains why.

**30-second storyboard:** Give the voice request; the first supplier reports out of stock; the system finds a compliant alternative; an accepted order appears in the supplier test system. Run a lower budget case that stops without ordering.

**Inputs and permissions:** Exact product identity, stock/delivery/price data, approved suppliers, and an actual supplier sandbox or explicitly labeled test supplier endpoint. The doc's suggestion that the team knows café operations is useful context, not verified team experience. Access is unconfirmed.

**Riskiest dependency:** Verifying an accepted order through a usable supplier interface.

**Proposed spike:** 30 minutes to read one product's inventory and place/verify one sandbox order. Pass only on an externally confirmed test order identifier, with retries unable to duplicate it. No test was run.

**Smallest scope:** One product, two suppliers, one deadline, delivered-cost cap, one verified order and an impossible-budget stop condition.

**Reduced scope:** A clearly disclosed test supplier service proving constraints, ordering and verification; this does not validate commercial supplier integration. If no credible order path exists, choose the appointment filler instead.

**Cut:** Payments, arbitrary web shopping, complete inventory forecasting, dozens of products and multiple stores.

## Why the other concepts do not lead

- **Alarm:** Good fallback for audience enjoyment, but calendar-aware conversational alarms already overlap the proposal. [WakeYa](https://wakeya.app/) advertises live AI conversations and calendar context; it explicitly says it does not place a phone call, so do not conflate in-app audio with a telephone call. Calendar-based urgency and snooze negotiation are the strongest parts of the document's version.
- **Marketplace seller:** Could enter the top three if supported marketplace access already works. Otherwise the impressive negotiation demo risks hiding unproven listing and transaction integration.
- **Solar:** A physical device responding to surplus, missing data and deadlines could rank higher with ready hardware. Without that, flexible compute is the more truthful live proof, using real jobs and explicitly simulated energy data.
- **GLOW / house ghost:** Ready lights and sensors materially change feasibility. A rendered glowing virtual home does not prove an ambient home system. The ghost's playful personality is a good presentation reference, not itself the technical contribution.
- **Ship / commute:** Conversational live data is engaging; rerouting or coordinating movement must change an actual decision to justify Dispatch. Merely narrating AIS or GPS is weaker.
- **Zoom:** Useful but thin as written. Needs consequential follow-through and a distinctive screen-free or autonomous workflow to compete here.

## What changes the recommendation

Prefer the appointment filler if the phone spike fails or calendar/messaging integration is already available and reliable. Promote the café restocker if supplier access is ready and the team has real firsthand operations insight. Promote a hardware concept only with the relevant working devices in hand. The selected idea still needs a short scope discussion before implementation; this evaluation does not authorize a build.

## Follow-up: job-applying agent

As initially described in Tab 1, the job-applying agent ranks fourth, above the marketplace seller, conditional on a fresh implementation. A stronger version could become the leading recommendation if it demonstrates real browser completion, grounded answer reuse, independent progress around exceptions, ambiguous-submission reconciliation and bounded stopping. See [the focused design brief](JOB_AGENT.md). Its proposed tests have not been executed, so the original recommendation has not been replaced by an untested promise.
