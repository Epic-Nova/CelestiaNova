# Local Auth API test routing and live-hosting gates

## What local-test routing does

The local VM slice may opt into its co-located Auth API with:

```bash
sudo /usr/local/lib/celestianova/configure-local-auth-api-test
```

This writes the root-owned, credential-free `/etc/celestianova/runtime.env`:

```ini
CELESTIA_AUTH_API_BASE_URL=http://127.0.0.1:8081
CELESTIA_LOCAL_TEST_MODE=1
```

Both systemd units and `celest` load it. Auth API calls that otherwise use
`https://auth.api.epicnova.net` are then routed to that exact local API base.
Plain HTTP is accepted only in this explicit mode, only for that configured
base and only below `/api/v1/`. Without the file, the production route remains
HTTPS-only.

This is intentionally a same-machine daemon test setting. An operator browser
on the Windows host can use the VM's host-only IP to visit the device-flow
approval page, but a Windows Celestia client must explicitly set its own local
test base; it never inherits the VM's loopback route. Use:

```powershell
.\Utilities\windows\configure_local_auth_api_test.ps1 http://<vm-host-only-ip>:8081
```

The explicit local-test switch is also required before the Windows KeyForge
and HTTPAgent surfaces will send OAuth device-flow credentials over that local
HTTP endpoint.

## Local slice now covers

- installation of Docker and the Celestia daemon;
- ContentForge materialisation of the Auth API;
- KeyForge-injected deployment secrets;
- local Compose pull/build/start, status and progress reporting;
- routing a daemon's Auth API requests to its local Auth API container;
- browser-driven Nova ID device-flow testing against the local API.

## Still separate before a real live deployment

These are deliberately not bypassed by local-test routing:

- **Production ingress:** DNS/CoreDNS, TLS, firewall policy and a reverse proxy;
- **production Auth API pack:** non-Sail runtime, immutable image build/publish,
  persistent data, backup/restore, migrations, queues and scheduled work;
- **daemon-to-Auth API transport:** Linux HTTPAgent still requires a real secure
  transport implementation for OAuth-bearing remote/update calls. The local
  route is a test contract, not permission to send tokens over arbitrary HTTP;
- **Mesh remote control:** two daemon instances, node certificates, an exposed
  authenticated Mesh endpoint, Auth API instance discovery/registration and
  primary-election persistence;
- **operations:** health/readiness policy, metrics/log shipping, alerts,
  resource limits, rolling deployment and rollback;
- **public update channel:** signed artifacts on CDN/object storage plus a
  production HTTPS manifest endpoint.

The next isolated test after local Auth API login is therefore a two-node
MeshCore test: Windows client plus VM daemon, both using a deliberate local
test Auth API configuration and a generated development CA. That validates
remote-control semantics without conflating them with public DNS or production
TLS.
