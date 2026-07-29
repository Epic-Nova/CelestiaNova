# Hosting test readiness

## Verified on the VirtualBox test VM

- Celestia Nova builds as a relocatable Linux package and runs as the
  `celestianova` systemd service.
- The service writes an atomic status snapshot below
  `/var/lib/celestianova/status/`.
- ContentForge discovers the declarative `auth-api` pack.
- ContentForge can acquire the Auth API from its declared source and
  materialize a non-secret release below
  `/var/lib/celestianova/content/auth-api/`.
- Docker Engine and Docker Compose v2 were installed through the restricted,
  root-owned DockerOrchestrator bootstrap helper. The `celestianova` account
  can use both Docker and Compose.
- Application releases exclude `.env` and private key material by design.
- `celest` provides both an interactive FTXUI command console and a
  non-interactive scripting surface.
- NovaAPIService exposes daemon health, extension status, and progress on the
  loopback-only status API (`127.0.0.1:9080`).

## Required before the clean-VM Auth API hosting test

1. Install the current package on the VM.
2. Create systemd-encrypted KeyForge credentials for the Auth API values
   declared in `Content/ContentPacks/AuthApiLocal.json`:

   - `keyforge://content/auth-api/app-key`
   - `keyforge://content/auth-api/db-password`

   Use the packaged, root-only utility:

   ```bash
   sudo /opt/celestianova/share/celestianova/bootstrap/initialize_keyforge_credential.sh \
     keyforge://content/auth-api/app-key
   ```

   Repeat for the database password, then restart `celestianova.service`.
   The helper deliberately writes files without a `.cred` suffix: systemd's
   `keyforge:` destination prefix then maps them to the credential names that
   KeyForge reads. Credential values are encrypted at rest and exposed only
   in the service's private credential directory at runtime.

3. Supply all non-secret Auth API deployment values required by its Compose
   file, including database host/name/user and the chosen persistent-volume
   layout. These belong in declarative Content configuration, not in KeyForge.

4. Run the final local lifecycle test:

   - materialize a new Auth API release;
   - inject the KeyForge runtime environment;
   - start the Compose project through DockerOrchestrator;
   - run migrations using the Auth API's two explicit migration connection
     paths;
   - verify the Auth API health endpoint, Compose status, and
     `http://127.0.0.1:9080/api/v1/status`;
   - stop and restart the release to confirm persistence and status recovery.

The packaged installer also installs `celestianova-auth-api-deploy.service`.
Start it with `sudo systemctl start celestianova-auth-api-deploy`; it is a
15-minute, synchronous one-shot transaction and succeeds only after Compose
has accepted the Auth API project.

## Required for remote control after the local host test

- Configure Auth API OAuth applications and KeyForge credentials for
  `celestianova-mesh-daemon` and `celestianova-syncforge`.
- Implement the MeshCore receiver on NovaAPIService (the outgoing MeshCore
  client already exists).
- Bind it only through a configured TLS listener and a KeyForge certificate
  reference; do not expose a plaintext daemon port.
- Validate Nova ID bearer signature, expiry, audience and
  `mesh.remote.execute` capability through the Auth API contract.
- Start with read-only allowlisted commands (`node.status` and
  `orchestrator.status`), durable non-secret receipts, status integration and
  audit metadata. Lifecycle/deployment mutations require a separate explicit
  policy/confirmation contract.

## Required before daemon auto-update can be enabled

- Configure the Auth API update manifest and package/signature URLs over HTTPS.
- Install the root-owned SyncForge package applier. It must verify the manifest
  SHA-256 and signature, stage the package under a fixed directory, atomically
  replace the package, and restart the service only after verification.
  The packaged `apply-syncforge-update` and `queue-syncforge-update` helpers
  now provide this flow; production only needs the generated public update key
  at `/etc/celestianova/update-trust.pem` plus HTTPS artifact URLs in Auth API.

## Deliberate non-goals for this test

- Remote deployment activation and remote secret writing.
- Nova ID device-flow completion against a production TLS Auth API.
- Any arbitrary package installation, arbitrary SSH command execution or
  unauthenticated remote-control listener.
