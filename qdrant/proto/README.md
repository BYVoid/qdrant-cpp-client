# Qdrant Minimal Proto

These are handwritten minimal subsets of Qdrant's official gRPC protobuf API,
not vendored from upstream. The schema is split in two:

- `qdrant_messages.proto` — all message and enum definitions.
- `qdrant_service.proto` — the `Points` and `Collections` services, importing
  `qdrant_messages.proto`.

Together they keep only the messages, enums, and RPCs used by the client:

- `Collections.CollectionExists`
- `Collections.Delete`
- `Collections.Create`
- `Points.CreateFieldIndex`
- `Points.Upsert`
- Dense vectors, point IDs, payload values, collection config, and field-index
  types required by those calls

The source of truth is Qdrant's upstream protobuf definitions:

- https://github.com/qdrant/qdrant/blob/master/lib/api/src/grpc/proto/points.proto
- https://github.com/qdrant/qdrant/blob/master/lib/api/src/grpc/proto/points_service.proto
- https://github.com/qdrant/qdrant/blob/master/lib/api/src/grpc/proto/collections.proto
- https://github.com/qdrant/qdrant/blob/master/lib/api/src/grpc/proto/collections_service.proto
- https://github.com/qdrant/qdrant/blob/master/lib/api/src/grpc/proto/json_with_int.proto

When updating this file, preserve protobuf wire compatibility with upstream:
field numbers, field types, enum numeric values, package name, service names,
and RPC request/response types must match the official definitions. Unused
optional fields can be omitted from this local subset, but existing field numbers
must not be renumbered.
