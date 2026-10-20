# LabBridge development contract

- Work against an accepted user story in `docs/backlog.md`. Add behavior-level acceptance tests before changing behavior. Observe the failing test, implement, then refactor with tests green.
- Preserve the DL24M frame codec and checked recovery semantics. Characterize existing behavior before replacing it; migration is not evidence that a driver works on hardware.
- Core code must not name individual device IDs or depend on ESP-IDF. Drivers own protocol facts; transports own connections; boards own resource and pin limits.
- Modify API contracts and generator templates, never generated output. Run `tools/check.sh` and generation drift checks before publishing a change.
- No real battery operation without a complete configured profile, confirmed output states, an explicit wiring arrangement (optional interlock) and fresh cell-temperature channel. Existing authorization for bounded reversible bench tests persists; record and restore original settings.
- Never publish credentials, NVS/flash backups, manufacturer binaries, APKs or raw private logs. Fixtures contain only sanitized protocol data.
- Keep deployment-specific metadata in ignored local notes: physical USB identities and assignments, installed host rules/aliases, configured network/node names, VM details, backups and cross-session hardware coordination. Public docs and tests use generic instructions and synthetic examples; retain sanitized protocol findings and acceptance limits only.
- Use small story branches and reviewable PRs. Do not merge code with failing required checks. Record real limitations in the backlog; do not replace hardware evidence with simulation.
- No multi-agent delegation unless the user explicitly requests it. Keep generated and vendored changes isolated from hand-written implementation.
