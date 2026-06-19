<script setup lang="ts">
import { ref, computed, watch } from 'vue'

function isZh(): boolean {
  return typeof document === 'undefined' || document.documentElement.lang.startsWith('zh')
}

const i18n = {
  btnRun:     { zh: '在 Compiler Explorer 中运行', en: 'Run in Compiler Explorer' },
  btnRunning: { zh: '编译中…', en: 'Compiling…' },
  tabAsm:     { zh: '汇编',     en: 'Assembly' },
  tabOutput:  { zh: '输出',     en: 'Output' },
  noAsm:      { zh: '无汇编输出', en: 'No assembly output' },
  runOk:      { zh: '✓ 运行成功（无输出）', en: '✓ Ran successfully (no output)' },
  compileOk:  { zh: '编译成功 ✓ （无运行时输出 — static_assert / constexpr 在编译期求值）', en: 'Compiled ✓ (no runtime output — static_assert / constexpr evaluated at compile time)' },
  reqFail:    { zh: '请求失败：', en: 'Request failed: ' },
  checkNet:   { zh: '请检查网络连接。', en: 'Check your network connection.' },
} as const

function t(key: keyof typeof i18n): string {
  return isZh() ? i18n[key].zh : i18n[key].en
}

const props = defineProps<{ code?: string }>()
const innerCode = ref(props.code ?? '')
const running = ref(false)
const opened = ref(false)
const activeTab = ref<'asm' | 'output'>('asm')
const output = ref('')
const rawAsm = ref('')
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

// Filter Godbolt assembly: keep only labels that look like user-defined code
// Drops: mangled C++ (_Z...), glibc (__...), switch tables (CSWTCH), local (.L...),
//        std:: names, operator new/delete, compiler internals
function isUserLabel(label: string): boolean {
  if (label.startsWith('.')) return false
  if (label.startsWith('_Z')) return false
  if (label.startsWith('__') || label.includes('__gnu_cxx::')) return false
  if (label.startsWith('CSWTCH')) return false
  if (/^operator\s+(new|delete)/.test(label)) return false
  if (label.includes('std::')) return false
  if (label.includes('typeinfo') || label.includes('vtable')) return false
  return true
}

function filterAsm(raw: string): string {
  const lines = raw.split('\n')
  type Block = { label: string; lines: string[] }
  const blocks: Block[] = []
  let cur: Block | null = null

  for (const line of lines) {
    const t = line.trimEnd()
    if (t.length > 0 && !t.startsWith(' ') && !t.startsWith('\t') && t.endsWith(':')) {
      if (cur) blocks.push(cur)
      cur = { label: t.replace(/:$/, ''), lines: [] }
    } else if (cur) {
      cur.lines.push(t)
    }
  }
  if (cur) blocks.push(cur)

  const kept = blocks.filter(b => isUserLabel(b.label))
  return kept.map(b => b.label + ':\n' + b.lines.join('\n')).join('\n\n')
}

const displayAsm = computed(() => filterAsm(rawAsm.value || ''))


const GODBOLT_API = `https://godbolt.org/api/compiler/${COMPILER}/compile`
const HEADERS = { 'Content-Type': 'application/json', Accept: 'application/json' }
// Compile-only request → gets assembly (no linking, no main() needed)
async function fetchAsm(src: string): Promise<string> {
  const res = await fetch(GODBOLT_API, {
    method: 'POST', headers: HEADERS,
    body: JSON.stringify({
      source: src,
      options: {
        userArguments: `${FLAGS} -std=${STD} -masm=intel -fkeep-inline-functions`,
        filters: { intel: true },
      },
    }),
  })
  if (!res.ok) return ''
  const json = await res.json() as Record<string, unknown>
  const asm = ((json.asm as Array<{ text: string }> | undefined) ?? [])
  return asm.map(l => l.text).join('\n')
}

async function fetchRun(src: string): Promise<{ html: string; ok: boolean }> {
  const res = await fetch(GODBOLT_API, {
    method: 'POST', headers: HEADERS,
    body: JSON.stringify({
      source: src,
      options: {
        userArguments: `${FLAGS} -std=${STD}`,
        filters: { execute: true },
        compilerOptions: { executorRequest: true },
      },
    }),
  })
  if (!res.ok) throw new Error(`API ${res.status}`)
  const json = await res.json() as Record<string, unknown>
  const ok = (json.code as number) === 0
  const stdout = ((json.stdout as Array<{ text: string }> | undefined) ?? []).map(l => l.text).join('\n')
  const stderr = ((json.stderr as Array<{ text: string }> | undefined) ?? []).map(l => l.text).join('\n')
  let html: string
  if (json.didExecute && stdout)      html = `<pre class="ce-stdout">${esc(stdout)}</pre>`
  else if (json.didExecute && stderr)  html = `<pre class="ce-stderr">${esc(stderr)}</pre>`
  else if (json.didExecute)            html = `<pre class="ce-ok">${t('runOk')}</pre>`
  else                                 html = `<pre class="ce-info">${t('compileOk')}</pre>`
  return { html, ok }
}

async function run(): Promise<void> {
  if (opened.value) { opened.value = false; return }
  if (!innerCode.value) return
  running.value = true
  try {
    const src = fullSource()
    const [asmRaw, execResult] = await Promise.all([
      fetchAsm(src),
      fetchRun(src).catch((e): { html: string; ok: boolean } => ({
        html: `<pre class="ce-stderr">${t('reqFail')}${esc(e instanceof Error ? e.message : String(e))}</pre>`,
        ok: false,
      })),
    ])
    rawAsm.value = asmRaw
    output.value = execResult.html
    exitOk.value = execResult.ok
    activeTab.value = 'asm'
    opened.value = true
  } catch (e) {
    const msg = e instanceof Error ? e.message : String(e)
    output.value = `<pre class="ce-stderr">${t('reqFail')}${esc(msg)}\n${t('checkNet')}</pre>`
    rawAsm.value = ''
    activeTab.value = 'output'
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
      {{ running ? t('btnRunning') : t('btnRun') }}
    </button>
    <Transition name="ce-slide">
      <div v-if="opened" class="ce-panel">
        <div class="ce-panel-head">
          <div class="ce-tabs">
            <button
              class="ce-tab" :class="{ active: activeTab === 'asm' }"
              @click="activeTab = 'asm'"
            >{{ t('tabAsm') }} ({{ FLAGS }})</button>
            <button
              class="ce-tab" :class="{ active: activeTab === 'output' }"
              @click="activeTab = 'output'"
            >{{ t('tabOutput') }}</button>
          </div>
          <span>
            {{ COMPILER }} \u00b7 {{ STD }}
            <span v-if="exitOk" class="ce-ok-tag">\u2713</span>
          </span>
        </div>
        <div v-if="activeTab === 'asm'" class="ce-asm-panel">
          <pre v-if="displayAsm" class="ce-asm-code">{{ displayAsm }}</pre>
          <pre v-else class="ce-info">{{ t('noAsm') }}</pre>
        </div>
        <div v-else class="ce-output" v-html="output"></div>
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
  padding: 0 1rem; background: var(--vp-c-bg-soft);
  border-bottom: 1px solid var(--vp-c-divider); font-size: 0.82rem; color: var(--vp-c-text-2);
}
.ce-tabs { display: flex; gap: 0; }
.ce-tab {
  padding: 0.5rem 1rem; border: none; background: none;
  font: 600 0.82rem system-ui, sans-serif; cursor: pointer;
  color: var(--vp-c-text-3); border-bottom: 2px solid transparent;
  transition: all 0.15s;
}
.ce-tab:hover { color: var(--vp-c-text-2); }
.ce-tab.active {
  color: var(--vp-c-brand-1); border-bottom-color: var(--vp-c-brand-1);
}
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

.ce-asm-panel {
  padding: 1rem; overflow-x: auto; max-height: 480px; overflow-y: auto;
}
.ce-asm-code {
  margin: 0; font-family: var(--vp-font-family-mono);
  font-size: 0.82rem; line-height: 1.65; color: var(--vp-c-text-2);
  white-space: pre; tab-size: 4;
}
.ce-asm-panel .ce-info {
  margin: 0; color: var(--vp-c-text-3);
  font-family: var(--vp-font-family-mono); font-size: 0.85rem;
}

.ce-slide-enter-active, .ce-slide-leave-active { transition: all 0.25s ease; }
.ce-slide-enter-from, .ce-slide-leave-to { opacity: 0; max-height: 0; }
.ce-slide-enter-to, .ce-slide-leave-from { max-height: 800px; }
</style>