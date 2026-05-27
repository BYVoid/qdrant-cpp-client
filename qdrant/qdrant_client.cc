#include "qdrant/qdrant_client.h"

#include <chrono>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/support/channel_arguments.h>

#include "qdrant/proto/qdrant_messages.pb.h"
#include "qdrant/proto/qdrant_service.grpc.pb.h"

namespace qdrant {
namespace {

using Clock = std::chrono::steady_clock;

double MsSince(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start)
      .count();
}

void CheckGrpc(const grpc::Status& status, std::string_view op) {
  if (!status.ok()) {
    throw std::runtime_error(std::string(op) + " failed: " +
                             status.error_message());
  }
}

}  // namespace

class QdrantClient::Impl {
 public:
  explicit Impl(std::string_view target) {
    grpc::ChannelArguments args;
    args.SetMaxReceiveMessageSize(64 * 1024 * 1024);
    auto channel = grpc::CreateCustomChannel(
        std::string(target), grpc::InsecureChannelCredentials(), args);
    points_ = qdrant::Points::NewStub(channel);
    collections_ = qdrant::Collections::NewStub(channel);
  }

  void EnsureCollection(
      std::string_view collection, int dimensions, bool recreate,
      const std::function<void(std::string_view)>& create_indexes) {
    qdrant::CollectionExistsRequest exists_request;
    exists_request.set_collection_name(collection);
    qdrant::CollectionExistsResponse exists_response;
    grpc::ClientContext exists_context;
    CheckGrpc(collections_->CollectionExists(&exists_context, exists_request,
                                             &exists_response),
              "Qdrant CollectionExists");

    if (exists_response.result().exists() && recreate) {
      qdrant::DeleteCollection delete_request;
      delete_request.set_collection_name(collection);
      delete_request.set_timeout(120);
      qdrant::CollectionOperationResponse delete_response;
      grpc::ClientContext delete_context;
      CheckGrpc(collections_->Delete(&delete_context, delete_request,
                                     &delete_response),
                "Qdrant DeleteCollection");
    } else if (exists_response.result().exists()) {
      if (create_indexes) {
        create_indexes(collection);
      }
      return;
    }

    qdrant::CreateCollection create_request;
    create_request.set_collection_name(collection);
    create_request.set_timeout(120);
    auto* params = create_request.mutable_vectors_config()->mutable_params();
    params->set_size(static_cast<std::uint64_t>(dimensions));
    params->set_distance(qdrant::Cosine);
    qdrant::CollectionOperationResponse create_response;
    grpc::ClientContext create_context;
    CheckGrpc(
        collections_->Create(&create_context, create_request, &create_response),
        "Qdrant CreateCollection");

    if (create_indexes) {
      create_indexes(collection);
    }
  }

  bool CollectionExists(std::string_view collection,
                        std::chrono::milliseconds timeout) {
    qdrant::CollectionExistsRequest request;
    request.set_collection_name(collection);
    qdrant::CollectionExistsResponse response;
    grpc::ClientContext context;
    if (timeout > std::chrono::milliseconds::zero()) {
      context.set_deadline(std::chrono::system_clock::now() + timeout);
    }
    CheckGrpc(collections_->CollectionExists(&context, request, &response),
              "Qdrant CollectionExists");
    return response.result().exists();
  }

  UpsertTiming UpsertPoints(std::string_view collection,
                            const std::vector<qdrant::PointStruct>& points,
                            bool wait) {
    UpsertTiming timing;
    const auto proto_start = Clock::now();
    qdrant::UpsertPoints request;
    request.set_collection_name(collection);
    request.set_wait(wait);
    request.mutable_points()->Reserve(static_cast<int>(points.size()));
    for (const qdrant::PointStruct& point : points) {
      *request.add_points() = point;
    }
    timing.proto_bytes = request.ByteSizeLong();
    timing.proto_build_ms = MsSince(proto_start);

    const auto grpc_start = Clock::now();
    qdrant::PointsOperationResponse response;
    grpc::ClientContext context;
    CheckGrpc(points_->Upsert(&context, request, &response), "Qdrant Upsert");
    timing.grpc_call_ms = MsSince(grpc_start);
    timing.qdrant_reported_ms = response.time() * 1000.0;
    if (response.has_result()) {
      if (response.result().has_operation_id()) {
        timing.operation_id = response.result().operation_id();
      }
      timing.status = response.result().status();
    }
    return timing;
  }

  std::vector<qdrant::ScoredPoint> Search(const qdrant::SearchPoints& request,
                                          QdrantSearchTiming* timing) {
    QdrantSearchTiming local_timing;
    QdrantSearchTiming& search_timing =
        timing == nullptr ? local_timing : *timing;
    search_timing.request_proto_bytes = request.ByteSizeLong();

    qdrant::SearchResponse response;
    grpc::ClientContext context;
    const auto grpc_start = Clock::now();
    CheckGrpc(points_->Search(&context, request, &response), "Qdrant Search");
    search_timing.grpc_call_ms = MsSince(grpc_start);
    search_timing.qdrant_reported_ms = response.time() * 1000.0;
    search_timing.response_proto_bytes = response.ByteSizeLong();
    search_timing.result_count = response.result_size();

    std::vector<qdrant::ScoredPoint> results;
    results.reserve(response.result_size());
    for (qdrant::ScoredPoint& point : *response.mutable_result()) {
      results.push_back(std::move(point));
    }
    return results;
  }

  void CreateFieldIndex(std::string_view collection, std::string_view field,
                        qdrant::FieldType type) {
    qdrant::CreateFieldIndexCollection request;
    request.set_collection_name(collection);
    request.set_wait(true);
    request.set_field_name(field);
    request.set_field_type(type);
    qdrant::PointsOperationResponse response;
    grpc::ClientContext context;
    const grpc::Status status =
        points_->CreateFieldIndex(&context, request, &response);
    if (!status.ok() &&
        status.error_code() == grpc::StatusCode::ALREADY_EXISTS) {
      return;
    }
    CheckGrpc(status, "Qdrant CreateFieldIndex");
  }

 private:
  std::unique_ptr<qdrant::Points::Stub> points_;
  std::unique_ptr<qdrant::Collections::Stub> collections_;
};

QdrantClient::QdrantClient(std::string_view target)
    : impl_(std::make_unique<Impl>(target)) {}
QdrantClient::~QdrantClient() = default;

void QdrantClient::EnsureCollection(
    std::string_view collection, int dimensions, bool recreate,
    const std::function<void(std::string_view collection)>& create_indexes) {
  impl_->EnsureCollection(collection, dimensions, recreate, create_indexes);
}

bool QdrantClient::CollectionExists(std::string_view collection,
                                    std::chrono::milliseconds timeout) {
  return impl_->CollectionExists(collection, timeout);
}

UpsertTiming QdrantClient::UpsertPoints(
    std::string_view collection,
    const std::vector<qdrant::PointStruct>& points, bool wait) {
  return impl_->UpsertPoints(collection, points, wait);
}

std::vector<qdrant::ScoredPoint> QdrantClient::Search(
    const qdrant::SearchPoints& request, QdrantSearchTiming* timing) {
  return impl_->Search(request, timing);
}

void QdrantClient::CreateFieldIndex(std::string_view collection,
                                    std::string_view field,
                                    qdrant::FieldType type) {
  impl_->CreateFieldIndex(collection, field, type);
}

}  // namespace qdrant
