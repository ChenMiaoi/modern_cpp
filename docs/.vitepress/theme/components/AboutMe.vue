<template>
  <div class="about-page">
    <div class="profile-card">
      <div class="avatar-wrapper">
        <img
          src="https://avatars.githubusercontent.com/u/96290967?v=4"
          alt="ChenMiaoi"
          class="avatar"
        />
      </div>
      <h1 class="name">Nya~ ChenMiaoi</h1>
      <p class="bio">💭 Studying!</p>
      <div class="location">
        <span>📍 The Hong Kong Polytechnic University · Kowloon, Hong Kong</span>
      </div>
      <div class="links">
        <a href="https://github.com/ChenMiaoi" target="_blank" class="link-btn github">
          <svg viewBox="0 0 16 16" width="18" height="18" fill="currentColor">
            <path d="M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.013 8.013 0 0016 8c0-4.42-3.58-8-8-8z"/>
          </svg>
          GitHub
        </a>
        <a href="https://chenmiaoi.github.io/" target="_blank" class="link-btn blog">
          🌐 Blog
        </a>
        <a href="mailto:chenmiao.ku@gmail.com" class="link-btn email">
          ✉️ Email
        </a>
      </div>
    </div>

    <div class="section">
      <h2>🛠 技术方向</h2>
      <div class="tech-stack">
        <div class="tech-card">
          <div class="tech-icon">🐧</div>
          <h3>领域</h3>
          <p>Operating Systems · Kernel · Drivers · Security</p>
        </div>
        <div class="tech-card">
          <div class="tech-icon">💻</div>
          <h3>技术栈</h3>
          <div class="tags">
            <span class="tag c">C</span>
            <span class="tag cpp">C++</span>
            <span class="tag rust">Rust</span>
            <span class="tag linux">Linux</span>
          </div>
        </div>
        <div class="tech-card">
          <div class="tech-icon">🎯</div>
          <h3>关注点</h3>
          <p>Performance · Reliability · Hardware-facing Software</p>
        </div>
      </div>
    </div>

    <div class="section">
      <h2>🧬 About Me</h2>
      <div class="code-block">
        <pre><code>struct Miao {
    domain:  "Operating Systems / Kernel / Drivers / Security",
    stack:   "C / C++ / Rust / Linux",
    focus:   "performance, reliability, hardware-facing software",
    mindset: "build deep, debug deeper",
};</code></pre>
      </div>
    </div>

    <div class="section">
      <h2>📦 主要项目</h2>
      <div class="project-grid">
        <a href="https://github.com/ChenMiaoi/CRIEW" target="_blank" class="project-card">
          <div class="project-header">
            <span class="project-icon">📁</span>
            <span class="project-lang rust">Rust</span>
          </div>
          <h3>CRIEW</h3>
          <p>Rust TUI for Linux kernel patch email workflow: sync lore threads, manage patch series, apply/download/undo, and browse source.</p>
          <div class="project-stars">⭐ 10</div>
        </a>
        <a href="https://github.com/ChenMiaoi/GSoC-2025-Final-Report" target="_blank" class="project-card">
          <div class="project-header">
            <span class="project-icon">📁</span>
            <span class="project-lang c">C</span>
          </div>
          <h3>GSoC-2025-Final-Report</h3>
          <p>Google Summer of Code 2025 Final Report</p>
        </a>
        <a href="https://github.com/ChenMiaoi/GSoC-2026-Final-Report" target="_blank" class="project-card">
          <div class="project-header">
            <span class="project-icon">📁</span>
            <span class="project-lang c">C</span>
          </div>
          <h3>GSoC-2026-Final-Report</h3>
          <p>Google Summer of Code 2026 Final Report</p>
        </a>
      </div>
    </div>
  </div>
</template>

<style scoped>
.about-page {
  max-width: 800px;
  margin: 0 auto;
  padding: 2rem 1.5rem;
}

.profile-card {
  text-align: center;
  padding: 3rem 2rem;
  background: linear-gradient(135deg, var(--vp-c-bg-soft) 0%, var(--vp-c-bg-alt) 100%);
  border-radius: 16px;
  border: 1px solid var(--vp-c-divider);
  margin-bottom: 2.5rem;
}

.avatar-wrapper {
  display: inline-block;
  position: relative;
  margin-bottom: 1.2rem;
}

.avatar {
  width: 120px;
  height: 120px;
  border-radius: 50%;
  border: 4px solid var(--vp-c-brand-1);
  box-shadow: 0 4px 20px rgba(0, 0, 0, 0.15);
  transition: transform 0.3s ease;
}

.avatar:hover {
  transform: scale(1.05);
}

.name {
  font-size: 2rem;
  font-weight: 700;
  margin: 0.5rem 0 0.3rem;
  background: linear-gradient(135deg, var(--vp-c-brand-1), var(--vp-c-brand-2));
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
}

.bio {
  font-size: 1.1rem;
  color: var(--vp-c-text-2);
  margin: 0.3rem 0 1rem;
}

.location {
  color: var(--vp-c-text-3);
  font-size: 0.95rem;
  margin-bottom: 1.5rem;
}

.links {
  display: flex;
  gap: 0.75rem;
  justify-content: center;
  flex-wrap: wrap;
}

.link-btn {
  display: inline-flex;
  align-items: center;
  gap: 0.4rem;
  padding: 0.5rem 1rem;
  border-radius: 8px;
  font-size: 0.9rem;
  font-weight: 500;
  text-decoration: none;
  color: var(--vp-c-text-1);
  background: var(--vp-c-bg);
  border: 1px solid var(--vp-c-divider);
  transition: all 0.2s ease;
}

.link-btn:hover {
  border-color: var(--vp-c-brand-1);
  color: var(--vp-c-brand-1);
  transform: translateY(-1px);
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.08);
}

.section {
  margin-bottom: 2.5rem;
}

.section h2 {
  font-size: 1.4rem;
  font-weight: 600;
  margin-bottom: 1rem;
  padding-bottom: 0.5rem;
  border-bottom: 2px solid var(--vp-c-divider);
}

.tech-stack {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 1rem;
}

@media (max-width: 640px) {
  .tech-stack {
    grid-template-columns: 1fr;
  }
}

.tech-card {
  padding: 1.2rem;
  background: var(--vp-c-bg-soft);
  border-radius: 12px;
  border: 1px solid var(--vp-c-divider);
  text-align: center;
  transition: all 0.2s ease;
}

.tech-card:hover {
  border-color: var(--vp-c-brand-1);
  transform: translateY(-2px);
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.08);
}

.tech-icon {
  font-size: 2rem;
  margin-bottom: 0.5rem;
}

.tech-card h3 {
  font-size: 1rem;
  font-weight: 600;
  margin: 0 0 0.5rem;
}

.tech-card p {
  font-size: 0.85rem;
  color: var(--vp-c-text-2);
  margin: 0;
}

.tags {
  display: flex;
  gap: 0.4rem;
  justify-content: center;
  flex-wrap: wrap;
}

.tag {
  padding: 0.25rem 0.6rem;
  border-radius: 6px;
  font-size: 0.8rem;
  font-weight: 600;
  font-family: monospace;
}

.tag.c { background: #555555; color: #fff; }
.tag.cpp { background: #00599C; color: #fff; }
.tag.rust { background: #CE422B; color: #fff; }
.tag.linux { background: #063846; color: #fff; }

.code-block {
  background: var(--vp-c-bg-soft);
  border-radius: 12px;
  border: 1px solid var(--vp-c-divider);
  overflow: hidden;
}

.code-block pre {
  margin: 0;
  padding: 1.5rem;
  overflow-x: auto;
}

.code-block code {
  font-family: 'Fira Code', 'JetBrains Mono', monospace;
  font-size: 0.9rem;
  line-height: 1.6;
  color: var(--vp-c-text-1);
}

.project-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(240px, 1fr));
  gap: 1rem;
}

.project-card {
  display: block;
  padding: 1.2rem;
  background: var(--vp-c-bg-soft);
  border-radius: 12px;
  border: 1px solid var(--vp-c-divider);
  text-decoration: none;
  color: inherit;
  transition: all 0.2s ease;
}

.project-card:hover {
  border-color: var(--vp-c-brand-1);
  transform: translateY(-2px);
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.08);
}

.project-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 0.5rem;
}

.project-icon {
  font-size: 1.2rem;
}

.project-lang {
  padding: 0.15rem 0.5rem;
  border-radius: 4px;
  font-size: 0.75rem;
  font-weight: 600;
}

.project-lang.rust { background: #CE422B; color: #fff; }
.project-lang.c { background: #555555; color: #fff; }

.project-card h3 {
  font-size: 1rem;
  font-weight: 600;
  margin: 0 0 0.4rem;
  color: var(--vp-c-brand-1);
}

.project-card p {
  font-size: 0.85rem;
  color: var(--vp-c-text-2);
  margin: 0 0 0.5rem;
  line-height: 1.5;
}

.project-stars {
  font-size: 0.8rem;
  color: var(--vp-c-text-3);
}
</style>
