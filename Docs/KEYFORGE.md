# KeyForge credential contract

KeyForge is the only component permitted to resolve `keyforge://` references.
Content packs, Canvas menus, ContentForge state, TerminalAgent commands and
logs retain only public metadata or references.

## Local stores

On Windows, KeyForge protects its local vault at
`Content/.runtime/keyforge-v1.dpapi` with DPAPI CurrentUser. It is not
portable and must remain outside source control. `Configs/KeyForge/LocalVault.json`
contains only non-secret endpoint wiring. Seed a bootstrap secret through
`Utilities/Initialize-KeyForgeBootstrap.ps1`; it is never printed or written
to JSON.

When `CELESTIA_LOCAL_TEST_MODE=1` is configured, KeyForge derives the local
`/api/v1/oauth/provision-application`, `device-authorize` and `device-token`
endpoints from `CELESTIA_AUTH_API_BASE_URL`. The operator still supplies the
Auth API provisioning bootstrap secret interactively; endpoint convenience
does not weaken secret ownership.

For the local Auth API hosting pack, initialize
`keyforge://content/auth-api/oauth-provisioning-key` on the daemon with the
same value that is seeded as the Windows DPAPI bootstrap reference
`keyforge://bootstrap/auth-api/provisioning`. This value is required only for
KeyForge's idempotent OAuth application provisioning endpoint and is never
placed in Content or an `.env` default.

On Linux service mode, administrators place encrypted systemd credentials in
`/etc/celestianova/credentials`. systemd exposes decrypted values only in the
daemon's private credential directory. KeyForge reads references from there
and fails closed when a value is absent.

## OAuth and Nova ID

For every API integration, the owning extension requests an OAuth application
lease. KeyForge owns client ID/secret storage, obtains short-lived tokens and
makes the secret-bearing token request internally. Access tokens and client
secrets never cross an extension ABI, reach Canvas, status data or logs.

Interactive Nova ID uses the same broker: KeyForge starts device
authorization and returns only the verification URI, user/device codes and
expiry metadata. It later performs the secret-bearing device-token poll and
returns the short-lived access token only to the in-memory owning session.
The local-test exception requires explicit
`CELESTIA_LOCAL_TEST_MODE=1` and an exact configured local `/api/v1/` base;
production endpoints remain HTTPS-only.

## Local Nova ID bootstrap flow

The `auth-api` local content pack intentionally enables an additional,
container-local `NOVA_LOCAL_LOGIN_BYPASS` switch. It is for the VirtualBox
integration slice only and must not be copied to a public deployment profile.
It avoids needing an external credential-provider service while preserving the
normal Nova ID device-authorization sequence.

The local Auth API pack runs its two database migration groups and creates the
idempotent local administrator automatically after Compose starts. If an
operator needs to repair an existing legacy release manually, `celest status`
lists the active release path; replace `<release>` below with that path:

```bash
sudo docker compose -f <release>/compose.yaml exec -T laravel.test \
  php artisan nova:local-admin --name=Admin
```

The command prints the administrator's `Authenticatable identifier`. Use that
value to obtain a short-lived user bearer in Postman (the bypass means no
password is stored or sent):

```http
POST http://<vm-ip>:8081/api/v1/authentication/login
Content-Type: application/json

{
  "authenticatable_identifier": "<printed-uuid>",
  "provider_identifier": "com.epicnova.authentication_provider.local-bypass",
  "input": "local-test",
  "login_bypass": true
}
```

When Celestia displays a device verification URL, open it to read the
`user_code`, then approve it with the user bearer:

```http
POST http://<vm-ip>:8081/api/v1/oauth/device-approve
Authorization: Bearer <access_token>
Content-Type: application/json

{ "user_code": "<code-from-url>" }
```

Return to the Celestia Canvas or run `celest progress`; KeyForge polls the
device token internally and the Nova ID session becomes authenticated. The
user bearer and device code are short-lived; client credentials and the
provisioning key remain inside KeyForge only.

## Deployment materialization

KeyForge writes a remote runtime environment only to
`<release>/.runtime.env` with mode `0600`. It validates references, keeps
secret bytes out of command lines and output, and fails closed until an
authenticated stdin-only remote writer is available. Remote staging may occur
without that writer; remote Compose activation must not.
