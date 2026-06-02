---
title: "Folly (Meta) Source-Level Deep Analysis"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# Folly (Meta) Source-Level Deep Analysis

> Folly is Meta's (formerly Facebook) C++ foundation library. Source code is located at `references/impl/folly/folly/`.

Folly's core components cover strings, hash tables, zero-copy buffers, async frameworks, and thread-safe containers—each more thoughtfully designed than its standard library counterpart.

## Table of Contents

| Chapter | Source Path | Content |
|---------|-----------|---------|
| [fbstring Three-Tier Storage](/libraries/folly/fbstring) | `FBString.h` | Small/Medium/Large three tiers, COW reference counting, page-crossing optimization |
| [F14 Hash Table](/libraries/folly/f14) | `container/F14*.h` | Chunk-based layout, SIMD tag probing, comparison with SwissTable |
| [IOBuf Zero-Copy Buffer](/libraries/folly/iobuf) | `IOBuf.h` | Circular doubly-linked list, reference counting, custom free callbacks |
| [Future/Promise Async Framework](/libraries/folly/future-promise) | `futures/Future.h`, `Promise.h` | Core shared state, four-state machine, SemiFuture/Future, Executor |
| [Synchronized and Tools](/libraries/folly/synchronized-tools) | `Synchronized.h`, `Function.h` | Type-safe lock guards, move-only Function SBO |

## Standard Library Comparison Overview

| Component | Folly | Standard Library | Key Difference |
|-----------|-------|-----------------|----------------|
| string | fbstring three-tier (Small/Medium/Large COW) | std::string two-tier (Small/Long) | SSO=23 vs 15/22 |
| hash_map | F14 (chunk + SIMD) | unordered_map (node-based chaining) | F14 is more cache-friendly |
| byte_buffer | IOBuf (reference counting + chained) | vector\<char\> (copy semantics) | Zero-copy |
| async | Future/Promise + Executor | std::future (blocking) | Full continuation support |
| thread_safe | Synchronized\<T\> (RAII) | Manual mutex + lock_guard | Compile-time enforced locking |
| callable | folly::Function (SBO, move-only) | std::function (SBO, copy-only) | Supports move-only |
