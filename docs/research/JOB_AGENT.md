# Job agent: a path to the strongest SIC entry

Design recommendation, October 24, 2026. No implementation or application activity was performed for this brief.

## Product promise

Give the agent a bounded job-search assignment and return to suitable applications with verified outcomes and a clear exception report. It keeps making progress when individual applications fail, reuses known answers across unfamiliar wording, and respects time, cost and submission limits.

Example mission, using fictional preferences: “Find up to three summer data internships in San Diego or remote. Use my approved profile. Work for twenty minutes within this spending cap.” Eligibility and fit must be explainable from the posting and profile, not represented by a fabricated probability of getting hired.

Enter **Unsupervised**. The [official event](https://luma.com/compiled-im3b) explicitly asks for agents that run independently and recognize loops, overspending, being stuck and when to stop. A judge-facing monitor is compatible with this track. Voice input is optional; phone integration is unnecessary for the central claim.

## Competitive position

[Simplify documents automatic application submission, resume tailoring and cautious handling of uncertain answers](https://help.simplify.jobs/help/articles/1784339-getting-started-with-autopilot). These features alone are not a defensible novelty claim. The proposed hackathon distinction is a demonstrated bounded worker that completes a goal across a changing queue, isolates exceptions and reconciles uncertain outcomes. This is a product/design hypothesis, not a finding that no competitor offers these behaviors.

## Four substantive changes

1. **A bounded assignment:** capture role/location constraints, approved applicant materials, maximum applications, elapsed-time limit and spending cap. Queue matching roles, exclude exact duplicates, and show reasons for choices. Do not silently broaden the search to increase counts.
2. **Evidence-based answer reuse:** connect each factual answer to its approved source; distinguish explicit facts, authorized defaults and supported deductions. Resolve new wording semantically. Review evidence before deferring an unsupported required assertion; skip unsupported optional fields. Respect the applicant's interruption policy. For Kevin, the current private policy says to defer that application and continue other work without routine applicant-answer questions.
3. **Recovery with preserved state:** bound repeated failures, continue other queue items, and resume after interruption without repeating completed work. After a submission timeout, inspect receipt/history evidence before any retry. If the outcome cannot be established, preserve it as uncertain rather than claiming success or resubmitting.
4. **An evidence-backed result:** distinguish confirmed completion, prepared but unsubmitted, skipped, deferred and submission-uncertain. Include measured elapsed time, actual or clearly labeled estimated cost, and receipt evidence. Prioritize suitable, verified outcomes over raw click or application counts.

## The live demonstration: “Try to break my job hunt”

Use a fictional applicant and clearly labeled test employer forms with real browser interactions, server-side state and receipts. A test receipt proves acceptance by that test service, not submission to a real employer. Any separate real-employer compatibility claim needs independent authorized evidence.

Start one short application visibly, then let a judge choose a challenge:

| Challenge | Behavior to demonstrate | Evidence |
| --- | --- | --- |
| Rephrase a known question | Resolve its meaning from the same approved fact | Correct bound field and source link |
| Require an unsupported credential | Check sources, defer only that application, continue another | No fabricated credential and visible queue progress |
| Lose the response after server acceptance | Reconcile using persisted receipt/history | Exactly one accepted server record |
| Repeat a recoverable error | Hit an attempt/time bound and move on | Bounded attempt count and saved state |
| Reach the spending/time limit | Stop initiating work and report current outcomes | Measured limit enforcement and no unaccounted actions |

Use one judge-selected challenge in a roughly 60–90 second presentation rather than cramming all cases into the live segment. The precise timing must be measured. A normal completion establishes the baseline before the challenge.

Possible closing report template: “N confirmed, N deferred, N uncertain; N interventions; measured cost and elapsed time.” Populate only actual results. Two confirmed and one deferred is a useful target scenario, not an observed result.

## Scope for the 4h45 build window

- One application form family and a small queue of test jobs.
- One synthetic applicant profile with grounded facts and explicit defaults.
- One actual browser worker, persistent application state and duplicate guard.
- A real receipt store and two failure controls: a missing required fact and a response lost after acceptance.
- A minimal observer view showing current action, decision, evidence and time/cost.
- A brief final report; spoken input/output only if the central loop is already reliable.

Cut multi-platform support, mass scraping, resume design, recruiter outreach, interview scheduling, mobile app, CAPTCHA bypass, phone infrastructure and unnecessary multi-agent roles. A single resilient worker is sufficient.

## First executable feasibility check — proposed, not run

Timebox: first 45 build minutes. Complete one test application through the browser, record a real server receipt, and verify it independently. Repeat with the server accepting the application but withholding its response. Pass when the worker reconciles acceptance without creating a second record, or honestly preserves an uncertain state when evidence is unavailable. A hard-coded animation does not pass.

If generic browser adaptation is too unreliable, reduce to one supported form family and retain the real uncertainty/reconciliation mechanism. Explicitly limit the compatibility claim. If a real completion and receipt cannot be established, this design should not be promoted above the currently recommended phone gatekeeper.

## What would earn the #1 recommendation

Within a defined test set: actual completions; correct reuse of answers across changed wording; isolation of unsupported assertions; no duplicate records in the ambiguous-response test; bounded retries/cost; and no manual intervention during the judge-selected recovery. These results would make this my strongest SIC recommendation because the usefulness is immediate and the Unsupervised behaviors are visible.

Run additional unattended cases during the build and record actual duration, task counts, errors and interventions. A short demonstration is not evidence of hours of autonomous reliability. Do not claim improved interview rates without outcome data.

The [document's event guide](https://docs.google.com/document/d/1h76F7lYYbtquW_43lZpmp_dveMFyaB3AZ0_zXf7E6KA/edit?tab=t.bot260rv2uqv) prohibits existing projects/starter code. Use experience with application failures to guide a fresh implementation; do not copy the existing repository. This brief recommends a design and does not select it on the user's behalf or authorize a build.
