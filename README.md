# qdrant-cpp-client

A small, hand-written C++ gRPC client for [Qdrant](https://qdrant.tech/),
packaged as a [Bazel](https://bazel.build/) module.

## Protos: vendored from upstream Qdrant

The protobuf schema under `qdrant/proto/` is **not** hand-written — it is
vendored verbatim from the upstream Qdrant repository:

- **Source**: `lib/api/src/grpc/proto/` in
  [qdrant/qdrant **v1.18.2**](https://github.com/qdrant/qdrant/releases/tag/v1.18.2)
  (downloaded from the release tarball, not a git checkout)
- **Scope**: the full public, client-facing API — the `Points`,
  `Collections`, `Snapshots`, root `Qdrant` (health check), and standard
  `grpc.health.v1.Health` services with all their messages.
  Cluster-internal service protos (raft, shard transfer, node-to-node
  point/collection ops) are deliberately excluded; a client never calls
  them.
- **Local deviations** (the only ones): import paths are rewritten to the
  `qdrant/proto/` prefix so the files build from the workspace root, and
  `qdrant.proto` is trimmed of its internal-service imports. See
  [`qdrant/proto/README.md`](qdrant/proto/README.md) for the exact file
  list and the step-by-step upgrade procedure.
- **License**: the vendored protos are © Qdrant, licensed under
  [Apache 2.0](qdrant/proto/LICENSE) (copied from the upstream
  repository).

This module's version (`MODULE.bazel`, currently **1.18.2**) tracks the
upstream Qdrant release the protos were vendored from.

Because the full schema is vendored, all generated message and stub types
(`qdrant::QueryPoints`, `qdrant::ScrollPoints`, quantization configs,
sparse/named vectors, …) are available to downstream code even where the
convenience wrapper below does not expose them.

## The client wrapper

The hand-written public surface is the `qdrant::QdrantClient` class
(`qdrant/qdrant_client.h`):

* `EnsureCollection` — create a cosine-distance collection (optionally
  recreating it) and invoke an optional `create_indexes(collection)` callback
  at the point a fixed payload-index step used to run.
* `CollectionExists` — existence check, with an optional deadline.
* `CreateFieldIndex` — create a payload field index of a given
  `qdrant::FieldType` (idempotent). The caller picks which fields to index.
* `UpsertPoints` — batch caller-built `qdrant::PointStruct` values (id +
  vector + payload) into a single Upsert RPC, returning per-call timing. The
  caller owns point construction; the client only handles transport.
* `Search` — runs a caller-built `qdrant::SearchPoints` request (vector,
  filter, limit, payload/vector selectors, search params) and returns the raw
  `qdrant::ScoredPoint` results, with optional per-call timing.

## Using it from Bazel (bzlmod)

Add it to your `MODULE.bazel`:

```starlark
bazel_dep(name = "qdrant_cpp_client", version = "1.18.2")
```

Then depend on the library and include the header:

```starlark
cc_binary(
    name = "my_tool",
    srcs = ["my_tool.cc"],
    deps = ["@qdrant_cpp_client//:qdrant_client"],
)
```

```cpp
#include <qdrant/qdrant_client.h>

qdrant::QdrantClient client("localhost:6334");
client.EnsureCollection(
    "my_collection", /*dimensions=*/768, /*recreate=*/false,
    [&](std::string_view collection) {
      client.CreateFieldIndex(collection, "symbol", qdrant::FieldTypeKeyword);
    });
```

To use parts of the API the wrapper does not cover, depend on
`@qdrant_cpp_client//:qdrant_cc_proto` (messages) and
`@qdrant_cpp_client//:qdrant_grpc` (service stubs) directly.

The repo builds with C++20 (set in `.bazelrc`).

## Building and testing this repo

```sh
bazel build //...
bazel test //...
```

`//:qdrant_client_compile_test` is a compile-only smoke test that confirms the
public header is self-contained (it includes only the generated message
classes, no generated gRPC service stubs).

## Dependencies

Resolved from the [Bazel Central Registry](https://registry.bazel.build/):

* `grpc` 1.80.0
* `protobuf` 33.4 — capped here because gRPC 1.80.0 still loads
  `@protobuf//bazel:upb_proto_library.bzl`, which protobuf removed in 34.x.
  Bump the two together.
* `abseil-cpp`, `rules_cc`, `rules_proto`

See `MODULE.bazel` for exact versions.
