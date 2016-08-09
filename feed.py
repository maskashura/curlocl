#!/usr/bin/python2

from __future__ import print_function
import subprocess
import httplib
import json
import sys

headers = {"Content-type": "application/json", "Accept": "text/plain"}
def t2n(data):
    try:
        conn = httplib.HTTPConnection('localhost:14265')
        conn.request("POST", "/", json.dumps(data), headers)
        bd = conn.getresponse().read()
        ld = json.loads(bd)
        if ld.get("error"):
            print("failed to talk to node: {}".format(bd), file=sys.stderr)
            quit()
        return ld
    except:
        print("failed")
        quit()


def getNodeInfo():
    data = {"command": "getNodeInfo"}
    return t2n(data)

def getMilestone(midx):
    data = {"command": "getMilestone", "index": midx}
    return t2n(data)

def getTransactionsToApprove(mhash):
    data = {"command": "getTransactionsToApprove", "milestone": mhash}
    return t2n(data)

def feed():
    return getMilestone(getNodeInfo().get("milestoneIndex")).get("milestone");
    
def getwork(mstone):
    ap = getTransactionsToApprove(mstone);
    return ap.get("trunkTransaction"), ap.get("branchTransaction");

def push(rawtx):
	req = {"command": "pushTransactions", "trytes": [rawtx]}
	return t2n(req);

if __name__ == '__main__':
	mstone = feed();
	TXsDone = 0;
	
	while True:
		trunk, branch = getwork(mstone);
		
		rawtx = subprocess.check_output(["./curlocl", trunk, branch]);
		resp = push(rawtx);
		TXsDone += 1;
		
		print("TX pushed! Response: {}".format(resp));
		print("TXs mined and broadcasted: {}".format(TXsDone));
		
