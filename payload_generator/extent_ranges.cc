//
// Copyright (C) 2010 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "update_engine/payload_generator/extent_ranges.h"

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

#include <base/logging.h>

#include "update_engine/common/utils.h"
#include "update_engine/payload_consumer/payload_constants.h"

using std::vector;

namespace chromeos_update_engine {

bool ExtentRanges::ExtentsOverlapOrTouch(const Extent& a, const Extent& b) {
  if (a.start_block() == b.start_block())
    return true;
  if (a.start_block() == kSparseHole || b.start_block() == kSparseHole)
    return false;
  if (a.start_block() < b.start_block()) {
    return a.start_block() + a.num_blocks() >= b.start_block();
  } else {
    return b.start_block() + b.num_blocks() >= a.start_block();
  }
}

bool ExtentRanges::ExtentsOverlap(const Extent& a, const Extent& b) {
  if (a.start_block() == b.start_block())
    return a.num_blocks() != 0;
  if (a.start_block() == kSparseHole || b.start_block() == kSparseHole)
    return false;
  if (a.start_block() < b.start_block()) {
    return a.start_block() + a.num_blocks() > b.start_block();
  } else {
    return b.start_block() + b.num_blocks() > a.start_block();
  }
}

void ExtentRanges::AddBlock(uint64_t block) {
  // Remember to respect |merge_touching_extents_| setting
  AddExtent(ExtentForRange(block, 1));
}

void ExtentRanges::SubtractBlock(uint64_t block) {
  SubtractExtent(ExtentForRange(block, 1));
}

namespace {

Extent UnionOverlappingExtents(const Extent& first, const Extent& second) {
  CHECK_NE(kSparseHole, first.start_block());
  CHECK_NE(kSparseHole, second.start_block());
  uint64_t start = std::min(first.start_block(), second.start_block());
  uint64_t end = std::max(first.start_block() + first.num_blocks(),
                          second.start_block() + second.num_blocks());
  return ExtentForRange(start, end - start);
}

}  // namespace

void ExtentRanges::AddExtent(Extent extent) {
  if (extent.start_block() == kSparseHole || extent.num_blocks() == 0)
    return;

  ExtentSet::iterator begin_del = extent_set_.end();
  ExtentSet::iterator end_del = extent_set_.end();
  uint64_t del_blocks = 0;
  const auto range = GetCandidateRange(extent);
  for (ExtentSet::iterator it = range.begin(), e = range.end(); it != e; ++it) {
    const bool should_merge = merge_touching_extents_
                                  ? ExtentsOverlapOrTouch(*it, extent)
                                  : ExtentsOverlap(*it, extent);
    if (should_merge) {
      end_del = it;
      ++end_del;
      del_blocks += it->num_blocks();
      if (begin_del == extent_set_.end())
        begin_del = it;

      extent = UnionOverlappingExtents(extent, *it);
    }
  }
  extent_set_.erase(begin_del, end_del);
  extent_set_.insert(extent);
  blocks_ -= del_blocks;
  blocks_ += extent.num_blocks();
}

namespace {
// Returns base - subtractee (set subtraction).
ExtentRanges::ExtentSet SubtractOverlappingExtents(const Extent& base,
                                                   const Extent& subtractee) {
  ExtentRanges::ExtentSet ret;
  if (subtractee.start_block() > base.start_block()) {
    ret.insert(ExtentForRange(base.start_block(),
                              subtractee.start_block() - base.start_block()));
  }
  uint64_t base_end = base.start_block() + base.num_blocks();
  uint64_t subtractee_end = subtractee.start_block() + subtractee.num_blocks();
  if (base_end > subtractee_end) {
    ret.insert(ExtentForRange(subtractee_end, base_end - subtractee_end));
  }
  return ret;
}
}  // namespace

void ExtentRanges::SubtractExtent(const Extent& extent) {
  if (extent.start_block() == kSparseHole || extent.num_blocks() == 0)
    return;

  ExtentSet::iterator begin_del = extent_set_.end();
  ExtentSet::iterator end_del = extent_set_.end();
  uint64_t del_blocks = 0;
  ExtentSet new_extents;
  for (ExtentSet::iterator it = extent_set_.begin(), e = extent_set_.end();
       it != e;
       ++it) {
    if (!ExtentsOverlap(*it, extent))
      continue;

    if (begin_del == extent_set_.end())
      begin_del = it;
    end_del = it;
    ++end_del;

    del_blocks += it->num_blocks();

    ExtentSet subtraction = SubtractOverlappingExtents(*it, extent);
    for (ExtentSet::iterator jt = subtraction.begin(), je = subtraction.end();
         jt != je;
         ++jt) {
      new_extents.insert(*jt);
      del_blocks -= jt->num_blocks();
    }
  }
  extent_set_.erase(begin_del, end_del);
  extent_set_.insert(new_extents.begin(), new_extents.end());
  blocks_ -= del_blocks;
}

void ExtentRanges::AddRanges(const ExtentRanges& ranges) {
  // Remember to respect |merge_touching_extents_| setting
  for (ExtentSet::const_iterator it = ranges.extent_set_.begin(),
                                 e = ranges.extent_set_.end();
       it != e;
       ++it) {
    AddExtent(*it);
  }
}

void ExtentRanges::SubtractRanges(const ExtentRanges& ranges) {
  for (ExtentSet::const_iterator it = ranges.extent_set_.begin(),
                                 e = ranges.extent_set_.end();
       it != e;
       ++it) {
    SubtractExtent(*it);
  }
}

void ExtentRanges::AddExtents(const vector<Extent>& extents) {
  // Remember to respect |merge_touching_extents_| setting
  for (vector<Extent>::const_iterator it = extents.begin(), e = extents.end();
       it != e;
       ++it) {
    AddExtent(*it);
  }
}

void ExtentRanges::SubtractExtents(const vector<Extent>& extents) {
  for (vector<Extent>::const_iterator it = extents.begin(), e = extents.end();
       it != e;
       ++it) {
    SubtractExtent(*it);
  }
}

void ExtentRanges::AddRepeatedExtents(
    const ::google::protobuf::RepeatedPtrField<Extent>& exts) {
  // Remember to respect |merge_touching_extents_| setting
  for (int i = 0, e = exts.size(); i != e; ++i) {
    AddExtent(exts.Get(i));
  }
}

void ExtentRanges::SubtractRepeatedExtents(
    const ::google::protobuf::RepeatedPtrField<Extent>& exts) {
  for (int i = 0, e = exts.size(); i != e; ++i) {
    SubtractExtent(exts.Get(i));
  }
}

bool ExtentRanges::OverlapsWithExtent(const Extent& extent) const {
  for (const auto& entry : GetCandidateRange(extent)) {
    if (ExtentsOverlap(entry, extent)) {
      return true;
    }
  }
  return false;
}

bool ExtentRanges::ContainsBlock(uint64_t block) const {
  auto lower = extent_set_.lower_bound(ExtentForRange(block, 1));
  // The block could be on the extent before the one in |lower|.
  if (lower != extent_set_.begin())
    lower--;
  // Any extent starting at block+1 or later is not interesting, so this is the
  // upper limit.
  auto upper = extent_set_.lower_bound(ExtentForRange(block + 1, 0));
  for (auto iter = lower; iter != upper; ++iter) {
    if (iter->start_block() <= block &&
        block < iter->start_block() + iter->num_blocks()) {
      return true;
    }
  }
  return false;
}

void ExtentRanges::Dump() const {
  LOG(INFO) << "ExtentRanges Dump. blocks: " << blocks_;
  for (ExtentSet::const_iterator it = extent_set_.begin(),
                                 e = extent_set_.end();
       it != e;
       ++it) {
    LOG(INFO) << "{" << it->start_block() << ", " << it->num_blocks() << "}";
  }
}

Extent ExtentForRange(uint64_t start_block, uint64_t num_blocks) {
  Extent ret;
  ret.set_start_block(start_block);
  ret.set_num_blocks(num_blocks);
  return ret;
}

Extent ExtentForBytes(uint64_t block_size,
                      uint64_t start_bytes,
                      uint64_t size_bytes) {
  uint64_t start_block = start_bytes / block_size;
  uint64_t end_block = utils::DivRoundUp(start_bytes + size_bytes, block_size);
  return ExtentForRange(start_block, end_block - start_block);
}

vector<Extent> ExtentRanges::GetExtentsForBlockCount(uint64_t count) const {
  vector<Extent> out;
  if (count == 0)
    return out;
  uint64_t out_blocks = 0;
  CHECK(count <= blocks_);
  for (ExtentSet::const_iterator it = extent_set_.begin(),
                                 e = extent_set_.end();
       it != e;
       ++it) {
    const uint64_t blocks_needed = count - out_blocks;
    const Extent& extent = *it;
    out.push_back(extent);
    out_blocks += extent.num_blocks();
    if (extent.num_blocks() < blocks_needed)
      continue;
    if (extent.num_blocks() == blocks_needed)
      break;
    // If we get here, we just added the last extent needed, but it's too big
    out_blocks -= extent.num_blocks();
    out_blocks += blocks_needed;
    out.back().set_num_blocks(blocks_needed);
    break;
  }
  CHECK(out_blocks == utils::BlocksInExtents(out));
  return out;
}

Range<ExtentRanges::ExtentSet::const_iterator> ExtentRanges::GetCandidateRange(
    const Extent& extent) const {
  auto lower_it = extent_set_.lower_bound(extent);
  while (lower_it != extent_set_.begin() &&
         (lower_it == extent_set_.end() ||
          lower_it->start_block() + lower_it->num_blocks() >
              extent.start_block())) {
    --lower_it;
  }

  auto upper_it = extent_set_.upper_bound(
      ExtentForRange(extent.start_block() + extent.num_blocks(), 1));
  while (upper_it != extent_set_.end() &&
         upper_it->start_block() <=
             extent.start_block() + extent.num_blocks()) {
    ++upper_it;
  }
  return {lower_it, upper_it};
}

std::vector<Extent> ExtentRanges::GetIntersectingExtents(
    const Extent& extent) const {
  const auto candidates = GetCandidateRange(extent);
  std::vector<Extent> result;
  for (const auto& ext : candidates) {
    if (auto intersection = GetOverlapExtent(ext, extent);
        intersection.num_blocks() != 0) {
      result.emplace_back(std::move(intersection));
    }
  }
  return result;
}

vector<Extent> FilterExtentRanges(const vector<Extent>& extents,
                                  const ExtentRanges& ranges) {
  vector<Extent> result;
  const ExtentRanges::ExtentSet& extent_set = ranges.extent_set();
  for (Extent extent : extents) {
    // The extents are sorted by the start_block. We want to iterate all the
    // Extents in the ExtentSet possibly overlapping the current |extent|. This
    // is achieved by looking from the extent whose start_block is *lower* than
    // the extent.start_block() up to the greatest extent whose start_block is
    // lower than extent.start_block() + extent.num_blocks().
    auto lower = extent_set.lower_bound(extent);
    // We need to decrement the lower_bound to look at the extent that could
    // overlap the beginning of the current |extent|.
    if (lower != extent_set.begin())
      lower--;
    auto upper = extent_set.lower_bound(
        ExtentForRange(extent.start_block() + extent.num_blocks(), 0));
    for (auto iter = lower; iter != upper; ++iter) {
      if (!ExtentRanges::ExtentsOverlap(extent, *iter))
        continue;
      if (iter->start_block() <= extent.start_block()) {
        // We need to cut blocks from the beginning of the |extent|.
        uint64_t cut_blocks =
            iter->start_block() + iter->num_blocks() - extent.start_block();
        if (cut_blocks >= extent.num_blocks()) {
          extent.set_num_blocks(0);
          break;
        }
        extent = ExtentForRange(extent.start_block() + cut_blocks,
                                extent.num_blocks() - cut_blocks);
      } else {
        // We need to cut blocks on the middle of the extent, possible up to the
        // end of it.
        result.push_back(ExtentForRange(
            extent.start_block(), iter->start_block() - extent.start_block()));
        uint64_t new_start = iter->start_block() + iter->num_blocks();
        uint64_t old_end = extent.start_block() + extent.num_blocks();
        if (new_start >= old_end) {
          extent.set_num_blocks(0);
          break;
        }
        extent = ExtentForRange(new_start, old_end - new_start);
      }
    }
    if (extent.num_blocks() > 0)
      result.push_back(extent);
  }
  return result;
}

Extent GetOverlapExtent(const Extent& extent1, const Extent& extent2) {
  if (!ExtentRanges::ExtentsOverlap(extent1, extent2)) {
    return {};
  }
  const auto start_block =
      std::max(extent1.start_block(), extent2.start_block());
  const auto end_block = std::min(extent1.start_block() + extent1.num_blocks(),
                                  extent2.start_block() + extent2.num_blocks());
  return ExtentForRange(start_block, end_block - start_block);
}

}  // namespace chromeos_update_engine
