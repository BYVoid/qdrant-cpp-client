# qdrant-cpp-client

A small, hand-written C++ gRPC client for [Qdrant](https://qdrant.tech/).

It does **not** vendor Qdrant's full protobuf API. Instead it talks to the
server over a minimal, hand-maintained subset of the upstream gRPC schema
(`qdrant/proto/qdrant_messages.proto` plus `qdrant/proto/qdrant_service.proto`)
covering only the collection and point operations the client needs. See
[`qdrant/proto/README.md`](qdrant/proto/README.md) for the wire-compatibility
contract with upstream.

## Features

The public surface is the `qdrant::QdrantClient` class
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

This package is a [Bazel](https://bazel.build/) module. Add it to your
`MODULE.bazel`:

```starlark
bazel_dep(name = "qdrant_cpp_client", version = "0.1.0")
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

The repo builds with C++20 (set in `.bazelrc`).

## Building and testing this repo

```sh
bazel build //...
bazel test //...
```

`//:qdrant_client_compile_test` is a compile-only smoke test that confirms the
public header is self-contained (it forward-declares the one proto type it
references and pulls in no generated gRPC headers).

## Dependencies

Resolved from the [Bazel Central Registry](https://registry.bazel.build/):

* `grpc` 1.80.0
* `protobuf` 33.4 — capped here because gRPC 1.80.0 still loads
  `@protobuf//bazel:upb_proto_library.bzl`, which protobuf removed in 34.x.
  Bump the two together.
* `abseil-cpp`, `rules_cc`, `rules_proto`

See `MODULE.bazel` for exact versions.
