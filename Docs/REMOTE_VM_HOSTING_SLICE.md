# Remote VM hosting slice

This slice stages a ContentForge materialized Laravel release on an Ubuntu VM
over SSH. It never copies `.env` files, private keys, or ContentForge-excluded
secret material.

## Contracts

- A content pack declares `remoteDeployment.targetManifest`.
- The target manifest contains only host metadata, a `keyforge://` ownership
  reference, a release root, and a path to a verified `known_hosts` file.
- TerminalAgent uses the local OS SSH agent with `BatchMode=yes` and strict
  host-key checking. Password authentication and `StrictHostKeyChecking=no`
  are deliberately unsupported.
- Remote staging uploads an immutable release into
  `<releaseRoot>/<content-id>/releases/<release-id>`.

## Prepare the VirtualBox VM

1. Create an Ubuntu Server 24.04 VM and configure a host-only or NAT-forwarded
   address reachable from the Windows host.
2. Create the non-root `celestia` user, add its public key to
   `~celestia/.ssh/authorized_keys`, and load the matching private key into the
   Windows SSH agent.
3. Permit only the required bootstrap command through passwordless sudo, or
   install Docker manually. `remote-bootstrap` intentionally uses `sudo -n`
   and refuses password prompts.
4. Verify the VM fingerprint out of band, then add its host key to
   `Configs/SSH/known_hosts`. Do not use `ssh-keyscan` without independently
   checking the fingerprint.
5. Update `Content/RemoteTargets/VirtualBoxUbuntu.json` with the VM address,
   user, and release root. It contains no credential value.

## Current actions

- `--remote-bootstrap auth-api` queues Docker installation/enabling on the
  configured VM.
- `--deploy-remote-content auth-api` materializes a release through
  ContentForge and streams it to the VM.
- `--remote-content-status auth-api` queries the active remote Compose release.
- The Laravel menu exposes the same three actions asynchronously.

## Deliberate boundary

Staging is implemented; activation is not. Before `docker compose up` may be
executed remotely, KeyForge needs to resolve the deployment's secret
references into a short-lived `.runtime.env` on the target and Nova ID needs a
registered OAuth client for Celestia Nova. This avoids a remote deployment
accidentally running with copied developer credentials or literal secrets in
content JSON.
