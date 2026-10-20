import {createServer} from 'node:http';import {readFile} from 'node:fs/promises';
createServer(async(req,res)=>{
 const path=new URL(req.url,'http://localhost').pathname;
 const assets={'/app.js':'app.js','/dl24':'dl24.html','/dl24/':'dl24.html','/dl24.js':'dl24.js','/dl24.css':'dl24.css','/dl24-license.txt':'dl24-license.txt','/dl24-source.tar.gz':'dl24-source.tar.gz'};
 const file=assets[path]??(['/','/wifi','/devices'].includes(path)?'index.html':null);
 if(!file){res.writeHead(404);res.end();return;}
 try{const content=await readFile(new URL('../dist/'+file,import.meta.url));res.writeHead(200,{'Content-Type':file.endsWith('.js')?'text/javascript':file.endsWith('.css')?'text/css':file.endsWith('.gz')?'application/gzip':file.endsWith('.txt')?'text/plain':'text/html','Content-Security-Policy':"default-src 'self'; style-src 'self' 'unsafe-inline'; script-src 'self'; frame-ancestors 'none'"});res.end(content);}catch{res.writeHead(500);res.end();}
}).listen(4173,'127.0.0.1');
