# First VirtualBox test: Auth API from a shared folder

## Goal and boundary

This is the smallest realistic Linux validation: install and run the Laravel
Auth API on an Ubuntu VM using files made available through a VirtualBox shared
folder. It deliberately does **not** use Celestia remote deployment, SSH
automation, MeshCore, service mode, or a Linux Celestia Nova build.

The shared folder is only a transport for the source/release artifact. Runtime
files, writable application state, and secrets must live on the VM filesystem,
not in the shared mount.

## What must be prepared on the Windows host

1. Create a VirtualBox shared folder containing the Auth API project (or a
   release archive created from it). Do not put `.env`, private keys, vendor
   dependencies, caches, or runtime logs into the shared folder.
2. Attach the folder to the Ubuntu VM with a fixed name, for example
   `auth-api-source`.
3. Keep the Auth API source compatible with Linux path and permission rules.
   The actual Linux/WSL build and Celestia package are a later phase.

## What the Ubuntu VM needs

1. Ubuntu 24.04 (or the chosen supported Ubuntu release), network access, and
   VirtualBox Guest Additions so the shared folder can mount.
2. A non-root application user, for example `authapi`.
3. PHP 8.4 or later (the test VM currently provides PHP 8.5) with the required
   extensions, Composer, MariaDB, and Redis. The Auth API has two named MariaDB
   connections: `AuthConnection` and `RestrictionsConnection`; it is not a
   single generic PostgreSQL application.
4. A web entrypoint: Nginx plus PHP-FPM is the intended production-shaped
   choice. `php artisan serve` is acceptable only for a short functional
   smoke-test.
5. TLS for any flow that contains OAuth or KeyForge credentials. A locally
   trusted development certificate is sufficient for this isolated VM test,
   but plain HTTP must not be used for the real device-login/KeyForge path.

## Installation flow on the VM

1. Mount the shared folder read-only if practical.
2. Copy the project from the mount into a VM-owned release directory, for
   example `/opt/auth-api/releases/initial`. This makes Linux ownership and
   Laravel writable directories predictable.
3. Create a VM-local `.env` with the VM URL, Redis/queue choice, and
   non-development credentials. Configure both `AUTH_DB_*` and
   `RESTRICTIONS_DB_*` values for local MariaDB databases; keep the file outside
   the shared folder and restrict it to the application user.
4. Install PHP dependencies on Linux with Composer using the production flags
   appropriate to the selected environment; do not reuse a Windows `vendor`
   directory.
5. Ensure `storage` and `bootstrap/cache` are writable by the PHP-FPM user.
6. Generate or securely provide `APP_KEY`, create the `Auth` and
   `Restrictions` databases, then run the explicit migration paths:
   `database/migrations/AuthConnection` using `AuthConnection`, followed by
   `database/migrations/RestrictionsConnection` using
   `RestrictionsConnection`. Clear/cache Laravel configuration around that
   connection switch.
7. Configure Nginx to serve only the Laravel `public` directory and pass PHP
   requests to PHP-FPM. Bind the VM address deliberately and terminate TLS.
8. Start/reload PHP-FPM and Nginx, then verify the existing versioned API
   endpoint and capability discovery endpoint over HTTPS.

## Auth and OAuth validation

1. Seed or register a test Nova-ID account using the existing Auth API flow.
2. Set the KeyForge bootstrap secret only in the Auth API VM environment if
   testing application provisioning. Never place it in source control or the
   shared folder.
3. Provision one OAuth application for the eventual Celestia client.
4. Exercise the existing device authorization, approval, and token polling
   flow with Postman against the VM HTTPS address.
5. Confirm that issued tokens, audience checks, capabilities, and expiry are
   enforced by the existing versioned controllers.

## Acceptance criteria

- The Auth API is served from the Ubuntu VM over HTTPS.
- Both database migration sets complete and survive an application restart.
- The API can register/authenticate the designated test user.
- OAuth device authorization can be created, approved, and exchanged for a
  token by the existing API endpoints.
- No secret, writable runtime directory, or Linux dependency is relied on
  directly from the VirtualBox shared folder.

## Explicitly deferred

- Building Celestia Nova for Linux through WSL.
- Installing and running Celestia Nova in Linux service mode.
- Remote deployment, SSH orchestration, Docker bootstrap, and Compose control.
- MeshCore receiver/control-plane communication.
- KeyForge-driven remote runtime environment materialization.

Once this test is stable, the next slice is a WSL/Linux Celestia build and
package, followed by Celestia-managed deployment to the same VM.
