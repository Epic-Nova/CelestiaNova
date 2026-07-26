# ContentForge content contract

ContentForge discovers deployable application packs from `Content/ContentPacks/` and extension-owned packs from `Extensions/<extension>/Content/ContentPacks/`.

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
