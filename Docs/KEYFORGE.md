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
expiry metadata. The local-test exception requires explicit
`CELESTIA_LOCAL_TEST_MODE=1` and an exact configured local `/api/v1/` base;
production endpoints remain HTTPS-only.

## Deployment materialization

KeyForge writes a remote runtime environment only to
`<release>/.runtime.env` with mode `0600`. It validates references, keeps
secret bytes out of command lines and output, and fails closed until an
authenticated stdin-only remote writer is available. Remote staging may occur
without that writer; remote Compose activation must not.
