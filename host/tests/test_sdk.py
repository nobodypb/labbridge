import io,json,unittest
from types import SimpleNamespace
from labbridge.sdk import Node,Cluster,OperationFailed
from labbridge.transport import SerialTransport
from labbridge_generated.rest import RESTResponse
class Rest:
 def __init__(self,fail=False):self.calls=[];self.fail=fail
 def request(self,method,url,headers=None,body=None,**kw):
  self.calls.append((method,url,body));path=url.split('/api/v1/')[-1]
  if method=='POST' and path=='leases':
   if self.fail:raise TimeoutError('offline')
   data={'id':'l1','device_ids':body['device_ids'],'expires_at_ms':10000,'state':'active'}
  elif path=='leases/l1/stop':data={'id':'l1','device_ids':['load'],'expires_at_ms':0,'state':'stopped'}
  elif path.startswith('operations/'):
   data={'id':'o1','device_id':'load','status':'failed','stage':'readback','read_attempts':3,'write_attempts':2,'reconnects':0,'duration_ms':300,'final_output':'off','restore_pending':False}
  else:data={'id':'node','boot_id':'b1','firmware_version':'test','board':'sim','api_version':'0.1.0'}
  return RESTResponse(SimpleNamespace(status=200,reason='OK',headers={'Content-Type':'application/json'},data=json.dumps(data).encode()))
class Port:
 def __init__(self):self.sent=[];self.lines=[]
 def write(self,b):
  request=json.loads(b);self.sent.append(request)
  self.lines=[b'boot log\n',json.dumps({'id':'unrelated','status':200,'body':{}}).encode()+b'\n',b'LABJSON '+json.dumps({'id':request['id'],'status':200,'headers':{},'body':{'ok':True}}).encode()+b'\n']
 def readline(self):return self.lines.pop(0) if self.lines else b''
 def flush(self):pass
 def close(self):pass
class SdkTests(unittest.TestCase):
 def test_generated_client_works_through_injected_transport(self):
  self.assertEqual(Node('http://test',rest=Rest()).core.get_node().id,'node')
 def test_partial_cluster_acquisition_unwinds(self):
  a=Rest();cluster=Cluster({'a':Node('http://a',rest=a),'b':Node('http://b',rest=Rest(True))})
  with self.assertRaises(TimeoutError):
   with cluster.lease({'a':['load'],'b':['source']}):pass
  self.assertEqual(a.calls[-1][1],'http://a/api/v1/leases/l1/stop')
 def test_failed_operation_propagates_evidence(self):
  with self.assertRaises(OperationFailed) as caught:Node('http://a',rest=Rest()).wait('o1')
  self.assertEqual(caught.exception.operation.write_attempts,2)
 def test_usb_correlates_ids_and_never_replays_mutations(self):
  port=Port();rest=SerialTransport(port=port);r=rest.request('PUT','http://usb/api/v1/x',body={'v':1})
  self.assertEqual(json.loads(r.read()),{'ok':True});self.assertEqual(len(port.sent),1)
