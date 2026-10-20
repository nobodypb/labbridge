// SPDX-License-Identifier: GPL-3.0-or-later
import {build} from 'esbuild';
import {copyFile,mkdir,writeFile} from 'node:fs/promises';
import {gzipSync} from 'node:zlib';
import {execFileSync} from 'node:child_process';
await mkdir('dist',{recursive:true});
await build({entryPoints:['src/bench/main.ts'],bundle:true,minify:true,outfile:'dist/dl24.js',target:'es2020',legalComments:'inline'});
await copyFile('bench/index.html','dist/dl24.html');
await copyFile('bench/bench.css','dist/dl24.css');
await copyFile('vendor/dl24-bench/COPYING','dist/dl24-license.txt');
// Offline corresponding-source delivery, including the generated MIT client,
// original snapshot, tests and exact build inputs. Exclude build/dependency trees.
const source=execFileSync('tar',['--sort=name','--mtime=2026-10-20','--owner=0','--group=0','--numeric-owner','--exclude=web/node_modules','--exclude=web/dist','-cf','-','web','LICENSE'],{cwd:'..',maxBuffer:16*1024*1024});
await writeFile('dist/dl24-source.tar.gz',gzipSync(source));
