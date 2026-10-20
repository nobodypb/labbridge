import {createServer} from 'node:http';import {readFile} from 'node:fs/promises';
createServer(async(req,res)=>{
 const path=new URL(req.url,'http://localhost').pathname;
 const file=path==='/app.js'?'app.js':['/','/wifi','/devices'].includes(path)?'index.html':null;
 if(!file){res.writeHead(404);res.end();return;}
 try{const content=await readFile(new URL('../dist/'+file,import.meta.url));res.writeHead(200,{'Content-Type':file.endsWith('.js')?'text/javascript':'text/html'});res.end(content);}catch{res.writeHead(500);res.end();}
}).listen(4173,'127.0.0.1');
