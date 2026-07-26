# ContentForge content contract

ContentForge discovers deployable application packs from `Content/ContentPacks/` and extension-owned packs from `Extensions/<extension>/Content/ContentPacks/`.

## Service-mode Git content

A content pack can declare a Git source instead of a developer-machine path.
ContentForge accepts only an HTTPS repository, a safe declared ref, and a
matching `allowedHosts` entry. It asks GitAgent to make a shallow clone into
the ContentForge runtime root:

`$CELESTIA_RUNTIME_ROOT/sources/<content-id>/<ref>/`

In Linux service mode this is `/var/lib/celestianova/content`. Existing source
caches are reused without a pull or fetch, so a running host cannot change its
application source merely because a remote branch changed. Materialized
releases remain copy-only and still exclude `.env` and private key material;
KeyForge owns the later secret-injection stage.

An incomplete cache is intentionally fail-closed. A future content-maintenance
action, with an audited cache replacement policy, is required to refresh it.

Extension-owned content belongs beside its implementation:

```text
Extensions/<extension>/Content/
  Actions/     # declarative actions owned by that extension
  Schemas/     # validated action/context inputs
  Templates/   # parameterized, non-secret artifacts
  Policies/    # safe defaults and execution policy
```

Root `Content/` contains application, API, and website packs that are operated by extensions. It is not a secret store.

Every job receives a typed injection context. `public` and `derived` values can be rendered into a staged release. `secretRefs` must use `keyforge://...`; only KeyForge may resolve them at execution time, and their resolved values must never be written to templates, command lines, state files, or logs.

Configuration depth (`auto`, `minimal`, `normal`, `advanced`) controls how many architecture decisions are exposed to the operator. It never removes platform capabilities from a deployment.
