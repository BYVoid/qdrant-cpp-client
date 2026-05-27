#include "qdrant/qdrant_client.h"

int main() {
  qdrant::UpsertTiming timing;
  return timing.operation_id == 0 ? 0 : 1;
}
