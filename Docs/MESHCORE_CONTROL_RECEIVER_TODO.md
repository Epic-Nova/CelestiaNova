# MeshCore control receiver: remaining work

MeshCore can already submit allowlisted, Nova-ID-authorized HTTPS control
commands to another Celestia Nova instance. The remaining counterpart is the
authenticated receiver hosted by the target instance.

## Required receiver slice

1. **Authenticated HTTPS host**
   - Extend NovaAPIService (or a deliberately selected API gateway) with a TLS
     listener for the target Celestia instance.
   - Do not expose an unauthenticated daemon port.
   - Bind the listener to an explicit configured address and use a KeyForge
     certificate reference.

2. **MeshCore control endpoint**
   - Implement the receiver for the declared MeshCore control base path.
   - Verify the Nova-ID bearer signature, expiry, audience, and the required
     `mesh.remote.execute` capability.
   - Match the requested action against the target's local MeshCore allowlist;
     never route arbitrary paths or shell commands.

3. **Command dispatch and receipts**
   - Convert accepted operations into typed orchestrator/service requests.
   - Return a receipt identifier immediately; publish later status transitions
     through the existing status/telemetry surfaces.
   - Persist only non-secret request metadata and bounded output.

4. **Status and audit integration**
   - Expose receiver health and receipt state through StatusApiSurface/PulseCore.
   - Emit an audit event containing caller subject, target, allowlisted action,
     receipt ID, timestamps, and result — never bearer tokens or secrets.

5. **Mutating action policy**
   - Start with read-only commands such as `node.status` and
     `orchestrator.status`.
   - Require an explicit confirmation/policy contract before enabling lifecycle
     or deployment mutations remotely.

## Not required for the first VM deploy test

The Laravel/Auth-API VM deployment, Nova-ID device login, KeyForge secret
materialization, Docker bootstrap, Compose activation, and service-mode status
reporting do not depend on this receiver. MeshCore is only required once one
Celestia Nova instance needs to control another over HTTPS.
