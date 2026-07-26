# KeyForge remote deployment contract

KeyForge is the only component allowed to resolve `keyforge://` references.
Content packs, menus, ContentForge state, TerminalAgent commands, and logs hold
only public metadata or references.

For every API deployment, its owning orchestrator calls
`EnsureOAuthApplication`. The returned client-id and client-secret references
are stable ownership handles, not credential values. A configured KeyForge
vault backend then performs the Auth API client registration once, stores the
one-time secret, and exposes it only while materializing the runtime file.

For interactive Nova ID login, menus call `BeginDeviceAuthorization` on this
same broker. KeyForge performs the secret-bearing Auth API request and returns
only the short-lived verification URI, user code, device code, expiry, and
polling interval. Canvas opens the verification URI; NovaID retains the device
code in its in-memory session and polls through KeyForge. No menu, JSON file,
or TerminalAgent command receives the OAuth client secret or an access token.
The present implementation exposes this contract but fails closed until an Auth
API device-authorization adapter is configured.

`MaterializeRemoteRuntimeEnvironment` has one destination: a release-local
`<release>/.runtime.env` with mode `0600`. It validates references and fails
closed until both a vault backend and authenticated remote writer are present.
It must not use command-line environment overrides, write a local staging
file, return secret content, or write secrets to job output.

The current repository implements the ABI and validation gate but deliberately
does not include a vault backend or remote secret transport. Therefore a remote
Laravel release may be staged, but must not be compose-activated through this
contract yet. The next implementation binds the KeyForge backend to the Auth
API OAuth registration endpoint and adds a stdin-only, redacted remote writer
to TerminalAgent.
