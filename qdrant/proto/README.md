# Qdrant Protos (vendored)

These files are © Qdrant, licensed under [Apache 2.0](LICENSE) (the
`LICENSE` file here is copied from the upstream repository root).

These .proto files are vendored verbatim from upstream Qdrant
**v1.18.2** (`lib/api/src/grpc/proto/` in
https://github.com/qdrant/qdrant/archive/refs/tags/v1.18.2.tar.gz),
with two deliberate deviations:

1. **Import paths**: upstream uses bare imports (`import
   "collections.proto"`); ours are rewritten to
   `import "qdrant/proto/collections.proto"` so the files build from the
   workspace root without `strip_import_prefix` (which would emit
   generic top-level headers like `points.pb.h` that could collide with
   downstream projects).
2. **`qdrant.proto` is trimmed**: upstream's root file imports the
   cluster-internal service protos (raft_service,
   points_internal_service, collections_internal_service,
   qdrant_internal_service, shard_snapshots_service,
   storage_read_service, telemetry_internal) purely to aggregate them
   into one descriptor. Those are node-to-node APIs a client never
   calls, so they are not vendored and the imports are dropped; only
   the root `Qdrant` service (HealthCheck) is kept.

Vendored set (the full public, client-facing API):

- `qdrant_common.proto` — PointId, Filter/Condition/Match, geo, ranges
- `json_with_int.proto` — `qdrant.Value`/`Struct`/`ListValue` (payloads)
- `collections.proto` + `collections_service.proto` — `Collections` service
- `points.proto` + `points_service.proto` — `Points` service
- `snapshots_service.proto` — `Snapshots` service
- `health_check.proto` — standard `grpc.health.v1.Health` service
- `qdrant.proto` — root `Qdrant` service (trimmed, see above)

## Updating to a new upstream release

1. Download `https://github.com/qdrant/qdrant/archive/refs/tags/v<X.Y.Z>.tar.gz`
   and extract `lib/api/src/grpc/proto/`.
2. Copy the files listed above over the ones here (skip `qdrant.proto`).
3. Rewrite local imports:
   `sed -i '' -E 's|^import "(collections\|points\|qdrant_common\|json_with_int)|import "qdrant/proto/\1|' *.proto`
4. Diff upstream `qdrant.proto` against our trimmed copy and port any
   changes to the root service / health-check messages.
5. Copy the tarball's root `LICENSE` over the one here if it changed.
6. Update the version in this README and in `MODULE.bazel`, then run
   `bazel test //...`.
