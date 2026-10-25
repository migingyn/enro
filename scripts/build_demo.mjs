import { cp, mkdir, readFile, rm, writeFile } from 'node:fs/promises';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const root = fileURLToPath(new URL('../', import.meta.url));
const source = path.join(root, 'prototype/public');
const output = path.join(root, 'site-dist');
execFileSync('npm', ['--prefix', 'frontend', 'run', 'build'], {
  cwd: root, stdio: 'inherit', env: { ...process.env, VITE_USE_MOCKS: 'true' },
});
await rm(output, { recursive: true, force: true });
await mkdir(output);
for (const name of ['demo', 'fonts', 'landing-app.css']) {
  await cp(path.join(source, name), path.join(output, name), {
    recursive: true,
    filter: file => !file.endsWith('.test.mjs'),
  });
}
await cp(path.join(root, 'frontend/dist/assets'), path.join(output, 'assets'), { recursive: true });
const favicon = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><rect width="100" height="100" rx="24" fill="#b9d8fb"/><path d="M50 50 L12 64 A40 40 0 1 1 22 79 Z" fill="#13213a"/></svg>';
await writeFile(path.join(output, 'favicon.svg'), favicon);
const metadata = '<meta property="og:title" content="Enro | En route. On time."><meta property="og:description" content="Watch a changed appointment become a new departure plan and a phone call."><meta property="og:type" content="website"><meta property="og:image" content="https://enro-sic.vercel.app/preview.png"><meta name="twitter:card" content="summary_large_image"><link rel="icon" href="/favicon.svg">';
let landing = await readFile(path.join(source, 'index.html'), 'utf8');
landing = landing.replace('</head>', metadata + '</head>')
  .replaceAll('/approve.html#accounts', '/demo/day/')
  .replaceAll('/approve.html', '/demo/')
  .replaceAll('/console.html', '/dashboard')
  .replace('Plan my day', 'Watch the film')
  .replace('Try a demo', 'Try a sample day')
  .replace('class="join hero-action" href="/demo/"', 'class="demo-action" href="/demo/"')
  .replace('class="demo-action" href="/demo/day/"', 'class="join hero-action" href="/demo/day/"')
  .replace('A sample morning. No signup needed.', '56 seconds. A call at the right time.')
  .replace('Connect Gmail', 'Sample inbox')
  .replace('Connect calendar', 'Calendar preview')
  .replace('Open console ↗', 'Team dashboard ↗');
await writeFile(path.join(output, 'index.html'), landing);
await writeFile(path.join(output, 'landing.js'), '// This public site uses sample data. Account connections run locally.\n');
for (const page of ['index.html', 'lab.html']) {
  let room = await readFile(path.join(output, 'demo', page), 'utf8');
  room = room.replace('<html lang="en">', '<html lang="en" data-public-demo="true">').replace('</head>', metadata + '</head>');
  await writeFile(path.join(output, 'demo', page), room);
}
const team = await readFile(path.join(root, 'frontend/dist/index.html'), 'utf8');
await writeFile(path.join(output, 'team.html'), team
  .replace('</head>', '<link rel="icon" href="/favicon.svg"><style>body{padding-top:36px}#demo-banner{position:fixed;top:0;left:0;right:0;z-index:9999;display:flex;justify-content:space-between;gap:12px;padding:8px 16px;background:#13213a;color:#e9f3ff;font:12px system-ui}#demo-banner a{color:inherit;text-decoration:underline}</style></head>')
  .replace(/<body([^>]*)>/, '<body$1><aside id="demo-banner"><span>Team dashboard preview. Sample data and simulated calls.</span><a href="/demo/">Watch the film</a></aside>'));
try { await cp(path.join(root, 'docs/assets/demo-preview.png'), path.join(output, 'preview.png')); } catch { /* First local build can precede the screenshot. */ }
await writeFile(path.join(output, 'robots.txt'), 'User-agent: *\nAllow: /\n');
console.log('Public demo built. No credentials, private state, or local operator pages were copied.');
