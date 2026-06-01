<script setup lang="ts">
import { ref, watch } from 'vue'
const props = defineProps<{ code?: string }>()
const innerCode = ref(props.code ?? '')
const running = ref(false)
const opened = ref(false)
const output = ref('')
const asmText = ref('')
const exitOk = ref(true)
watch(() => props.code, (v) => { if (v) innerCode.value = v })

const COMPILER = 'g142'
const STD = 'c++20'
const FLAGS = '-O2'

const DEFAULT_INCLUDES = `#include <iostream>
#include <iomanip>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <type_traits>
#include <algorithm>
#include <numeric>
#include <functional>
#include <string>
#include <string_view>
#include <array>
#include <vector>
#include <deque>
#include <queue>
#include <stack>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <variant>
#include <any>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <limits>
#include <stdexcept>
#include <utility>
#include <tuple>
#include <chrono>
#include <concepts>
#include <ranges>
#include <memory_resource>
#include <cassert>
`


function fullSource(): string {
  return DEFAULT_INCLUDES + '\n' + innerCode.value
}

async function run(): Promise<void> {
  if (opened.value) { opened.value = false; return }
  if (!innerCode.value) return
  running.value = true
  try {
    const res = await fetch(
      `https://godbolt.org/api/compiler/${COMPILER}/compile`,
      {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify({
          source: fullSource(),
          options: {
            userArguments: `${FLAGS} -std=${STD}`,
            filters: { execute: true },
            compilerOptions: { executorRequest: true },
          },
        }),
      },
    )
    if (!res.ok) throw new Error(`API ${res.status}`)
    const json: Record<string, unknown> = await res.json()

    const stdout = ((json.stdout as Array<{ text: string }> | undefined) ?? []).map((l) => l.text).join('\n')
    const stderr = ((json.stderr as Array<{ text: string }> | undefined) ?? []).map((l) => l.text).join('\n')
    exitOk.value = (json.code as number) === 0

    if (json.didExecute && stdout)      output.value = `<pre class="ce-stdout">${esc(stdout)}</pre>`
    else if (json.didExecute && stderr)  output.value = `<pre class="ce-stderr">${esc(stderr)}</pre>`
    else if (json.didExecute)            output.value = `<pre class="ce-ok">\u2713 运行成功（无输出）</pre>`
    else                                 output.value = `<pre class="ce-info">编译成功 \u2713 （无运行时输出 — static_assert / constexpr 在编译期求值）</pre>`

    const buildResult = json.buildResult as Record<string, unknown> | undefined
    const asm = ((buildResult?.asm as Array<{ text: string }> | undefined) ?? []).map((l) => l.text).join('\n')
    asmText.value = asm.length > 1500 ? asm.slice(0, 1500) + '\n\u2026' : asm
    opened.value = true
  } catch (e) {
    const msg = e instanceof Error ? e.message : String(e)
    output.value = `<pre class="ce-stderr">请求失败：${esc(msg)}\n请检查网络连接。</pre>`
    asmText.value = ''
    opened.value = true
  } finally {
    running.value = false
  }
}

function esc(s: string): string {
  return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
}
</script>

<template>
  <div class="ce-inner">
    <button class="ce-btn" :class="{ running }" :disabled="running" @click="run">
      <span class="ce-ico">{{ running ? '\u23f3' : '\u25b6' }}</span>
      {{ running ? '编译中\u2026' : '在 Compiler Explorer 中运行' }}
    </button>
    <Transition name="ce-slide">
      <div v-if="opened" class="ce-panel">
        <div class="ce-panel-head">
          <span>{{ COMPILER }} \u00b7 {{ STD }} <span v-if="exitOk" class="ce-ok-tag">\u2713</span></span>
          <a href="https://godbolt.org" target="_blank" rel="noopener">在新标签页中打开 \u2197</a>
        </div>
        <div class="ce-output" v-html="output"></div>
        <details v-if="asmText" class="ce-asm">
          <summary>汇编（前 1500 字符）</summary>
          <pre>{{ asmText }}</pre>
        </details>
      </div>
    </Transition>
  </div>
</template>

<style scoped>
.ce-inner { margin: -0.4rem 0 1.4rem; }

.ce-btn {
  display: inline-flex; align-items: center; gap: 0.5rem;
  padding: 0.5rem 1.2rem; border: none; border-radius: 8px;
  background: linear-gradient(135deg, #43a047, #66bb6a); color: #fff;
  font: 700 0.85rem system-ui, sans-serif; cursor: pointer;
  transition: all 0.2s; box-shadow: 0 2px 6px rgba(67, 160, 71, 0.3);
}
.ce-btn:hover:not(:disabled) {
  transform: translateY(-1px); background: linear-gradient(135deg, #4caf50, #81c784);
  box-shadow: 0 4px 12px rgba(67, 160, 71, 0.45);
}
.ce-btn.running { opacity: 0.7; cursor: wait; }
.ce-ico { font-size: 0.9em; }

.ce-panel {
  margin-top: 0.6rem; border: 1px solid var(--vp-c-divider);
  border-radius: 8px; overflow: hidden; background: var(--vp-code-bg);
}
.ce-panel-head {
  display: flex; justify-content: space-between; align-items: center;
  padding: 0.5rem 1rem; background: var(--vp-c-bg-soft);
  border-bottom: 1px solid var(--vp-c-divider); font-size: 0.82rem; color: var(--vp-c-text-2);
}
.ce-panel-head a { color: var(--vp-c-brand-1); font-size: 0.8rem; }
.ce-ok-tag { color: #66bb6a; font-weight: 700; }

.ce-output { padding: 1rem; overflow-x: auto; }
.ce-output :deep(pre) {
  margin: 0; font-family: var(--vp-font-family-mono);
  font-size: 0.88rem; line-height: 1.6; white-space: pre-wrap;
}
.ce-output :deep(.ce-stdout) { color: #a5d6a7; }
.ce-output :deep(.ce-stderr) { color: #ef9a9a; }
.ce-output :deep(.ce-ok)     { color: #66bb6a; }
.ce-output :deep(.ce-info)   { color: var(--vp-c-text-3); }

.ce-asm { border-top: 1px solid var(--vp-c-divider); font-size: 0.82rem; }
.ce-asm summary {
  padding: 0.5rem 1rem; cursor: pointer; color: var(--vp-c-text-3);
  background: var(--vp-c-bg-soft); user-select: none;
}
.ce-asm pre {
  padding: 1rem; margin: 0; max-height: 240px; overflow: auto;
  font-family: var(--vp-font-family-mono); font-size: 0.78rem;
  line-height: 1.5; color: var(--vp-c-text-3);
}

.ce-slide-enter-active, .ce-slide-leave-active { transition: all 0.25s ease; }
.ce-slide-enter-from, .ce-slide-leave-to { opacity: 0; max-height: 0; }
.ce-slide-enter-to, .ce-slide-leave-from { max-height: 600px; }
</style>
