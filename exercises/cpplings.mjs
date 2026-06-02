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
 *   cpplings verify --solutions  验证所有 solution 文件
 *   cpplings verify --all        验证全部练习
 *   cpplings verify --ci         机器可读输出
 *   cpplings asm <id>            查看优化后的汇编输出
 *   cpplings asm <id> --opt=O3   指定优化级别
 *   cpplings next          运行下一个未完成的练习
 *   cpplings reset <id>    重置练习状态
 */

import { readFileSync, writeFileSync, existsSync, mkdirSync, watch, statSync } from 'fs';
import { join, dirname, resolve, basename } from 'path';
import { execFileSync } from 'child_process';
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
    catch { return { completed: {}, meta: {} }; }
}

function saveState(state) {
    const dir = dirname(STATE_FILE);
    if (!existsSync(dir)) mkdirSync(dir, { recursive: true });
    state.meta = state.meta || {};
    state.meta.repo_path = EXERCISES_DIR;
    state.meta.last_updated = new Date().toISOString();
    try {
        state.meta.git_commit = execFileSync('git', ['rev-parse', '--short', 'HEAD'], { cwd: EXERCISES_DIR, stdio: 'pipe' }).toString().trim();
    } catch {}
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

const PROFILES_FILE = join(EXERCISES_DIR, 'compiler-profiles.json');

function loadProfiles() {
    if (!existsSync(PROFILES_FILE)) return {};
    return JSON.parse(readFileSync(PROFILES_FILE, 'utf-8')).profiles || {};
}

function resolveProfile(flags) {
    const profiles = loadProfiles();
    const name = flags.profile;
    if (!name) return {};
    const p = profiles[name];
    if (!p) {
        console.error(clr(`  未知 profile: ${name}`, C.red));
        console.error(clr(`  可用: ${Object.keys(profiles).join(', ')}`, C.dim));
        process.exit(1);
    }
    return p;
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
function compile(filePath, exercise, opts = {}) {
    const m = loadManifest();
    const compiler = opts.compiler || m.compiler || 'g++';
    const std = opts.std || exercise.std || exercise.topicInfo?.std || m.std || 'c++17';
    const extraFlags = opts.flags || [];
    const buildDir = join(EXERCISES_DIR, '.build');
    if (!existsSync(buildDir)) mkdirSync(buildDir, { recursive: true });
    const outExt = process.platform === 'win32' ? '.exe' : '';
    const outFile = join(buildDir, exercise.id + outExt);

    const args = [
        filePath,
        `-std=${std}`,
        `-I${INCLUDE_DIR}`,
        '-o', outFile,
        '-Wall', '-Wextra', '-Wpedantic',
        '-O0',
        ...extraFlags,
    ];

    try {
        execFileSync(compiler, args, {
            cwd: EXERCISES_DIR,
            stdio: 'pipe',
            timeout: 30000,
        });
        return { ok: true, exe: outFile, errors: '' };
    } catch (err) {
        return { ok: false, exe: '', errors: err.stderr?.toString() || '' };
    }
}

function compileAsm(filePath, exercise, opts = {}) {
    const m = loadManifest();
    const compiler = opts.compiler || m.compiler || 'g++';
    const std = opts.std || exercise.std || exercise.topicInfo?.std || m.std || 'c++17';
    const optLevel = opts.optLevel || '-O2';
    const buildDir = join(EXERCISES_DIR, '.build');
    if (!existsSync(buildDir)) mkdirSync(buildDir, { recursive: true });
    const asmFile = join(buildDir, exercise.id + '.s');

    const isClang = compiler.includes('clang');
    const isMsvc = compiler.includes('cl');

    const args = [
        filePath,
        `-std=${std}`,
        `-I${INCLUDE_DIR}`,
        optLevel,
        '-S',
        '-masm=intel',
        '-o', asmFile,
    ];

    // GCC: add -fno-asynchronous-unwind-tables to strip .cfi noise
    if (!isClang && !isMsvc) {
        args.push('-fno-asynchronous-unwind-tables');
    }

    try {
        execFileSync(compiler, args, {
            cwd: EXERCISES_DIR,
            stdio: 'pipe',
            timeout: 30000,
        });
        return { ok: true, asmFile, errors: '' };
    } catch (err) {
        const stderr = err.stderr?.toString() || '';
        return { ok: false, asmFile: '', errors: stderr || err.message || '编译器未找到' };
    }
}

// Strip assembler directives, keep only labels + instructions + comments
function cleanAsmLine(line) {
    const t = line.trim();
    if (!t) return null;
    // Keep code labels (.L5, .LBB0_1:) but not DWARF/section markers
    if (/^\.L[a-zA-Z0-9_]*:/.test(t)) {
        // Skip DWARF markers, section markers, LSDA, exception labels
        if (/^\.L(FB|FE|C[0-9]|HOTB|COLDB|EH|LSDA|FSB|FS)/.test(t)) return null;
        return t;
    }
    // Keep comments
    if (t.startsWith('#') || t.startsWith(';')) return t;
    // Skip all other directives (lines starting with .)
    if (t.startsWith('.')) return null;
    return t;
}

// Detect function label in assembly (mangled name)
// Excludes local labels (.LBB, .LFB, .LFE, etc.)
function isFuncLabel(line) {
    const t = line.trim();
    return /^[a-zA-Z_$][a-zA-Z0-9_.$]*:/.test(t) && !t.startsWith('.L');
}

// Extract function bodies from cleaned assembly lines
function extractFunctions(asmLines) {
    const funcs = [];
    let current = null;

    for (const line of asmLines) {
        if (isFuncLabel(line)) {
            if (current) funcs.push(current);
            const name = line.trim().replace(/:$/, '');
            current = { name, lines: [] };
        } else if (current) {
            current.lines.push(line);
        }
    }
    if (current) funcs.push(current);
    return funcs;
}

// Extract user-defined function names from C++ source
// Returns array of { name, returnType, line }
function extractUserFunctions(sourcePath) {
    const src = readFileSync(sourcePath, 'utf-8').split('\n');
    const funcs = [];
    let braceDepth = 0;
    let inTest = false;

    for (let i = 0; i < src.length; i++) {
        const line = src[i];
        // Skip preprocessor, comments, includes
        if (/^\s*(#|\/\/)/.test(line)) continue;

        // Track TEST blocks to skip
        if (/^\s*TEST\s*\(/.test(line)) { inTest = true; }

        // Track brace depth
        for (const ch of line) {
            if (ch === '{') braceDepth++;
            if (ch === '}') {
                braceDepth--;
                if (braceDepth <= 0 && inTest) { inTest = false; braceDepth = 0; }
            }
        }

        if (inTest) continue;

        // Match function definitions: return_type function_name(params) {
        // Also handle: inline, static, template, constexpr, etc.
        const fnMatch = line.match(
            /^(?:\s*(?:inline|static|constexpr|virtual|explicit|friend|template\s*<[^>]*>|__attribute__\s*\(\([^)]*\)\)|\[\[gnu::[^\]]*\]\])\s+)*[\w:~*&<>, ]+?\s+(\*?[\w~]+)\s*\([^;]*$/);
        if (fnMatch) {
            const name = fnMatch[1].replace(/^[*&]+/, '');
            // Skip main, CPPLINGS_MAIN, namespace, class, struct
            if (['main', 'CPPLINGS_MAIN', 'namespace', 'class', 'struct'].some(
                k => line.includes(k + ' ') || line.includes(k + '{'))) continue;
            funcs.push({ name, line: i + 1 });
        }
    }
    return funcs;
}

// Batch demangle C++ symbols using c++filt
function demangleAll(names) {
    if (names.length === 0) return {};
    try {
        const result = execFileSync('c++filt', names, {
            stdio: 'pipe', timeout: 5000,
        }).toString().trim().split('\n');
        const map = {};
        for (let i = 0; i < names.length; i++) {
            map[names[i]] = result[i] || names[i];
        }
        return map;
    } catch {
        // c++filt not available — return identity map
        const map = {};
        for (const n of names) map[n] = n;
        return map;
    }
}

function runExe(exePath) {
    try {
        const out = execFileSync(exePath, [], {
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
    console.log(clr('    verify --solutions  ', C.green) + '验证所有 solution 文件');
    console.log(clr('    verify --all        ', C.green) + '验证全部练习');
    console.log(clr('    verify --ci         ', C.green) + '机器可读输出');
    console.log(clr('    verify --profile <name> ', C.green) + '使用编译器 profile 验证');
    console.log(clr('    matrix --solutions     ', C.green) + '多 profile 矩阵验证');
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
    console.log(clr(`  文件: ${ex.file}`, C.dim));
    console.log();
}

function cmdAsm(id, flags = {}) {
    if (!id) {
        console.error(clr('\n  用法: cpplings asm <id> [--opt=LEVEL] [--all]', C.red));
        console.error(clr('  LEVEL: O0, O1, O2 (默认), O3, Os, Oz', C.dim));
        console.error(clr('  --all: 显示所有函数（默认只显示用户定义的函数）', C.dim));
        console.error(clr('\n  示例:', C.dim));
        console.error(clr('    cpplings asm format1', C.dim));
        console.error(clr('    cpplings asm branchless1 --opt=O3', C.dim));
        console.error(clr('    cpplings asm format1 --all', C.dim));
        console.error();
        process.exit(1);
    }

    const ex = findEx(id);
    if (!ex) {
        console.error(clr(`\n  ✗ 未找到练习: ${id}\n`, C.red));
        process.exit(1);
    }

    // Use solution file if available, otherwise exercise file
    const solPath = ex.solution ? join(EXERCISES_DIR, ex.solution) : null;
    const filePath = (solPath && existsSync(solPath))
        ? solPath
        : join(EXERCISES_DIR, ex.file);

    if (!existsSync(filePath)) {
        console.error(clr(`\n  ✗ 文件不存在: ${filePath}\n`, C.red));
        process.exit(1);
    }

    const optLevel = flags.opt ? `-${flags.opt.replace(/^-/, '')}` : '-O2';
    console.log();
    console.log(clr(`  ─── ${ex.id}: ${ex.title} ───`, C.bold));
    console.log(clr(`  优化级别: ${optLevel}  语法: Intel`, C.dim));
    console.log();

    // Compile to assembly
    const comp = compileAsm(filePath, ex, { optLevel });
    if (!comp.ok) {
        console.log(clr('  ✗ 编译失败', C.red, C.bold));
        console.log();
        console.log(fmtErrors(comp.errors));
        console.log();
        return;
    }

    // Read and parse assembly
    const asmText = readFileSync(comp.asmFile, 'utf-8');
    const rawLines = asmText.split('\n');

    // Clean lines and extract function blocks
    const cleanedLines = [];
    const globlMap = new Map(); // mangled name → true if .globl
    let pendingGlobl = null;

    for (const line of rawLines) {
        const t = line.trim();
        // Track .globl declarations
        if (t.startsWith('.globl ')) {
            pendingGlobl = t.replace('.globl ', '').trim();
            continue;
        }

        const cleaned = cleanAsmLine(line);
        if (cleaned !== null) {
            // Check if this is a function label that had a preceding .globl
            if (isFuncLabel(cleaned) && pendingGlobl) {
                const labelName = cleaned.replace(/:$/, '');
                if (labelName === pendingGlobl) {
                    globlMap.set(labelName, true);
                }
                pendingGlobl = null;
            } else {
                pendingGlobl = null;
            }
            cleanedLines.push(cleaned);
        }
    }

    const funcs = extractFunctions(cleanedLines);

    if (funcs.length === 0) {
        console.log(clr('  ✗ 未找到函数体', C.red));
        console.log(clr('  提示: 文件可能只包含内联/模板代码', C.dim));
        console.log();
        return;
    }

    // Demangle all function names
    const mangledNames = funcs.map(f => f.name);
    const demangledMap = demangleAll(mangledNames);

    // Extract user function names from source
    const userFuncs = extractUserFunctions(filePath);
    const userFuncNames = new Set(userFuncs.map(f => f.name));
    // Pre-filter: exclude static init, cold-path, RTTI, and empty functions
    const filteredFuncs = funcs.filter(f => {
        if (f.name.startsWith('_GLOBAL__') || f.name.startsWith('__static_initialization')) return false;
        if (f.name.endsWith('.cold')) return false;
        if (f.lines.length === 0) return false;
        // Skip RTTI symbols (typeinfo, vtable, guard variables)
        const d = demangledMap[f.name] || f.name;
        if (/^(typeinfo|vtable|guard variable|VTT for)/.test(d)) return false;
        return true;
    });

    let importantFuncs;

    if (flags.all) {
        // --all: show all functions
        importantFuncs = filteredFuncs;
    } else {
        // Filter: show only user-defined functions (matching by demangled name)
        importantFuncs = filteredFuncs.filter(f => {
            const demangled = demangledMap[f.name] || f.name;
            for (const ufn of userFuncNames) {
                if (demangled.includes(ufn)) return true;
            }
            return false;
        });

        // If no user functions matched, show all exported (globl) functions
        if (importantFuncs.length === 0 && userFuncNames.size > 0) {
            importantFuncs = filteredFuncs.filter(f => globlMap.has(f.name));
        }

        // Still nothing? Show all non-internal functions
        if (importantFuncs.length === 0) {
            importantFuncs = filteredFuncs.filter(f => !f.name.startsWith('.'));
        }
    }

    // Display
    if (importantFuncs.length === 0) {
        console.log(clr('  ✗ 优化后无可见函数（可能全部被内联消除）', C.yellow));
        console.log();
        return;
    }

    for (const func of importantFuncs) {
        const demangled = demangledMap[func.name] || func.name;
        const isExported = globlMap.has(func.name);

        // Function header
        console.log(clr(`  ┌─ ${demangled}`, C.bold, C.cyan)
            + (isExported ? '' : clr(' [local]', C.dim)));
        if (demangled !== func.name) {
            console.log(clr(`  │  ${func.name}`, C.dim));
        }
        console.log(clr('  │', C.dim));

        // Instructions
        for (const line of func.lines) {
            const stripped = line.trim();

            // Highlight: labels in yellow, comments in dim, instructions normal
            let colored;
            if (stripped.startsWith('.')) {
                // Local label (.LBB0_1:)
                colored = clr(`  │  ${line}`, C.yellow);
            } else if (stripped.startsWith('#') || stripped.startsWith(';')) {
                // Comment
                colored = clr(`  │  ${line}`, C.dim);
            } else if (/^\s*(ret|jmp|je|jne|jg|jl|jge|jle|ja|jb|jae|jbe|call|loop)/.test(stripped)) {
                // Branch/call/ret in magenta
                colored = clr(`  │  ${line}`, C.magenta);
            } else {
                colored = `  │  ${line}`;
            }
            console.log(colored);
        }

        console.log(clr('  └─', C.dim));
        console.log();
    }

    console.log(clr(`  ${importantFuncs.length} 个函数`, C.dim)
        + clr(`  (${funcs.length} 总计, ${optLevel})`, C.dim));
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

function cmdVerify(flags = {}) {
    const m = loadManifest();
    const exercises = getExercises(m);
    const state = loadState();
    const profile = resolveProfile(flags);
    const compileOpts = {
        compiler: flags.compiler || profile.compiler,
        std: flags.std || profile.std,
        flags: profile.flags || [],
    };

    let completed = exercises.filter(e => state.completed[e.id]);
    if (flags.topic) {
        completed = completed.filter(e => e.topic === flags.topic);
    }

    if (completed.length === 0) {
        console.log(clr('\n  还没有完成任何练习\n', C.dim));
        return;
    }

    console.log();
    console.log(clr(`  验证 ${completed.length} 个已完成的练习...`, C.bold));
    console.log();

    let pass = 0, fail = 0;
    const failures = [];
    for (const ex of completed) {
        const filePath = join(EXERCISES_DIR, ex.file);
        if (!existsSync(filePath)) {
            fail++;
            failures.push({ id: ex.id, reason: '文件不存在' });
            console.log(clr(`  ✗ ${ex.id} — 文件不存在`, C.red));
            continue;
        }
        const comp = compile(filePath, ex, compileOpts);
        if (!comp.ok) {
            fail++;
            failures.push({ id: ex.id, reason: '编译失败', errors: comp.errors });
            console.log(clr(`  ✗ ${ex.id} — 编译失败`, C.red));
            continue;
        }
        const result = runExe(comp.exe);
        if (result.code === 0 && !result.stderr.includes('FAIL')) {
            pass++;
            console.log(clr(`  ✓ ${ex.id}`, C.green));
        } else {
            fail++;
            failures.push({ id: ex.id, reason: '测试失败' });
            console.log(clr(`  ✗ ${ex.id} — 测试失败`, C.red));
        }
    }

    console.log();
    const color = fail > 0 ? C.red : C.green;
    console.log(clr(`  结果: ${pass} 通过, ${fail} 失败`, color));
    console.log();

    if (flags.ci) {
        console.log(JSON.stringify({ pass, fail, failures }, null, 2));
    }

    if (fail > 0 && flags.ci) {
        process.exit(1);
    }
}

function cmdVerifySolutions(flags = {}) {
    const m = loadManifest();
    const exercises = getExercises(m);
    const profile = resolveProfile(flags);
    const compileOpts = {
        compiler: flags.compiler || profile.compiler,
        std: flags.std || profile.std,
        flags: profile.flags || [],
    };

    let target = exercises;
    if (flags.topic) {
        target = exercises.filter(e => e.topic === flags.topic);
    }

    target = target.filter(e => (e.kind || 'run-pass') === 'run-pass');

    console.log();
    console.log(clr(`  验证 ${target.length} 个练习的 solution 文件...`, C.bold));
    console.log();

    let pass = 0, fail = 0, skip = 0;
    const failures = [];

    for (const ex of target) {
        if (!ex.solution) {
            skip++;
            if (flags.verbose) console.log(clr(`  ⏭ ${ex.id} — 无 solution 文件`, C.dim));
            continue;
        }
        const solPath = join(EXERCISES_DIR, ex.solution);
        if (!existsSync(solPath)) {
            skip++;
            if (flags.verbose) console.log(clr(`  ⏭ ${ex.id} — solution 文件不存在`, C.dim));
            continue;
        }

        const comp = compile(solPath, ex, compileOpts);
        if (!comp.ok) {
            fail++;
            failures.push({ id: ex.id, reason: '编译失败', errors: comp.errors });
            console.log(clr(`  ✗ ${ex.id} — 编译失败`, C.red));
            continue;
        }
        const result = runExe(comp.exe);
        if (result.code === 0 && !result.stderr.includes('FAIL')) {
            pass++;
            console.log(clr(`  ✓ ${ex.id}`, C.green));
        } else {
            fail++;
            failures.push({ id: ex.id, reason: '测试失败' });
            console.log(clr(`  ✗ ${ex.id} — 测试失败`, C.red));
        }
    }

    console.log();
    const color = fail > 0 ? C.red : C.green;
    console.log(clr(`  结果: ${pass} 通过, ${fail} 失败, ${skip} 跳过`, color));
    console.log();

    if (flags.ci) {
        console.log(JSON.stringify({ pass, fail, skip, failures }, null, 2));
    }

    if (fail > 0 && flags.ci) {
        process.exit(1);
    }
}

function cmdVerifyAll(flags = {}) {
    const m = loadManifest();
    const exercises = getExercises(m);
    const profile = resolveProfile(flags);
    const compileOpts = {
        compiler: flags.compiler || profile.compiler,
        std: flags.std || profile.std,
        flags: profile.flags || [],
    };

    let target = exercises;
    if (flags.topic) {
        target = exercises.filter(e => e.topic === flags.topic);
    }

    const runPass = target.filter(e => (e.kind || 'run-pass') === 'run-pass');
    const compileFail = target.filter(e => e.kind === 'compile-fail');

    console.log();
    console.log(clr(`  验证全部 ${target.length} 个练习...`, C.bold));
    console.log();

    let pass = 0, fail = 0;

    for (const ex of runPass) {
        const filePath = join(EXERCISES_DIR, ex.file);
        const solPath = ex.solution ? join(EXERCISES_DIR, ex.solution) : null;
        const testPath = (solPath && existsSync(solPath)) ? solPath : filePath;

        if (!existsSync(testPath)) {
            fail++;
            console.log(clr(`  ✗ ${ex.id} — 文件不存在: ${basename(testPath)}`, C.red));
            continue;
        }

        const comp = compile(testPath, ex, compileOpts);
        if (!comp.ok) {
            fail++;
            console.log(clr(`  ✗ ${ex.id} — 编译失败`, C.red));
            continue;
        }
        const result = runExe(comp.exe);
        if (result.code === 0 && !result.stderr.includes('FAIL')) {
            pass++;
            console.log(clr(`  ✓ ${ex.id}`, C.green));
        } else {
            fail++;
            console.log(clr(`  ✗ ${ex.id} — 测试失败`, C.red));
        }
    }

    for (const ex of compileFail) {
        const filePath = join(EXERCISES_DIR, ex.file);
        if (!existsSync(filePath)) {
            fail++;
            console.log(clr(`  ✗ ${ex.id} — 文件不存在`, C.red));
            continue;
        }

        const comp = compile(filePath, ex, compileOpts);
        if (comp.ok) {
            fail++;
            console.log(clr(`  ✗ ${ex.id} — compile-fail 但编译成功了`, C.red));
        } else if (ex.expected_error && !comp.errors.includes(ex.expected_error)) {
            fail++;
            console.log(clr(`  ✗ ${ex.id} — 编译失败但未包含预期错误: ${ex.expected_error}`, C.red));
        } else {
            pass++;
            console.log(clr(`  ✓ ${ex.id} (compile-fail 通过)`, C.green));
        }
    }

    console.log();
    const color = fail > 0 ? C.red : C.green;
    console.log(clr(`  结果: ${pass} 通过, ${fail} 失败`, color));
    console.log();

    if (fail > 0 && flags.ci) {
        if (flags.ci) console.log(JSON.stringify({ pass, fail }));
        process.exit(1);
    }
}

function cmdMatrix(flags = {}) {
    const m = loadManifest();
    const exercises = getExercises(m);
    const profiles = loadProfiles();
    const profileNames = flags.profiles
        ? flags.profiles.split(',').map(s => s.trim())
        : Object.keys(profiles);

    const target = flags.solutions
        ? exercises.filter(e => e.solution && (e.kind || 'run-pass') === 'run-pass')
        : exercises.filter(e => (e.kind || 'run-pass') === 'run-pass');

    console.log();
    console.log(clr(`  Matrix: ${target.length} exercises × ${profileNames.length} profiles`, C.bold));
    console.log();

    const results = {};
    for (const pname of profileNames) {
        const p = profiles[pname];
        if (!p) { console.log(clr(`  ⏭ ${pname} — 未知 profile`, C.dim)); continue; }
        console.log(clr(`  ▸ ${pname} (${p.compiler} ${p.std})`, C.cyan));
        let pass = 0, fail = 0;
        for (const ex of target) {
            const testPath = flags.solutions
                ? join(EXERCISES_DIR, ex.solution)
                : join(EXERCISES_DIR, ex.file);
            if (!existsSync(testPath)) { fail++; continue; }
            const comp = compile(testPath, ex, { compiler: p.compiler, std: p.std, flags: p.flags || [] });
            if (!comp.ok) { fail++; continue; }
            const result = runExe(comp.exe);
            if (result.code === 0 && !result.stderr.includes('FAIL')) pass++;
            else fail++;
        }
        results[pname] = { pass, fail };
        const color = fail > 0 ? C.red : C.green;
        console.log(clr(`    ${pass} ✓  ${fail} ✗`, color));
    }
    console.log();
    if (flags.ci) console.log(JSON.stringify(results, null, 2));
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
            const ex = exercises.find(e => normalized.endsWith(e.file));
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

function parseArgs(args) {
    const flags = {};
    const positional = [];
    for (let i = 0; i < args.length; i++) {
        const a = args[i];
        if (a === '--solutions') { flags.solutions = true; }
        else if (a === '--all') { flags.all = true; }
        else if (a === '--ci') { flags.ci = true; }
        else if (a === '--topic' && i + 1 < args.length) { flags.topic = args[++i]; }
        else if (a === '--profile' && i + 1 < args.length) { flags.profile = args[++i]; }
        else if (a === '--profiles' && i + 1 < args.length) { flags.profiles = args[++i]; }
        else if (a === '--verbose') { flags.verbose = true; }
        else if (a === '--compiler' && i + 1 < args.length) { flags.compiler = args[++i]; }
        else if (a === '--std' && i + 1 < args.length) { flags.std = args[++i]; }
        else if (a.startsWith('--opt=') && a.length > 6) { flags.opt = a.slice(6); }
        else if (a.startsWith('--opt') && i + 1 < args.length) { flags.opt = args[++i]; }
        else if (!a.startsWith('-')) { positional.push(a); }
    }
    return { flags, positional };
}

// ── Main ──────────────────────────────────────────────────────────────
const rawArgs = process.argv.slice(2);
const { flags, positional } = parseArgs(rawArgs);
const cmd = positional[0];
const arg = positional[1];

switch (cmd) {
    case 'list':   case 'ls': cmdList(); break;
    case 'run':    case 'r':  cmdRun(arg); break;
    case 'hint':   case 'h':  cmdHint(arg); break;
    case 'watch':  case 'w':  cmdWatch(); break;
    case 'progress': case 'p': cmdProgress(); break;
    case 'verify': case 'v':
        if (flags.solutions) {
            cmdVerifySolutions(flags);
        } else if (flags.all) {
            cmdVerifyAll(flags);
        } else {
            cmdVerify(flags);
        }
        break;
    case 'next':   case 'n':  cmdNext(); break;
    case 'reset':             cmdReset(arg); break;
    case 'matrix': case 'm': cmdMatrix(flags); break;
    case 'asm':    case 'a': cmdAsm(arg, flags); break;
    default:                  cmdWelcome(); break;
}
