# KeyForge local vault

On Windows, KeyForge stores each secret name and value using DPAPI `CurrentUser`
protection in `Content/.runtime/keyforge-v1.dpapi`. The vault is not portable:
only the same Windows user on the same machine can decrypt it. The runtime
folder must remain ignored by source control.

`Configs/KeyForge/LocalVault.json` contains only non-secret wiring. The normal
OAuth provisioning path is `authApiProvisionEndpoint` (HTTPS) plus
`bootstrapSecretReference`. The reference points to a pre-seeded DPAPI vault
entry (by default `keyforge://bootstrap/auth-api/provisioning`); the bootstrap
secret is never present in JSON, logs, menus, a command line, or a returned
lease. KeyForge sends it only as the `X-KeyForge-Bootstrap` TLS request header,
then DPAPI-protects the one-time `client_secret` returned by the Auth API.

Before the first provisioning request, seed the bootstrap secret interactively:
`Utilities/Initialize-KeyForgeBootstrap.ps1`. It prompts with `SecureString`,
creates the vault only when it does not yet exist, and binds the entry to the
current Windows user through DPAPI. It never prints the supplied value.

`authApiRegisterCommand` remains a secondary compatibility fallback for older
Auth APIs. It may contain only `{application}`, `{scopes}`, and `{ttl}`
substitutions and must emit `client_id` plus `client_secret`. Its output stays
in memory and is immediately DPAPI-protected.

Device authorization requires an HTTPS `deviceAuthorizationEndpoint`; HTTP is
rejected.

## Linux service mode

Linux production service mode uses systemd encrypted credentials, placed by an
administrator below `/etc/celestianova/credentials`. systemd decrypts them only
into the daemon's private credential directory. KeyForge reads references from
that directory and fails closed when an expected credential is absent.

KeyForge can broker an OAuth client-credentials request internally: it reads
the registered client secret, obtains a short-lived token, then sends the
authenticated resource request. Neither the client secret nor the access token
crosses an extension ABI, reaches a menu, or is written into the status API.
