# LaravelOrchestrator Content

This directory contains extension-owned, declarative Laravel deployment content.
It is discovered through the `contentRoots` entry in `LaravelOrchestrator.json`.

## Contents

- `Templates/laravel/`: rendered into a content pack's deployment workspace.
- `Actions/deploy-laravel.json`: describes the non-interactive deployment plan. The
  orchestrator owns execution; Canvas only supplies the action and context.
- `Schemas/laravel-deployment-context.schema.json`: validates the render context
  before any file or Docker command is produced.

## Rendering contract

Templates use `{{variable.path}}` tokens. ContentForge must resolve every token
from a validated context before materialization. Missing values are an error;
there are deliberately no hidden defaults for identity, paths, domains, or
secrets.

Secret-bearing values are references only, for example
`keyforge://projects/auth-api/app-key`. KeyForge resolves those references at
execution time and writes the resulting environment file with restricted
permissions. Rendered templates, logs, Canvas requirement payloads, and
ContentForge state must never contain the resolved secret.

## Profile depth

`auto`, `minimal`, `normal`, and `advanced` control how much configuration a
system architect supplies. They do not remove infrastructure from the deployment
model. The resolved context always has an application, database, cache/queue,
network/routing, persistent storage, health checks, and TLS-routing settings.
`auto` derives safe values from the content pack and host policy; the other
levels progressively expose those values for explicit review.

## Safety boundary

This content does not run migrations, issue certificates, alter host cron jobs,
or delete Docker resources. Those operations require separate, explicit actions
with confirmation policy and are intentionally not migrated from the legacy
scripts in this first pass.
