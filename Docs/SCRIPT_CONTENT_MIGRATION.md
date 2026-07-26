# Script and Template Content Migration

## Purpose and boundary

This document turns the legacy material in `../SomeScripts`,
`../ScriptVersion/Merged`, `../CoreDNS-QuickStart`, and the historical
certificate-authority material in `../Keys/PIV/CertificateAuthority` into a
content migration plan.

The legacy directories remain **read-only source material** until a target has
been reimplemented, reviewed, and tested. They are not runtime dependencies.
In particular, do not copy an old shell script into an extension and call that
an implementation: every migrated action must receive typed input, render its
files into a per-deployment working directory, validate them, and report a
structured result.

`Content/` at the application root is for deployable user content packs. An
extension may own `Content/` for its templates, action definitions, schemas,
and policies. CanvasCore renders menu definitions only; it must not contain
deployment commands, script-specific conditions, or content-pack knowledge.
The responsible extension implements actions and availability conditions.

## Common injection contract

All migrated templates/actions use one `InjectionContext`, resolved by
ContentForge before execution. Values are grouped by contract category rather
than by a framework-specific `.env` convention.

| Category | Examples | Source / handling |
| --- | --- | --- |
| Identity | `content.id`, `deployment.id`, `site.name`, `framework` | Pack manifest and generated deployment state |
| Paths | `source.path`, `workspace.path`, `release.path`, `data.path` | Normalized absolute paths; never shell-concatenated |
| Runtime | image/tag, service names, ports, Compose project, health URL | Content pack defaults plus explicit system-architect overrides |
| Network | domains, aliases, proxy network, DNS zone, entrypoints | Validated domain/port types; ownership checked by Networking/WebServer extensions |
| Build | command, output directory, package manager, release retention | Framework template schema; commands use an allowlisted action model |
| Database | engine, connection reference, mirror mode, migration policy | Database extension; credentials are secret references only |
| Certificate | issuer, public CA/CRL paths, DNS challenge provider reference | KeyForge/CertificateStore; private material is never rendered from content |
| Remote execution | target identity, SSH port, host key policy, remote base path | TerminalAgent with KeyForge-provided credentials and host verification |
| Policy | dry-run, confirmation class, backup/rollback strategy, actor | Action definition plus authorization/audit context |

Secrets are represented only as opaque `keyforge://...` references. ContentForge
can pass references to the owning extension; only KeyForge may resolve a value
for the short-lived process that needs it. Rendered manifests, status views,
logs, and Canvas menus must redact them.

### Configuration depth profiles

`Auto`, `Minimal`, `Normal`, and `Advanced` define how much a system architect
has to configure explicitly. They do **not** remove infrastructure capabilities.
Every deployment still has runtime, health checks, persistent storage where
required, networking/domain support, TLS/certificates where applicable, and
observability. `Auto` derives safe values; the other levels progressively expose
and require more of the same contract fields. A content pack declares which
fields are derivable, required, or optionally overridable at each level.

## Target extension content layout

Each target below follows this layout where applicable:

```text
Extensions/<owner>/Content/
  Templates/       # parameterized, non-secret source files
  Actions/         # action contracts; no raw UI logic
  Schemas/         # JSON schemas for pack/action/injection validation
  Policies/        # confirmation, authorization, redaction, rollback rules
  Examples/        # sanitized fixtures only
```

Root `Content/ContentPacks/` references these extension-owned capabilities by
stable IDs. It describes a deployable application/site, not its implementation.

## Migration matrix

| Legacy source | New owner and target content | Required injection categories | Safety / migration notes | Order |
| --- | --- | --- | --- | --- |
| `ScriptVersion/Merged/config/global.yaml` | `TraefikOrchestrator/Content/Schemas/global-hosting.yaml` and validated global hosting configuration | Network, Certificate, Paths, Runtime, Policy | Replace fixed `/opt` and email values with typed defaults. Do not expose the dashboard by default. | 2 |
| `ScriptVersion/Merged/config/sites/example-laravel.yaml` | `LaravelOrchestrator/Content/Examples/site.yaml` + site schema | Identity, Paths, Network, Build, Runtime, Certificate | Becomes a framework deployment profile example, not a runtime config file. | 1 |
| `ScriptVersion/Merged/docker/frameworks/laravel/docker-compose.template.yml` | `LaravelOrchestrator/Content/Templates/compose.yml.template` | Identity, Paths, Runtime, Network, Database, Certificate | Render then run `docker compose config`; project/service names must be generated and validated. | 1 |
| `ScriptVersion/Merged/docker/php/*`, `Dockerfile`, nginx templates, `docker-compose*.yml` | LaravelOrchestrator owns PHP/Laravel assets; `NginxOrchestrator` owns generic Nginx templates; DockerOrchestrator validates/runs final Compose | Runtime, Paths, Build, Network, Database, Policy | Split duplicate variants into named capabilities. Preserve no implicit destructive `chown`, migration, or port binding behavior. | 1 |
| `ScriptVersion/Merged/scripts/frameworks/laravel/docker-service.sh` and `post-deploy-fix.sh` | LaravelOrchestrator actions: `validate`, `post-deploy`, `migrate`, `health-check` | Runtime, Paths, Database, Policy | `migrate --force`, filesystem permission changes, cache clears, and optional SQLite creation are separate confirmation-classed steps. Capture rollback/backup prerequisites. | 1 |
| `ScriptVersion/Merged/scripts/docker-service.sh` | DockerOrchestrator actions: `compose.validate`, `compose.start`, `compose.stop`, `compose.status`, `compose.logs`, `compose.exec` | Runtime, Paths, Policy | Only the DockerOrchestrator invokes Docker/Compose. `exec` requires an explicit command allowlist and audit trail. | 1 |
| `ScriptVersion/Merged/docker/frameworks/astro/*`, `config/sites/example-astro.yaml`, `scripts/frameworks/astro/docker-service.sh` | AstroJSOrchestrator templates, schemas, and lifecycle actions | Identity, Paths, Runtime, Network, Build, Certificate, Policy | Model static output and nginx behavior explicitly; validate output directory before serving. | 3 |
| `ScriptVersion/Merged/scripts/deploy-remote.sh` | New shared `CoreOrchestrator` remote-deployment action contract, executed by TerminalAgent; framework orchestrators provide build/release phases | Remote execution, Identity, Paths, Build, Policy | Remove prompts. Require target fingerprint/host-key policy, dry-run plan, immutable release ID, atomic activation, retention cleanup confirmation, and structured command results. | 4 |
| `ScriptVersion/Merged/scripts/traefik-stack.mjs` | TraefikOrchestrator renderer/action: `render-stack`, `validate-stack`, `apply-stack`, `health-check` | Network, Certificate, Paths, Runtime, Policy | Keep dashboard disabled unless explicitly authorized and bind it only to approved hosts. Validate ACME storage permissions and external network ownership. | 2 |
| `ScriptVersion/Merged/scripts/issue-cert-bypass-traefik.sh` | TraefikOrchestrator coordinated certificate-renewal action, with KeyForge certificate references | Network, Certificate, Runtime, Policy | High-impact: stops listeners on 80/443. Require hostname/domain ownership check, port-conflict preview, explicit confirmation, backup of current certificates, restoration plan, and audit event. | 5 |
| `SomeScripts/crontab.txt` and PHP cron files | New JobScheduler capability under `CoreOrchestrator`, with LaravelOrchestrator job definitions | Identity, Runtime, Paths, Policy | Jobs are declarative schedules with concurrency and timeout policies; no direct crontab mutation from a pack. | 3 |
| `SomeScripts/DATABASE_MIRRORING.md`, `novaverse-mirror.sh`, `db-tunnel.cmd` | MariaDBOrchestrator `mirror` policies/actions; TerminalAgent tunnel action; LaravelOrchestrator database profile mapping | Database, Remote execution, Paths, Policy | `full`/`wipe` are destructive and require explicit target selection, snapshot/backup plan, PII sanitization policy, and post-action verification. Never embed DB password or SSH private-key paths. | 4 |
| `SomeScripts/dns.sh` and `CoreDNS-QuickStart/SanitizedDNS/*` | CoreDNSOrchestrator zone templates/actions; CertificateStore owns public certificate distribution separately | Network, Certificate, Paths, Policy | Zone generation is previewable and signed/serial-validated. Reload only after `named`/CoreDNS validation. Sanitized examples may migrate; private key files do not. | 5 |
| `CoreDNS-QuickStart/Setup/*.sh` | DockerOrchestrator installation prerequisite capability, called by CoreDNSOrchestrator | Runtime, Remote execution, Policy | Host package installation is privileged; it must be an admin-approved host bootstrap action, not automatic content deployment. | 6 |
| `SomeScripts/create_ca.sh`, `cleanup_ca.sh`, `renew_piv.sh`, `mail.sh`, and PIV CA scripts | New `CertificateStoreOrchestrator` (or KeyForge-owned CertificateStore capability) with KeyForge | Certificate, Identity, Remote execution, Policy | **Do not migrate private CA keys, passphrases, device state, or issuance databases into Content.** Reimplement public-cert/CRL distribution first; issuance/renewal requires approval, HSM/KMS design, audit log, and dual control. | 7 |
| `SomeScripts/usbipd-yubikey.cmd`, `wsl-ssh-pageant-amd64-gui.exe` | TerminalAgent + KeyForge host integration documentation/actions | Remote execution, Certificate, Policy | Windows/WSL/YubiKey integration is host-local and opt-in. No binary should be copied into deployable content; checksum and provenance are required before use. | 7 |
| `SomeScripts/novaverse-mirror.sh` and CA `novaverse-mirror.sh` | Split: MariaDB mirror action vs. archive-only historical CA copy logic | Database, Paths, Policy | Do not retain a shared ambiguous script. Give each operation a specific action ID and destructive classification. | 4 / 7 |
| `ScriptVersion/Merged/merge_backups/**` and PHP `variants/**` | `Docs/Legacy/` provenance only; selected behavior becomes tests/fixtures in its owner extension | None at runtime | Never execute or ship these as templates. They document merge history and conflicting alternatives. | Archive |

## Required action behavior

Every migrated action must provide:

1. Schema validation before rendering or execution.
2. A dry-run plan listing files, network listeners, containers, remote hosts, and destructive effects.
3. An explicit confirmation class: `none`, `standard`, `destructive`, or `privileged`.
4. Structured asynchronous results (`accepted`, `running`, `succeeded`, `failed`) so Canvas menus stay responsive.
5. Redacted logs, status/health probes, and a rollback or an explicit statement that rollback is unavailable.
6. Content and template version IDs recorded in deployment state for reproducibility.

## Delivery order

1. **Laravel vertical slice:** Compose/PHP/Nginx template separation, Laravel actions, root content-pack references, Docker validation/execution, and a test deployment.
2. **Traefik baseline:** global schema, renderer, external network validation, health/status, safe dashboard policy.
3. **Astro and scheduled jobs:** static-site template/action plus declarative scheduler content.
4. **Remote deployments and MariaDB mirroring:** TerminalAgent contract, immutable releases, safe DB-mirror policies.
5. **DNS and certificate renewal:** CoreDNS plan/apply/validate; coordinated ACME workflow.
6. **Host bootstrap:** Docker/CoreDNS installation and other privileged host changes.
7. **Certificate store/PIV:** public distribution first, then separately approved issuance/renewal design.

## Definition of done for each row

A legacy artifact is considered migrated only when the target extension owns a
schema, sanitized template/action, validation test, dry-run, safety policy,
documentation, and a root content-pack can reference it without hardcoded
application names or local paths. The legacy copy is then retained as
provenance, never executed by Celestia Nova.
