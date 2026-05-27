#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "qdrant/proto/qdrant_messages.pb.h"

namespace qdrant {

struct UpsertTiming {
  double proto_build_ms = 0;
  double grpc_call_ms = 0;
  double qdrant_reported_ms = 0;
  std::uint64_t proto_bytes = 0;
  std::uint64_t operation_id = 0;
  qdrant::UpdateStatus status = qdrant::UnknownUpdateStatus;
};

struct QdrantSearchTiming {
  double grpc_call_ms = 0;
  double qdrant_reported_ms = 0;
  std::uint64_t request_proto_bytes = 0;
  std::uint64_t response_proto_bytes = 0;
  int result_count = 0;
};

class QdrantClient {
 public:
  explicit QdrantClient(std::string_view target);
  ~QdrantClient();

  // Creates a cosine-distance collection (optionally recreating it). At the
  // point a fixed payload-index step used to run — right after the collection
  // is created, or when it already exists and is not being recreated —
  // `create_indexes` is invoked with the collection name. It is the generic
  // replacement for that step (e.g. call CreateFieldIndex for each field).
  // Defaults to empty.
  void EnsureCollection(
      std::string_view collection, int dimensions, bool recreate,
      const std::function<void(std::string_view collection)>& create_indexes =
          {});
  bool CollectionExists(
      std::string_view collection,
      std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());
  // Batches caller-built points into a single Upsert RPC. The caller owns
  // id, vector, and payload construction; this only handles transport and
  // timing. All points must match the collection's fixed vector dimension.
  UpsertTiming UpsertPoints(std::string_view collection,
                            const std::vector<qdrant::PointStruct>& points,
                            bool wait);
  // Runs a caller-built search request and returns the raw scored points.
  // The caller owns the vector, filter, limit, payload/vector selectors, and
  // any search params; this only handles transport and timing.
  std::vector<qdrant::ScoredPoint> Search(const qdrant::SearchPoints& request,
                                          QdrantSearchTiming* timing = nullptr);
  // Creates a payload field index on the collection. Idempotent: an
  // already-existing index is treated as success. The caller decides which
  // fields to index and with what type.
  void CreateFieldIndex(std::string_view collection, std::string_view field,
                        qdrant::FieldType type);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace qdrant
