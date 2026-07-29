# Celestia Nova universal TODO

This is the sole backlog for unfinished Celestia Nova work. `Docs/` contains
only operating and architecture contracts; completed work is recorded in Git.

## Current local slice — complete baseline

- [x] Linux package, systemd daemon, `celest` console and loopback daemon status API.
- [x] Auth API ContentForge acquisition, KeyForge runtime injection, Docker bootstrap and Compose deployment.
- [x] Non-blocking deployment UX with structured Docker pull/build/start progress.
- [x] Explicit local Auth API routing for the VM and Windows test client.
- [x] Signed SyncForge artifact packaging and root-owned verification/applier flow.

## Immediate local-auth and two-node test

- [x] Implement KeyForge device-token polling and retain the resulting short-lived Nova ID session in memory only.
- [ ] Validate the browser device-login flow against the local Auth API from the Windows client.
- [ ] Implement a Linux secure HTTP transport for OAuth-bearing HTTPAgent requests; local routing does not replace this requirement.
- [ ] Run a two-node development MeshCore test: Windows client plus VM daemon, development CA, explicit allowlist and read-only commands.
- [ ] Persist bounded, redacted Mesh command receipts and expose them in the daemon status surface.

## MeshCore remote-control receiver

- [ ] Host an authenticated TLS receiver on NovaAPIService or the selected gateway; never publish an unauthenticated daemon control port.
- [ ] Verify Nova ID bearer signature, expiry, audience and `mesh.remote.execute` capability.
- [ ] Enforce target-local allowlisted typed actions; never dispatch arbitrary paths or shell commands.
- [ ] Return durable receipt IDs, publish state transitions, and add audit records without tokens or secrets.
- [ ] Start only with `node.status` and `orchestrator.status`; add a separate confirmation/policy contract before remote mutations.
- [ ] Implement Auth API instance registration/discovery, heartbeat, trusted membership and persisted primary election.

## Production Auth API hosting pack

- [ ] Replace Laravel Sail with a production Compose/profile: PHP-FPM or Octane behind Nginx/Caddy and no development bind mounts.
- [ ] Build and publish versioned immutable application images; target nodes pull verified images rather than build application code.
- [ ] Define persistent MariaDB, Redis and RabbitMQ volumes, encrypted backups, restore tests and retention policy.
- [ ] Define migrations, queue workers, scheduler and WebSocket/Soketi lifecycle and readiness checks.
- [ ] Remove test/default runtime values; require all production secrets through KeyForge.
- [ ] Add application health/readiness checks, restart policy, structured logs, metrics and alert rules to the daemon status contract.
- [ ] Implement release validation, atomic switch-over, rollback and retention policy.

## Production platform and security

- [ ] Add domain ownership, CoreDNS records, TLS certificate lifecycle, reverse proxy and firewall policies.
- [ ] Provision dedicated OAuth applications for every API/extension integration and rotate credentials through KeyForge.
- [ ] Define host admission, Mesh identity/certificates, least-privilege service accounts and remote deployment policy.
- [ ] Publish signed updates through the Auth API HTTPS manifest and CDN/object-storage artifacts; validate rollback on failed update.
- [ ] Add observability retention, dashboards, alert routing and recovery runbooks.

## Declarative company infrastructure

- [ ] Define a versioned, signed infrastructure-plan schema with no secret values.
- [ ] Render plan topology and node state from JSON in CanvasCore.
- [ ] Implement dependency-aware, idempotent Unattended Mode with preview, approval, resume and audit receipt.
- [ ] Add server-role contracts for CoreDNS/domain, CDN, gateway, management frontend, databases and observability.
- [ ] Build the Epic Nova site composer, graph export/import and credential-reference mapping workflow.

## Legacy script/content migration

- [ ] Laravel production templates/actions: Compose validation, build, post-deploy, migration, health and rollback policy.
- [ ] Traefik/Nginx templates and safe gateway/certificate actions.
- [ ] Astro deployment profile and declarative scheduler capability.
- [ ] Typed remote deployment action, immutable releases and remote activation policy.
- [ ] MariaDB mirroring policies with backups, PII safeguards and destructive confirmation.
- [ ] CoreDNS plan/validate/apply and certificate distribution workflow.
- [ ] Privileged host bootstrap actions for Docker/CoreDNS and other runtime prerequisites.
- [ ] Certificate-store/PIV design: public distribution first; private CA issuance only with KMS/HSM, audit and dual-control design.

## Definition of done for every TODO

Each completed item needs an owning extension, schema/typed input, dry-run,
policy/confirmation class, asynchronous structured result, redacted logs,
health/status integration, tests, and a documented rollback or explicit
statement that rollback is unavailable.
