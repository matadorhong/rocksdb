//  Copyright (c) 2013, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
#pragma once

#include <map>
#include <memory>
#include <string>

#include "db/builder.h"
#include "rocksdb/comparator.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "rocksdb/table_properties.h"
#include "table/block_builder.h"

namespace rocksdb {

class BlockBuilder;
class BlockHandle;
class Env;
class Footer;
class Logger;
class RandomAccessFile;
struct TableProperties;

// An STL style comparator that does the bytewise comparator comparasion
// internally.
struct BytewiseLessThan {
  bool operator()(const std::string& key1, const std::string& key2) const {
    // smaller entries will be placed in front.
    return comparator->Compare(key1, key2) <= 0;
  }

  const Comparator* comparator = BytewiseComparator();
};

// When writing to a block that requires entries to be sorted by
// `BytewiseComparator`, we can buffer the content to `BytewiseSortedMap`
// before writng to store.
typedef std::map<std::string, std::string, BytewiseLessThan> BytewiseSortedMap;

class MetaIndexBuilder {
 public:
  MetaIndexBuilder(const MetaIndexBuilder&) = delete;
  MetaIndexBuilder& operator=(const MetaIndexBuilder&) = delete;

  MetaIndexBuilder();
  void Add(const std::string& key, const BlockHandle& handle);

  // Write all the added key/value pairs to the block and return the contents
  // of the block.
  Slice Finish();

 private:
  // store the sorted key/handle of the metablocks.
  BytewiseSortedMap meta_block_handles_;
  std::unique_ptr<BlockBuilder> meta_index_block_;
};

class PropertyBlockBuilder {
 public:
  PropertyBlockBuilder(const PropertyBlockBuilder&) = delete;
  PropertyBlockBuilder& operator=(const PropertyBlockBuilder&) = delete;

  PropertyBlockBuilder();

  void AddTableProperty(const TableProperties& props);
  void Add(const std::string& key, uint64_t value);
  void Add(const std::string& key, const std::string& value);
  void Add(const UserCollectedProperties& user_collected_properties);

  // Write all the added entries to the block and return the block contents
  Slice Finish();

 private:
  std::unique_ptr<BlockBuilder> properties_block_;
  BytewiseSortedMap props_;
};

// Were we encounter any error occurs during user-defined statistics collection,
// we'll write the warning message to info log.
void LogPropertiesCollectionError(
    Logger* info_log, const std::string& method, const std::string& name);

// Utility functions help table builder to trigger batch events for user
// defined property collectors.
// Return value indicates if there is any error occurred; if error occurred,
// the warning message will be logged.
// NotifyCollectTableCollectorsOnAdd() triggers the `Add` event for all
// property collectors.
bool NotifyCollectTableCollectorsOnAdd(
    const Slice& key,
    const Slice& value,
    const Options::TablePropertiesCollectors& collectors,
    Logger* info_log);

// NotifyCollectTableCollectorsOnAdd() triggers the `Finish` event for all
// property collectors. The collected properties will be added to `builder`.
bool NotifyCollectTableCollectorsOnFinish(
    const Options::TablePropertiesCollectors& collectors,
    Logger* info_log,
    PropertyBlockBuilder* builder);

// Read the properties from the table.
// @returns a status to indicate if the operation succeeded. On success,
//          *table_properties will point to a heap-allocated TableProperties
//          object, otherwise value of `table_properties` will not be modified.
Status ReadProperties(const Slice &handle_value, RandomAccessFile *file,
                      const Footer &footer, Env *env, Logger *logger,
                      TableProperties **table_properties);

// Directly read the properties from the properties block of a plain table.
// @returns a status to indicate if the operation succeeded. On success,
//          *table_properties will point to a heap-allocated TableProperties
//          object, otherwise value of `table_properties` will not be modified.
Status ReadTableProperties(RandomAccessFile* file, uint64_t file_size,
                           uint64_t table_magic_number, Env* env,
                           Logger* info_log, TableProperties** properties);

// Seek to the properties block.
// If it successfully seeks to the properties block, "is_found" will be
// set to true.
extern Status SeekToPropertiesBlock(Iterator* meta_iter, bool* is_found);

}  // namespace rocksdb
