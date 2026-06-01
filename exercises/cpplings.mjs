#!/usr/bin/env node
/**
 * Cpplings — Modern C++ 交互式练习 CLI
 * 仿照 Rustlings 的学习体验：编辑代码 → 自动编译 → 通过后自动下一个
 *
 * 用法:
 *   cpplings               显示欢迎信息和当前练习
 *   cpplings list          列出所有练习及状态
 *   cpplings run <id>      编译并运行指定练习
 *   cpplings hint <id>     显示提示
 *   cpplings watch         监听模式（自动重编译）
 *   cpplings progress      显示学习进度
 *   cpplings verify        验证所有已完成的练习
 *   cpplings next          运行下一个未完成的练习
 *   cpplings reset <id>    重置练习状态
 */

import { readFileSync, writeFileSync, existsSync, mkdirSync, watch, statSync } from 'fs';
import { join, dirname, resolve, basename } from 'path';
import { execSync } from 'child_process';
import { fileURLToPath } from 'url';
import { homedir } from 'os';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);
const EXERCISES_DIR = __dirname;
const MANIFEST = join(EXERCISES_DIR, 'exercises.json');
const INCLUDE_DIR = join(EXERCISES_DIR, 'include');
const STATE_FILE = join(homedir(), '.cpplings', 'state.json');

// ── ANSI ──────────────────────────────────────────────────────────────
const ESC = '\x1b';
const C = {
    reset:   `${ESC}[0m`,
    bold:    `${ESC}[1m`,
    dim:     `${ESC}[2m`,
    red:     `${ESC}[31m`,
    green:   `${ESC}[32m`,
    yellow:  `${ESC}[33m`,
    blue:    `${ESC}[34m`,
    magenta: `${ESC}[35m`,
    cyan:    `${ESC}[36m`,
    white:   `${ESC}[37m`,
    bgGreen: `${ESC}[42m`,
    bgRed:   `${ESC}[41m`,
    bgYellow:`${ESC}[43m`,
    bgBlue:  `${ESC}[44m`,
};

function clr(text, ...codes) { return codes.join('') + text + C.reset; }
function line(ch = '─', w = 60) { return clr(ch.repeat(w), C.dim); }

// ── State ─────────────────────────────────────────────────────────────
function loadState() {
    try { return JSON.parse(readFileSync(STATE_FILE, 'utf-8')); }
    catch { return { completed: {} }; }
}

function saveState(state) {
    const dir = dirname(STATE_FILE);
    if (!existsSync(dir)) mkdirSync(dir, { recursive: true });
    writeFileSync(STATE_FILE, JSON.stringify(state, null, 2));
}

function markDone(id) {
    const state = loadState();
    state.completed[id] = Date.now();
    saveState(state);
}

function isDone(id) {
    return !!loadState().completed[id];
}

// ── Manifest ──────────────────────────────────────────────────────────
function loadManifest() {
    return JSON.parse(readFileSync(MANIFEST, 'utf-8'));
}

function getExercises(m) {
    return m.exercises.map(ex => ({
        ...ex,
        topicInfo: m.topics.find(t => t.id === ex.topic),
    }));
}

function findEx(id) {
    const m = loadManifest();
    return getExercises(m).find(e => e.id === id || e.file === id);
}

// ── Compile & Run ─────────────────────────────────────────────────────
function compile(filePath, exercise) {
    const m = loadManifest();
    const compiler = m.compiler || 'g++';
    const std = exercise.topicInfo?.std || m.std || 'c++17';
    const buildDir = join(EXERCISES_DIR, '.build');
    if (!existsSync(buildDir)) mkdirSync(buildDir, { recursive: true });
    const outExt = process.platform === 'win32' ? '.exe' : '';
    const outFile = join(buildDir, exercise.id + outExt);

    const args = [
        `"${filePath}"`,
        `-std=${std}`,
        `-I"${INCLUDE_DIR}"`,
        `-o "${outFile}"`,
        '-Wall', '-Wextra', '-Wpedantic',
        '-O0',
    ];

    try {
        execSync(`${compiler} ${args.join(' ')}`, {
            cwd: EXERCISES_DIR,
            stdio: 'pipe',
            timeout: 30000,
        });
        return { ok: true, exe: outFile, errors: '' };
    } catch (err) {
        return { ok: false, exe: '', errors: err.stderr?.toString() || '' };
    }
}

function runExe(exePath) {
    try {
        const out = execSync(`"${exePath}"`, {
            cwd: EXERCISES_DIR,
            stdio: 'pipe',
            timeout: 10000,
            env: { ...process.env, FORCE_COLOR: '1' },
        });
        return { ok: true, stdout: out.toString(), stderr: '', code: 0 };
    } catch (err) {
        return {
            ok: false,
            stdout: err.stdout?.toString() || '',
            stderr: err.stderr?.toString() || '',
            code: err.status ?? 1,
        };
    }
}

// ── Formatting ────────────────────────────────────────────────────────
function fmtErrors(stderr) {
    const lines = stderr.split('\n');
    const out = [];
    for (const l of lines) {
        if (!l.trim()) continue;
        if (l.includes('error:'))       out.push(clr(`    ${l}`, C.red));
        else if (l.includes('warning:')) out.push(clr(`    ${l}`, C.yellow));
        else if (l.includes('note:'))    out.push(clr(`    ${l}`, C.dim));
        else                             out.push(clr(`    ${l}`, C.dim));
    }
    return out.join('\n');
}

function progressBar(done, total, w = 20) {
    const pct = total > 0 ? done / total : 0;
    const filled = Math.round(pct * w);
    const bar = '█'.repeat(filled) + '░'.repeat(w - filled);
    const color = pct >= 1 ? C.green : pct > 0 ? C.yellow : C.dim;
    return `[${clr(bar, color)}] ${done}/${total}`;
}

// ── Commands ──────────────────────────────────────────────────────────

function cmdWelcome() {
    const m = loadManifest();
    const exercises = getExercises(m);
    const state = loadState();
    const done = exercises.filter(e => state.completed[e.id]).length;
    const next = exercises.find(e => !state.completed[e.id]);

    console.log();
    console.log(clr('  ╔══════════════════════════════════════════════════╗', C.cyan));
    console.log(clr('  ║                                                  ║', C.cyan));
    console.log(clr('  ║      ⚡  Cpplings — Modern C++ 练习  ⚡          ║', C.cyan));
    console.log(clr('  ║                                                  ║', C.cyan));
    console.log(clr('  ╚══════════════════════════════════════════════════╝', C.cyan));
    console.log();
    console.log(clr('  通过动手编程掌握 C++ — 像 Rustlings 一样学 C++', C.white));
    console.log();
    console.log(`  ${progressBar(done, exercises.length)}  ${clr(`${Math.round(done / exercises.length * 100)}%`, C.bold)}`);
    console.log();

    if (next) {
        console.log(clr('  → 下一个练习:', C.bold));
        console.log(`    ${clr(next.id, C.cyan)} ${next.title}`);
        console.log();
        console.log(clr('  编辑代码，然后运行:', C.dim));
        console.log(clr(`    cpplings run ${next.id}`, C.green));
        console.log(clr(`    cpplings watch              ${'(监听模式，自动重编译)'}`, C.dim));
    } else {
        console.log(clr('  🎉 恭喜！你已完成所有练习！', C.bold, C.green));
    }

    console.log();
    console.log(clr('  命令:', C.bold));
    console.log(clr('    list        ', C.green) + '列出所有练习');
    console.log(clr('    run <id>    ', C.green) + '编译并运行练习');
    console.log(clr('    hint <id>   ', C.green) + '显示提示');
    console.log(clr('    watch       ', C.green) + '监听模式（修改文件自动重编译）');
    console.log(clr('    progress    ', C.green) + '显示详细进度');
    console.log(clr('    next        ', C.green) + '运行下一个练习');
    console.log(clr('    verify      ', C.green) + '验证所有已完成的练习');
    console.log(clr('    reset <id>  ', C.green) + '重置练习');
    console.log();
}

function cmdList() {
    const m = loadManifest();
    const exercises = getExercises(m);
    const state = loadState();
    let currentTopic = '';
    let done = 0;

    console.log();
    for (const ex of exercises) {
        if (ex.topic !== currentTopic) {
            currentTopic = ex.topic;
            const t = ex.topicInfo;
            console.log(clr(`  ${t?.title || currentTopic}`, C.bold, C.cyan));
            console.log(clr(`  ${t?.description || ''}`, C.dim));
        }
        const completed = !!state.completed[ex.id];
        if (completed) done++;
        const icon = completed ? clr(' ✓ ', C.bgGreen, C.white) : clr(' · ', C.dim);
        const titleClr = completed ? C.green : C.white;
        console.log(`  ${icon} ${clr(ex.id.padEnd(16), titleClr)} ${ex.title}`);
    }

    console.log();
    console.log(`  ${progressBar(done, exercises.length)}`);
    console.log();
}

function cmdRun(id) {
    if (!id) {
        console.error(clr('\n  用法: cpplings run <id>\n', C.red));
        process.exit(1);
    }
    const ex = findEx(id);
    if (!ex) {
        console.error(clr(`\n  ✗ 未找到练习: ${id}\n`, C.red));
        process.exit(1);
    }

    const filePath = join(EXERCISES_DIR, ex.file);
    if (!existsSync(filePath)) {
        console.error(clr(`\n  ✗ 练习文件不存在: ${ex.file}\n`, C.red));
        process.exit(1);
    }

    console.log();
    console.log(clr(`  ─── ${ex.id}: ${ex.title} ───`, C.bold));
    console.log();

    // Compile
    const comp = compile(filePath, ex);
    if (!comp.ok) {
        console.log(clr('  ✗ 编译失败', C.red, C.bold));
        console.log();
        console.log(fmtErrors(comp.errors));
        console.log();
        console.log(clr('  提示: 修复上面的编译错误。找到代码中的 TODO 注释。', C.dim));
        console.log(clr(`  需要帮助? cpplings hint ${ex.id}`, C.dim));
        console.log();
        return false;
    }

    // Run
    const result = runExe(comp.exe);
    if (result.stdout) process.stdout.write(result.stdout);
    if (result.stderr) process.stderr.write(result.stderr);

    // Check result
    const passed = result.code === 0 && !result.stderr.includes('FAIL');

    if (passed) {
        markDone(ex.id);
        console.log(clr('  ✓ 通过！', C.bold, C.green));
        console.log();
        // Show next
        const m = loadManifest();
        const exercises = getExercises(m);
        const idx = exercises.findIndex(e => e.id === ex.id);
        const next = exercises.slice(idx + 1).find(e => !isDone(e.id));
        if (next) {
            console.log(clr(`  → 下一个: ${next.id} (${next.title})`, C.dim));
            console.log(clr(`    cpplings run ${next.id}`, C.dim));
        } else {
            const done = exercises.filter(e => isDone(e.id)).length;
            if (done === exercises.length) {
                console.log(clr('  🎉 恭喜！你已完成所有练习！', C.bold, C.green));
            }
        }
    } else {
        console.log(clr('  ✗ 未通过', C.red, C.bold));
        console.log();
        console.log(clr('  编辑代码修复问题，然后重新运行。', C.dim));
        console.log(clr(`  需要帮助? cpplings hint ${ex.id}`, C.dim));
    }
    console.log();
    return passed;
}

function cmdHint(id) {
    if (!id) {
        console.error(clr('\n  用法: cpplings hint <id>\n', C.red));
        process.exit(1);
    }
    const ex = findEx(id);
    if (!ex) {
        console.error(clr(`\n  ✗ 未找到练习: ${id}\n`, C.red));
        process.exit(1);
    }
    console.log();
    console.log(clr(`  💡 ${ex.title}`, C.bold, C.yellow));
    console.log();
    console.log(`  ${ex.hint}`);
    console.log();
    console.log(clr(`  文件: ${ex.file}`, C.dim));
    console.log();
}

function cmdProgress() {
    const m = loadManifest();
    const exercises = getExercises(m);
    const state = loadState();

    console.log();
    for (const topic of m.topics) {
        const topicEx = exercises.filter(e => e.topic === topic.id);
        const topicDone = topicEx.filter(e => state.completed[e.id]).length;
        const total = topicEx.length;
        const pct = total > 0 ? Math.round(topicDone / total * 100) : 0;

        console.log(clr(`  ${topic.title}`, C.bold));
        console.log(`  ${progressBar(topicDone, total)}  ${pct}%`);

        for (const ex of topicEx) {
            const done = !!state.completed[ex.id];
            const icon = done ? clr('✓', C.green) : clr('○', C.dim);
            const titleClr = done ? C.green : C.dim;
            console.log(`    ${icon} ${clr(ex.id.padEnd(16), titleClr)} ${ex.title}`);
        }
        console.log();
    }

    const totalDone = exercises.filter(e => state.completed[e.id]).length;
    const totalPct = Math.round(totalDone / exercises.length * 100);
    console.log(clr(`  总进度: ${progressBar(totalDone, exercises.length)}  ${totalPct}%`, C.bold));
    console.log();
}

function cmdNext() {
    const m = loadManifest();
    const exercises = getExercises(m);
    const next = exercises.find(e => !isDone(e.id));
    if (!next) {
        console.log();
        console.log(clr('  🎉 恭喜！你已完成所有练习！', C.bold, C.green));
        console.log();
        return;
    }
    console.log();
    console.log(clr(`  → ${next.id}: ${next.title}`, C.bold));
    console.log(clr(`    ${next.description}`, C.dim));
    console.log();
    cmdRun(next.id);
}

function cmdVerify() {
    const m = loadManifest();
    const exercises = getExercises(m);
    const state = loadState();
    const completed = exercises.filter(e => state.completed[e.id]);

    if (completed.length === 0) {
        console.log(clr('\n  还没有完成任何练习\n', C.dim));
        return;
    }

    console.log();
    console.log(clr(`  验证 ${completed.length} 个已完成的练习...`, C.bold));
    console.log();

    let pass = 0, fail = 0;
    for (const ex of completed) {
        const filePath = join(EXERCISES_DIR, ex.file);
        const comp = compile(filePath, ex);
        if (!comp.ok) {
            fail++;
            console.log(clr(`  ✗ ${ex.id} — 编译失败`, C.red));
            delete state.completed[ex.id];
            continue;
        }
        const result = runExe(comp.exe);
        if (result.code === 0 && !result.stderr.includes('FAIL')) {
            pass++;
            console.log(clr(`  ✓ ${ex.id}`, C.green));
        } else {
            fail++;
            console.log(clr(`  ✗ ${ex.id} — 测试失败`, C.red));
            delete state.completed[ex.id];
        }
    }

    saveState(state);
    console.log();
    const color = fail > 0 ? C.red : C.green;
    console.log(clr(`  结果: ${pass} 通过, ${fail} 失败`, color));
    console.log();
}

function cmdWatch() {
    const m = loadManifest();
    const exercises = getExercises(m);

    console.log();
    console.log(clr('  👀 监听模式', C.bold, C.cyan));
    console.log(clr('  修改练习文件后自动重新编译运行。按 Ctrl+C 退出。', C.dim));
    console.log();

    // Run current exercise
    const next = exercises.find(e => !isDone(e.id));
    if (next) {
        console.log(clr(`  当前练习: ${next.id} (${next.title})`, C.cyan));
        console.log(clr(`  文件: ${next.file}`, C.dim));
        console.log();
        cmdRun(next.id);
    } else {
        console.log(clr('  所有练习已完成！', C.green));
    }

    // Collect all exercise directories
    const dirs = [...new Set(exercises.map(e => join(EXERCISES_DIR, dirname(e.file))))];

    let debounce = null;
    function onChange(filename) {
        if (!filename || !filename.endsWith('.cpp')) return;
        if (debounce) clearTimeout(debounce);
        debounce = setTimeout(() => {
            // Find which exercise this file belongs to
            const normalized = filename.replace(/\\/g, '/');
            const ex = exercises.find(e => normalized.endsWith(basename(e.file)));
            if (!ex) return;

            console.clear();
            console.log(clr(`  📝 检测到变更: ${filename}`, C.cyan));
            console.log();
            cmdRun(ex.id);

            // Update "next" if this one passed
            const nextEx = exercises.find(e => !isDone(e.id));
            if (nextEx) {
                console.log(clr(`  当前练习: ${nextEx.id} (${nextEx.title})`, C.cyan));
                console.log(clr(`  文件: ${nextEx.file}`, C.dim));
                console.log();
            }
        }, 300);
    }

    for (const dir of dirs) {
        if (!existsSync(dir)) continue;
        try {
            watch(dir, { recursive: true }, (event, filename) => onChange(filename));
        } catch {}
    }

    // Keep alive
    process.stdin.resume();
    process.on('SIGINT', () => {
        console.log(clr('\n\n  再见！\n', C.dim));
        process.exit(0);
    });
}

function cmdReset(id) {
    if (!id) {
        console.error(clr('\n  用法: cpplings reset <id>\n', C.red));
        process.exit(1);
    }
    const state = loadState();
    delete state.completed[id];
    saveState(state);
    console.log(clr(`\n  ✓ 已重置: ${id}\n`, C.green));
}

// ── Main ──────────────────────────────────────────────────────────────
const args = process.argv.slice(2);
const cmd = args[0];
const arg = args[1];

switch (cmd) {
    case 'list':   case 'ls': cmdList(); break;
    case 'run':    case 'r':  cmdRun(arg); break;
    case 'hint':   case 'h':  cmdHint(arg); break;
    case 'watch':  case 'w':  cmdWatch(); break;
    case 'progress': case 'p': cmdProgress(); break;
    case 'verify': case 'v':  cmdVerify(); break;
    case 'next':   case 'n':  cmdNext(); break;
    case 'reset':             cmdReset(arg); break;
    default:                  cmdWelcome(); break;
}
