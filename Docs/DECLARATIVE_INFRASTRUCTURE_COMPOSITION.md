# Declarative infrastructure composition — target vision

Celestia Nova's long-term purpose is to turn a deliberate infrastructure design into a managed, observable company platform. This is an architectural target, not a claim that the current hosting slice is complete.

## Desired operator journey

1. In the official Epic Nova site, an operator composes the desired estate: domain, environments, servers, applications, data stores, gateways, CDN, monitoring, and access policy.
2. The site renders this as an infrastructure graph and exports a signed, declarative infrastructure configuration. The configuration contains no secret values.
3. The operator opens that configuration in Celestia Nova on Windows, Linux, or macOS. CanvasCore renders the same graph locally and permits a final review and host-specific configuration before any apply operation.
4. The operator supplies or maps credentials through KeyForge. The configuration identifies credential references and target hosts; secret bytes stay in KeyForge rather than the graph or exported configuration.
5. Unattended Mode validates the plan, connects to the declared servers, installs the required runtimes and orchestrators, deploys applications, configures DNS through CoreDNS/domain control, and publishes status and operational metadata.
6. Celestia Nova installs and configures the infrastructure-management frontend. That frontend consumes daemon status data and becomes the long-lived management surface for the new estate.
7. Once CDN and artifact services are provisioned, relevant outputs are published as authenticated download links through the newly configured delivery layer.

## Core product surfaces

| Surface | Responsibility |
| --- | --- |
| Epic Nova site | Infrastructure composer, graph editor, signed plan export, reusable templates. |
| Celestia Nova Canvas | Local graph review, per-host credential mapping, plan validation, approval and progress UI. |
| Unattended Mode | Idempotent plan executor, dependency ordering, retries, resumable progress, and audit records. |
| ContentForge | Ownership and acquisition of application, script, template, and configuration content. |
| KeyForge | Secret references, encrypted credential storage, scoped materialization, certificates, and keys. |
| Orchestrators | Install and manage the concrete infrastructure dependencies named by a plan. |
| MeshCore/NexusCore | Multi-node coordination, authority state, remote dispatch, status aggregation, and topology. |
| PulseCore/SignalCore/NovaAPIService | Telemetry, events, and authenticated status/dashboard data transport. |

## Plan requirements

The exported configuration must be declarative, versioned, signed, idempotent, and safe to preview. It should define desired state rather than arbitrary shell commands. Each node must declare its intended role, required capabilities, dependency edges, and credential references. A plan apply must produce a machine-readable receipt that records node status, deployed versions, generated public endpoints, and redacted diagnostics.

Initial plans may assume control of a domain through CoreDNS and a small set of servers. They must still support staged execution: validate, preview graph, bootstrap one node, deploy a service group, verify health, then continue. A failed stage must be resumable without recreating already healthy resources.

## Implementation milestones

1. Define the signed infrastructure-plan schema and versioning policy.
2. Make CanvasCore render plan topology and individual node state from JSON, with no topology hardcoded in C++.
3. Extend Unattended Mode from individual actions to a dependency-aware, persisted execution graph.
4. Complete authenticated Mesh membership, remote command receiving, election, and audit trails.
5. Add CoreDNS/domain, CDN, frontend, and server-role orchestrator contracts.
6. Establish plan preview, approval, rollback, resume, and infrastructure receipt semantics.

The current Auth API Linux hosting slice is a foundation for this: it proves package installation, daemon execution, ContentForge acquisition, KeyForge injection, Docker bootstrap, deployment progress, and status reporting. The full declarative company-infrastructure composer remains a substantial next phase.
