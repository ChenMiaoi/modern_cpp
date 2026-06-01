import DefaultTheme from 'vitepress/theme'
import { createApp, h } from 'vue'
import CompilerExplorer from './components/CompilerExplorer.vue'
import type { Theme } from 'vitepress'

export default {
  extends: DefaultTheme,
  enhanceApp({ app }) {
    app.component('CompilerExplorer', CompilerExplorer)
  },
} satisfies Theme

// Auto-mount <CompilerExplorer> on .ce-wrapper divs from markdown-it plugin
if (typeof window !== 'undefined') {
  function mountAll(root: ParentNode) {
    const wrappers = root.querySelectorAll<HTMLElement>('.ce-wrapper:not([data-ce-mounted])')
    wrappers.forEach((el) => {
      el.setAttribute('data-ce-mounted', '')
      const b64 = el.dataset.ceCode ?? ''
      let code = ''
      try { code = atob(b64) } catch { /* ignore */ }

      const mountPoint = document.createElement('div')
      el.appendChild(mountPoint)
      createApp({
        render: () => h(CompilerExplorer, { code }),
      }).mount(mountPoint)
    })
  }

  mountAll(document)

  const observer = new MutationObserver((mutations) => {
    for (const m of mutations) {
      for (const node of m.addedNodes) {
        if (node instanceof HTMLElement) mountAll(node)
      }
    }
  })
  observer.observe(document.body, { childList: true, subtree: true })
}
