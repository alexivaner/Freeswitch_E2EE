#!/usr/bin/python
import os
import sys
import json
import pjsua as pj
from socket import *
import random
import time
import re
from select import *


# SIP request template
req_templ = \
"""$METHOD $TARGET_URI SIP/2.0\r
Via: SIP/2.0/UDP $LOCAL_IP:$LOCAL_PORT;rport;branch=z9hG4bK$BRANCH\r
Max-Forwards: 70\r
From: <sip:caller@pjsip.org>$FROM_TAG\r
To: <$TARGET_URI>$TO_TAG\r
Contact: <sip:$LOCAL_IP:$LOCAL_PORT;transport=udp>\r
Call-ID: $CALL_ID@pjsip.org\r
CSeq: $CSEQ $METHOD\r
Allow: PRACK, INVITE, ACK, BYE, CANCEL, UPDATE, REFER\r
Supported: replaces, 100rel, norefersub\r
User-Agent: pjsip.org Python tester\r
Content-Length: $CONTENT_LENGTH\r
$SIP_HEADERS"""
## iHMP IVR REQUEST TEMPLATE 
ivr_req_templ=\
"""{ "system":"true",
"request":{
    "InitialTimeOut_ReturnCode":"-1",
    "SessionID":"",
    "MinKey":1,
    "Domain":"system_default",
    "ClearBuffer":true,
    "Barge-in":false,
    "InterDigitTimeOut_ReturnCode":"-2",
    "TermKey":"#",
    "RepeatPlay":false,
    "Name":"IVR_PLAY_PROMPT_REQ",
    "InitialTimeOut":20000,
    "DigitMask_ReturnCode":"-3",
    "Using-Dtmf":false,
    "DigitMask":"1234567890*#",
    "InterDigitTimeOut":20000,
    "ReturnEvt":true,
    "FileList":[],
    "Language Type":"TW",
    "MaxKey":1
}
}"""
merge_templ="""{ "system":"true","request":{"Name":"MERGE_CALL_REQ","call_id1":"","call_id2":""}}"""
X_IHMP_REPLY_Codes=["Play End","Term Key","Max Key","Input Timeout","Input Digit Timeout","Key Miss Mask"]
X_IHMP_REPLY_list=["EVENT","CallID","Code","Error","PlayEOF","DTMF Length","DTMF","Last Digit"]

def is_request(msg):
	return msg.split(" ", 1)[0] != "SIP/2.0"
	
def is_response(msg):
	return msg.split(" ", 1)[0] == "SIP/2.0"

def get_code(msg):
	if msg=="":
		return 0
	return int(msg.split(" ", 2)[1])

def get_tag(msg, hdr="To"):
	pat = "^" + hdr + ":.*"
	result = re.search(pat, msg, re.M | re.I)
	if result==None:
		return ""
	line = result.group()
	#print "line=", line
	tags = line.split(";tag=")
	if len(tags)>1:
		return tags[1]
	return ""
	#return re.split("[;& ]", s)

def get_header(msg, hname):
	headers = msg.splitlines()
	for hdr in headers:
		hfields = hdr.split(": ", 2)
		if hfields[0]==hname:
			return hfields[1]
	return None

class iHMPSim:
	sock = None
	dst_addr = ""
	dst_port = 5060
	local_ip = ""
	local_port = 0
	tcp = False
	call_id = str(random.random())
	cseq = 0
	local_tag = ";tag=" + str(random.random())
	rem_tag = ""
	last_resp_code = 0
	last_resp = ""
	inv_branch = ""
	trace_enabled = True
	last_request = ""
	last_response=""
	def __init__(self, dst_addr, dst_port=5060, tcp=False, trace=True, local_port=0, local_ip="127.0.0.1"):
		self.dst_addr = dst_addr
		self.dst_port = dst_port
		self.tcp = tcp
		self.trace_enabled = trace
		self.local_ip = local_ip
		if tcp == True:
			self.sock = socket(AF_INET, SOCK_STREAM)
			self.sock.connect(dst_addr, dst_port)
		else:
			self.sock = socket(AF_INET, SOCK_DGRAM)
			self.sock.bind((self.local_ip, local_port))
		
		self.local_ip, self.local_port = self.sock.getsockname()
		self.trace("Dialog socket bound to " + self.local_ip + ":" + str(self.local_port))

	def trace(self, txt):
		if self.trace_enabled:
			print str(time.strftime("%H:%M:%S ")) + txt

	def update_fields(self, msg):
		if self.tcp:
			transport_param = ";transport=tcp"
		else:
			transport_param = ""
		msg = msg.replace("$TARGET_URI", "sip:"+self.dst_addr+":"+str(self.dst_port) + transport_param)
		msg = msg.replace("$LOCAL_IP", self.local_ip)
		msg = msg.replace("$LOCAL_PORT", str(self.local_port))
		msg = msg.replace("$FROM_TAG", self.local_tag)
		msg = msg.replace("$TO_TAG", self.rem_tag)
		msg = msg.replace("$CALL_ID", self.call_id)
		msg = msg.replace("$CSEQ", str(self.cseq))
		branch=str(random.random())
		msg = msg.replace("$BRANCH", branch)
		return msg

	def create_req(self, method, sdp, branch="", extra_headers="", body=""):
		if branch=="":
			self.cseq = self.cseq + 1
		msg = req_templ
		msg = msg.replace("$METHOD", method)
		msg = msg.replace("$SIP_HEADERS", extra_headers)
		if branch=="":
			branch=str(random.random())
		msg = msg.replace("$BRANCH", branch)
		if sdp!="":
			msg = msg.replace("$CONTENT_LENGTH", str(len(sdp)))
			msg = msg + "Content-Type: application/sdp\r\n"
			msg = msg + "\r\n"
			msg = msg + sdp
		elif body!="":
			msg = msg.replace("$CONTENT_LENGTH", str(len(body)))
			msg = msg + "\r\n"
			msg = msg + body
		else:
			msg = msg.replace("$CONTENT_LENGTH", "0")
		self.call_id = str(random.random())
		self.local_tag= ";tag=" + str(random.random())
		return self.update_fields(msg)

	def create_response(self, request, code, reason, to_tag=""):
		response = "SIP/2.0 " + str(code) + " " + reason + "\r\n"
		lines = request.splitlines()
		for line in lines:
			hdr = line.split(":", 1)[0]
			if hdr in ["Via", "From", "To", "CSeq", "Call-ID"]:
				if hdr=="To" and to_tag!="":
					line = line + ";tag=" + to_tag
				elif hdr=="Via":
					line = line + ";received=127.0.0.1"
				response = response + line + "\r\n"
		return response

	def create_invite(self, sdp, extra_headers="", body=""):
		self.inv_branch = str(random.random())
		return self.create_req("INVITE", sdp, branch=self.inv_branch, extra_headers=extra_headers, body=body)

	def create_notify(self,extra_headers="", body=""):
		self.inv_branch = str(random.random())
		return self.create_req("NOTIFY", sdp="", branch=self.inv_branch, extra_headers=extra_headers, body=body)

	def create_ack(self, sdp="", extra_headers=""):
		return self.create_req("ACK", sdp, extra_headers=extra_headers, branch=self.inv_branch)

	def create_bye(self, extra_headers=""):
		return self.create_req("BYE", "", extra_headers)

	def send_msg(self, msg, dst_addr=None):
		if (is_request(msg)):
			self.last_request = msg.split(" ", 1)[0]
		if not dst_addr:
			dst_addr = (self.dst_addr, self.dst_port)
		self.trace("============== TX MSG to " + str(dst_addr) + " ============= \n" + msg)
		self.sock.sendto(msg, 0, dst_addr)

	def wait_msg_from(self, timeout):
		endtime = time.time() + timeout
		msg = ""
		src_addr = None
		while time.time() < endtime:
			readset = select([self.sock], [], [], 1)
			if len(readset[0]) < 1 or not self.sock in readset[0]:
				if len(readset[0]) < 1:
					print "select() timeout (will wait for " + str(int(endtime - time.time())) + "more secs)"
				elif not self.sock in readset[0]:
					print "select() alien socket"
				else:
					print "select other error"
				continue
			try:
				msg, src_addr = self.sock.recvfrom(4096)
				break
			except:
				print "recv() exception: ", sys.exc_info()[0]
				continue

		if msg=="":
			return ("", None)
		if self.last_request=="INVITE" and self.rem_tag=="":
			self.rem_tag = get_tag(msg, "To")
			self.rem_tag = self.rem_tag.rstrip("\r\n;")
			if self.rem_tag != "":
				self.rem_tag = ";tag=" + self.rem_tag
			self.trace("=== rem_tag:" + self.rem_tag)
		self.trace("=========== RX MSG from " + str(src_addr) +  " ===========\n" + msg)
		return (msg, src_addr)
	
	def wait_msg(self, timeout):
		return self.wait_msg_from(timeout)[0]
		
	# Send request and wait for final response
	def send_request_wait(self, msg, timeout):
		t1 = 1.0
		endtime = time.time() + timeout
		resp = ""
		code = 0
		for i in range(0,5):
			self.send_msg(msg)
			resp = self.wait_msg(t1)
			if resp!="" and is_response(resp):
				self.last_resp_code = code = get_code(resp)
				break
		self.last_resp = resp
		while code < 200 and time.time() < endtime:
			resp = self.wait_msg(endtime - time.time())
			if resp != "" and is_response(resp):
				self.last_resp_code = code = get_code(resp)
				self.last_resp = resp
			elif resp=="":
				break
		return self.last_resp
	
	def hangup(self, last_code=0):
		self.trace("====== hangup =====")
		if last_code!=0:
			self.last_resp_code = last_code
		if self.last_resp_code>0 and self.last_resp_code<200:
			msg = self.create_req("CANCEL", "", branch=self.inv_branch, extra_headers="")
			self.send_request_wait(msg, 5)
			msg = self.create_ack()
			self.send_msg(msg)
		elif self.last_resp_code>=200 and self.last_resp_code<300:
			msg = self.create_ack()
			self.send_msg(msg)
			msg = self.create_bye()
			self.send_request_wait(msg, 5)
		else:
			msg = self.create_ack()
			self.send_msg(msg)
		
	def on_read(self):
		msg = ""
		src_addr = None
		try:
			msg, src_addr = self.sock.recvfrom(4096)
		except:
			print "recv() exception: ", sys.exc_info()[0]
		
		if msg=="":
			return 0
		if self.last_request=="INVITE" and self.rem_tag=="":
			self.rem_tag = get_tag(msg, "To")
			self.rem_tag = self.rem_tag.rstrip("\r\n;")
			if self.rem_tag != "":
				self.rem_tag = ";tag=" + self.rem_tag
			self.trace("=== rem_tag:" + self.rem_tag)
		self.trace("=========== RX MSG from " + str(src_addr) +  " ===========\n" + msg)
		#Auto response 200 OK 
		if is_request(msg):
			self.last_request=msg;
			self.send_msg(self.create_response(msg,200,"OK"))
			x_ihmp_replay=get_header(msg,"X-IHMP-REPLY")
			print "X-IHMP-REPLY->"+x_ihmp_replay
			if x_ihmp_replay !=None and len(x_ihmp_replay) >0:
				xlst=x_ihmp_replay.split(" ")
				i=0
				print "----Play Response(" + str(len(xlst)) +")----------"
				while i < len(xlst):
					if i == 2:
						print X_IHMP_REPLY_list[i] + " : "  + X_IHMP_REPLY_Codes[int(xlst[i])]
					else:
						print X_IHMP_REPLY_list[i] + " : "  + xlst[i]
					i=i+1
			return 1

		self.last_response=msg;
		return 2
	def ivr_play_req(self,callid,ReturnEvt=False,dtmf=False,BargeIn=False,ClearBuffer=True,MaxKey=1,TermKey="",RepeatPlay=False,DigitMask="1234567890*#",InitialTimeOut=10000,InterDigitTimeOut=5000):
		ivr_play_prompt_req=json.loads(ivr_req_templ)
		ivr_play_prompt_req['request']['SessionID']=callid
		ivr_play_prompt_req['request']['FileList'].append("voicemail/8000/vm-tutorial_change_pin.wav")
		if dtmf==True:
			ReturnEvt=True
		ivr_play_prompt_req['request']['ReturnEvt']=ReturnEvt
		ivr_play_prompt_req['request']['Using-Dtmf']=dtmf
		ivr_play_prompt_req['request']['Barge-in']=BargeIn
		ivr_play_prompt_req['request']['ClearBuffer']=ClearBuffer
		ivr_play_prompt_req['request']['MaxKey']=MaxKey
		ivr_play_prompt_req['request']['TermKey']=TermKey
		ivr_play_prompt_req['request']['RepeatPlay']=RepeatPlay
		ivr_play_prompt_req['request']['DigitMask']=DigitMask
		ivr_play_prompt_req['request']['InitialTimeOut']=InitialTimeOut
		ivr_play_prompt_req['request']['InterDigitTimeOut']=InterDigitTimeOut
		nmsg=self.create_notify(extra_headers="X-IHMP-REQ: IVR_PLAY_PROMPT_REQ\r\nContent-Type: application/json\r\n",body=json.dumps(ivr_play_prompt_req))
		resp=self.send_msg(nmsg)
	def merge_call(self,callid1,callid2):
		merge_req=json.loads(merge_templ)
		merge_req['request']['call_id1']=callid1
		merge_req['request']['call_id2']=callid2
		nmsg=self.create_notify(extra_headers="X-IHMP-REQ: MERGE_CALL_REQ\r\nContent-Type: application/json\r\n",body=json.dumps(merge_req))
		resp=self.send_msg(nmsg)
	def monitor_call(self,callid1,callid2):
		merge_req=json.loads(merge_templ)
		merge_req['request']['Name']="MONITOR_CALL_REQ"
		merge_req['request']['call_id1']=callid1
		merge_req['request']['call_id2']=callid2
		nmsg=self.create_notify(extra_headers="X-IHMP-REQ: MERGE_CALL_REQ\r\nContent-Type: application/json\r\n",body=json.dumps(merge_req))
		resp=self.send_msg(nmsg)
	def coach_call(self,callid1,callid2):
		merge_req=json.loads(merge_templ)
		merge_req['request']['Name']="COACH_CALL_REQ"
		merge_req['request']['call_id1']=callid1
		merge_req['request']['call_id2']=callid2
		nmsg=self.create_notify(extra_headers="X-IHMP-REQ: MERGE_CALL_REQ\r\nContent-Type: application/json\r\n",body=json.dumps(merge_req))
		resp=self.send_msg(nmsg)

# Logging callback
def log_cb(level, str, len):
	print str,

# Callback to receive events from Call
class MyCallCallback(pj.CallCallback):
	def __init__(self, call=None):
		pj.CallCallback.__init__(self, call)
	
def getpwd():
	pwd=sys.path[0]
	if os.path.isfile(pwd):
		pwd=os.path.dirname(pwd)
	return pwd

# Print application menu
#
def print_menu():
	print ""
	print ">>>"
	print """
+=========================================================+
|         Call Commands :      |  iHMP Call Commands:     |
|                              |                          |
|  c1  Make User  call         |  m1  Merge call  c1 c2   |
|  c2  Make Agent call         |  m2  Monitor call c3 c2  |
|  c3  Make Super call         |  m3  Coche call   c3 c2  |
|  r1  Release User call       |                          |
|  r2  Release Agent call      |                          |
|  r3  Release Super call      |                          |
|  d   Send DTMF rfc2833 by c1 |                          |
+------------------------------+--------------------------+
|  Local Media Commands        |  iHM Media Commands:     |
|                              |                          |
|  h1   hear call1             | p1  play file no event   |
|  h2   hear call2             | p2  play file have event |
|  h3   hear call3             | p3  play get 1 digit     |
|                              |                          |
|------------------------------+                          |
|  q  Quit application         |                          |
+=========================================================+"""
	print ">>>", 

# Menu
#

class MyTest:
	wav_dir=""
	lib=None
	tp_cfg=None
	acc=None
	dst=""
	call1=None
	call2=None
	call3=None
	play1=-1
	play2=-1
	play3=-1
	record1=-1
	record2=-1
	record3=-1
	f1=0
	f2=0
	f3=0
	
	trace_enabled=True
	def __init__(self,dst="sip:1000@192.168.56.101:5070",wav_dir="/home/wangkan/tmp/"):
		self.dst=dst
		self.wav_dir=wav_dir
	def trace(self, txt):
		if self.trace_enabled:
			print str(time.strftime("%H:%M:%S ")) + txt		
	def init(self):
		self.lib=pj.Lib()
		self.tp_cfg=pj.TransportConfig(5090,"192.168.56.1","192.168.56.1")
		self.lib.init(log_cfg = pj.LogConfig(level=3, callback=log_cb))
		transport = self.lib.create_transport(pj.TransportType.UDP,self.tp_cfg)
		self.acc = self.lib.create_account_for_transport(transport)
		self.play1=self.lib.create_player(self.wav_dir+"p1.wav",True)
		self.play2=self.lib.create_player(self.wav_dir+"p2.wav",True)
		self.play3=self.lib.create_player(self.wav_dir+"p3.wav",True)
		self.record1=self.lib.create_recorder(self.wav_dir+"r1.wav")
		self.record2=self.lib.create_recorder(self.wav_dir+"r2.wav")
		self.record3=self.lib.create_recorder(self.wav_dir+"r3.wav")
		self.lib.start()
	def destroy(self):
		self.h1()
		self.h2()
		self.h3()
		
		self.lib.player_destroy(self.play1)
		self.lib.player_destroy(self.play2)
		self.lib.player_destroy(self.play3)
		self.lib.recorder_destroy(self.record1)
		self.lib.recorder_destroy(self.record2)
		self.lib.recorder_destroy(self.record3)
		self.lib.destroy()
		self.lib=None
	def h1(self):
		if self.call1!=None:
			print "Release Call1"
			if self.f1==1:
				self.lib.conf_disconnect(self.lib.player_get_slot(self.play1),self.call1.info().conf_slot)
				self.lib.conf_disconnect(self.call1.info().conf_slot,self.lib.player_get_slot(self.record1))
				self.f1=0
			self.call1.hangup()
			del self.call1
		self.call1=None
	def ff1(self,sound=0):
		if sound==0 and self.call1!=None and self.f1==1:
			self.ff2(1)
			self.ff3(1)
			self.lib.conf_disconnect(self.lib.player_get_slot(self.play1),self.call1.info().conf_slot)
			self.lib.conf_disconnect(self.call1.info().conf_slot,self.lib.player_get_slot(self.record1))
			self.lib.conf_connect(0,self.call1.info().conf_slot)
			self.lib.conf_connect(self.call1.info().conf_slot,0)
			self.f1=0
			print "Call1 Switch to Speaker"
		if sound==1 and self.call1!=None and self.f1==0:
			self.lib.conf_disconnect(0,self.call1.info().conf_slot)
			self.lib.conf_disconnect(self.call1.info().conf_slot,0)
			self.lib.conf_connect(self.lib.player_get_slot(self.play1),self.call1.info().conf_slot)
			self.lib.conf_connect(self.call1.info().conf_slot,self.lib.player_get_slot(self.record1))
			self.f1=1
			print "Call1 Leave to Speaker"
			
	def c1(self):
		self.h1()
		self.call1 = self.acc.make_call(self.dst, MyCallCallback())

		while True:
			if self.call1.info().state == pj.CallState.CONFIRMED:
			 	break
			else:
				time.sleep(1)
		self.trace("Call1 Sip Is Connected\n")
		while True:
			if self.call1.info().media_state == pj.MediaState.ACTIVE:
				break
			else:
				time.sleep(1)
		self.trace("Call1 Media Is ACTIVE \n")
		self.f1=0
		self.lib.conf_connect(0,self.call1.info().conf_slot)
		self.lib.conf_connect(self.call1.info().conf_slot,0)
		
	def h2(self):
		if self.call2!=None:
			print "Release Call2"
			if self.f2==1:
				self.lib.conf_disconnect(self.lib.player_get_slot(self.play2),self.call2.info().conf_slot)
				self.lib.conf_disconnect(self.call2.info().conf_slot,self.lib.player_get_slot(self.record2))
				self.f2=0
			self.call2.hangup()
			del self.call2
		self.call2=None
	def ff2(self,sound=0):
		if sound==0 and self.call2!=None and self.f2==1:
			self.ff1(1)
			self.ff3(1)
			self.lib.conf_disconnect(self.lib.player_get_slot(self.play2),self.call2.info().conf_slot)
			self.lib.conf_disconnect(self.call2.info().conf_slot,self.lib.player_get_slot(self.record2))
			self.lib.conf_connect(0,self.call2.info().conf_slot)
			self.lib.conf_connect(self.call2.info().conf_slot,0)
			print "Call2 Switch to Speaker"
			self.f2=0
		if sound==1 and self.call2!=None and self.f2==0:
			self.lib.conf_disconnect(0,self.call2.info().conf_slot)
			self.lib.conf_disconnect(self.call2.info().conf_slot,0)
			self.lib.conf_connect(self.lib.player_get_slot(self.play2),self.call2.info().conf_slot)
			self.lib.conf_connect(self.call2.info().conf_slot,self.lib.player_get_slot(self.record2))
			print "Call2 Leave to Speaker"
			self.f2=1
			
	def c2(self):
		self.h2()
		self.call2 = self.acc.make_call(self.dst, MyCallCallback())

		while True:
			if self.call2.info().state == pj.CallState.CONFIRMED:
			 	break
			else:
				time.sleep(1)
		self.trace("Call2 Sip Is Connected\n")
		while True:
			if self.call2.info().media_state == pj.MediaState.ACTIVE:
				break
			else:
				time.sleep(1)
		self.trace("Call2 Media Is ACTIVE \n")
		self.f2=1
		self.lib.conf_connect(self.lib.player_get_slot(self.play2),self.call2.info().conf_slot)
		self.lib.conf_connect(self.call2.info().conf_slot,self.lib.player_get_slot(self.record2))

	def h3(self):
		if self.call3!=None:
			print "Release Call3"
			if self.f3==1:
				self.lib.conf_disconnect(self.lib.player_get_slot(self.play3),self.call3.info().conf_slot)
				self.lib.conf_disconnect(self.call3.info().conf_slot,self.lib.player_get_slot(self.record3))
				self.f3=0
			self.call3.hangup()
			del self.call3
		self.call3=None
	def ff3(self,sound=0):
		if sound==0 and self.call3!=None and self.f3==1:
			self.ff1(1)
			self.ff2(1)
			self.lib.conf_disconnect(self.lib.player_get_slot(self.play3),self.call3.info().conf_slot)
			self.lib.conf_disconnect(self.call3.info().conf_slot,self.lib.player_get_slot(self.record3))
			self.lib.conf_connect(0,self.call3.info().conf_slot)
			self.lib.conf_connect(self.call3.info().conf_slot,0)
			self.f3=0		
			print "Call3 Switch to Speaker"
		if sound==1 and self.call3!=None and self.f3==0:
			self.lib.conf_disconnect(0,self.call3.info().conf_slot)
			self.lib.conf_disconnect(self.call3.info().conf_slot,0)
			self.lib.conf_connect(self.lib.player_get_slot(self.play3),self.call3.info().conf_slot)
			self.lib.conf_connect(self.call3.info().conf_slot,self.lib.player_get_slot(self.record3))
			self.f3=1		
			print "Call3 Leave to Speaker"
	def c3(self):
		self.h3()
		self.call3 = self.acc.make_call(self.dst, MyCallCallback())

		while True:
			if self.call3.info().state == pj.CallState.CONFIRMED:
			 	break
			else:
				time.sleep(1)
		self.trace("Call3 Sip Is Connected\n")
		while True:
			if self.call3.info().media_state == pj.MediaState.ACTIVE:
				break
			else:
				time.sleep(1)
		self.trace("Call3 Media Is ACTIVE \n")
		self.f3=1
		self.lib.conf_connect(self.lib.player_get_slot(self.play3),self.call3.info().conf_slot)
		self.lib.conf_connect(self.call3.info().conf_slot,self.lib.player_get_slot(self.record3))

def input_process():
	global tester
	global ihmp
	input = sys.stdin.readline().rstrip("\r\n")
	print "INPUT: "+input
	if input=="q" or input =="Q":
		return 0
	if len(input) ==0:
		return 1
	if input[0]=="c" or input[0]=="C":
		if input[1]=="1":
			tester.c1()
		elif input[1]=="2":
			tester.c2()
		elif input[1]=="3":
			tester.c3()
		else:
			print "ERROR COMMAND " + input
	if input[0]=="p" or input[0] =="P":
		if input[1]=="1":
			if tester.call1 != None:
				ihmp.ivr_play_req(tester.call1.info().sip_call_id)
		if input[1]=="2":
			if tester.call1 != None:
				ihmp.ivr_play_req(tester.call1.info().sip_call_id,ReturnEvt=True)	
		if input[1]=="3":
			if tester.call1 != None:
				ihmp.ivr_play_req(tester.call1.info().sip_call_id,dtmf=True)	
	
	if input[0]=="r" or input[0] =="R":
		if input[1]=="1":
			tester.h1()
		if input[1]=="2":
			tester.h2()
		if input[1]=="3":
			tester.h3()
	if input[0]=="h" or input[0] =="H":
		if input[1]=="1":
			tester.ff1()
		if input[1]=="2":
			tester.ff2()
		if input[1]=="3":
			tester.ff3()
	if input[0]=="m" or input[0] =="M":
		if input[1]=="1":
			if tester.call1!=None and tester.call2!=None:
				ihmp.merge_call(tester.call1.info().sip_call_id,tester.call2.info().sip_call_id)
		if input[1]=="2":
			if tester.call2!=None and tester.call3!=None:
				ihmp.monitor_call(tester.call3.info().sip_call_id,tester.call2.info().sip_call_id)
		if input[1]=="3":
			if tester.call2!=None and tester.call3!=None:
				ihmp.coach_call(tester.call3.info().sip_call_id,tester.call2.info().sip_call_id)

	return 1

try:
	tester=MyTest()
	tester.init()
	ihmp=iHMPSim("192.168.56.101",5070,local_ip="192.168.56.1")
	while True:
		print_menu()
		readset = select([ihmp.sock,sys.stdin], [], [])
		if ihmp.sock in readset[0]:
			ihmp.on_read()
		elif sys.stdin in readset[0]:
			r=input_process()
			if r==0:
				break

	# We're done, shutdown the library
	tester.destroy()
	tester = None

except pj.Error, e:
	print "Exception: " + str(e)
	tester.destroy()
	tester = None
	sys.exit(1)
