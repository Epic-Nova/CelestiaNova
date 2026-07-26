# CoreDNSOrchestrator Content

This is the extension-owned, safe migration target for the sanitized CoreDNS QuickStart material. It contains a render-only action contract, a Corefile template, a zone template, and a strict input schema.

## Ownership and boundaries

- `CoreDNSOrchestrator` validates zone input, renders configuration plans, and will eventually coordinate apply/reload with `DockerOrchestrator` or a host runtime.
- `KeyForge` resolves `keyforge://` references only during a future, explicitly confirmed apply operation. This content never accepts or stores a private key, certificate PEM, token, password, or provider credential.
- `CanvasCore` may render menus for the action, but it does not interpret DNS rules or execute the plan.
- `ContentForge` discovers this extension-owned content; a root content pack may later select it for a concrete deployment.

## Current contract

`Actions/coredns-zone-management.json` is intentionally `dryRunOnly`. A valid invocation uses `mode: "dry-run"` and produces a configuration plan. It performs no key generation, file writes, service reload, DNS update, or systemd operation.

The `Auto`, `Minimal`, `Normal`, and `Advanced` system-architect profiles may control which values a user must specify. They must not remove baseline DNS health, validation, rollback planning, or security requirements.

## Migration notes

The original sanitized scripts mixed rendering, SOA mutation, DNSSEC key generation, archival, ownership changes, and systemd management. Those privileged and destructive operations are deliberately not copied here. Future actions must split them into independently validated plans with explicit confirmation and an audited apply implementation.
