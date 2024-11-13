#!/bin/bash

# Globals
ret=0
server="server"
client="client"
serverOut="testServerOutput.txt"
clientOut="testClientOutput.txt"
successFile="successRuleAdded"
IPADDRESS="localhost"
PORT=2200

# --- Helper Functions ---
function run() {
    echo -e "$1:"
    $1
    tmp=$?
    if [ $tmp -ne 0 ]; then
        ret=$tmp
    fi
    echo ""
    return $tmp
}

function checkConnection() {
    sleep 0.1
    case `netstat -a -n -p 2>/dev/null | grep $PORT ` in
        *":2200"*"LISTEN"*"server"*) return 1;;
    esac
    return 0
}

function sendCommand() {
    command=$1
    expectedOutput=$2

    echo -en "Executing command '$command': \t"
    ./$client $IPADDRESS $PORT "$command" > $clientOut 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "Error: Could not execute client"
        killall $server > /dev/null 2>&1
        return -1
    fi

    if [ ! -r $clientOut ]; then
        echo "Error: Client produced no output"
        return -1
    fi

    # Compare output and show diff if there's an error
    if ! diff -q $clientOut $expectedOutput >/dev/null; then
        echo "Error: Unexpected output for '$command'"
        echo "Expected:"
        cat $expectedOutput
        echo "Got:"
        cat $clientOut
        return -1
    else
        echo "OK"
    fi
}

# --- Setup Expected Output Files ---
echo "Rule added" > successRuleAdded
echo "Connection accepted" > successAccepted
echo "Connection rejected" > successRejected
echo -e "Rule: 1.1.1.1 10\nQuery: 1.1.1.1 10\nRule: 1.1.1.1 10\nRule: 1.1.1.1-2.2.2.2 10-20" > successList1
echo -e "Rule: 1.1.1.1 10\nQuery: 1.1.1.1 10\nQuery: 1.1.1.1 10\nRule: 1.1.1.1 10\nRule: 1.1.1.1-2.2.2.2 10-20" > successList2
echo -e "Rule: 1.1.1.1 10\nQuery: 1.1.1.1 10\nQuery: 1.1.1.1 10\nRule: 1.1.1.1 10\nRule: 1.1.1.1-2.2.2.2 10-20\nQuery: 1.3.6.7 15" > successList3
echo -e "Rule: 1.1.1.1 10\nRule: 1.1.1.1-2.2.2.2 10-20\nQuery: 1.3.6.7 15" > successList4
echo -e "A 1.1.1.1 10\nA 1.1.1.1 10\nA 1.1.1.1-2.2.2.2 10-20\nC 1.1.1.1 10\nL\nC 1.1.1.1 10\nL\nC 1.3.6.7 15\nL\nC 100.100.100.100 2\nD 1.1.1.1 10\nL\nR" > successHistory

# --- Test Commands ---
function server_mode_testcase() {
    # Cleanup
    rm -f $serverOut $clientOut
    killall $server > /dev/null 2>&1

    # Start Server
    echo -en "Starting server: \t"
    ./$server $PORT > $serverOut 2>&1 &
    checkConnection
    if [ $? -ne 1 ]; then
        echo "ERROR: Could not start server"
        return -1
    else
        echo "OK"
    fi

    # Commands in Order
    sendCommand "A 1.1.1.1 10" successRuleAdded
    sendCommand "A 1.1.1.1 10" successRuleAdded
    sendCommand "A 1.1.1.1-2.2.2.2 10-20" successRuleAdded
    sendCommand "C 1.1.1.1 10" successAccepted
    sendCommand "L" successList1
    sendCommand "C 1.1.1.1 10" successAccepted
    sendCommand "L" successList2
    sendCommand "C 1.3.6.7 15" successAccepted
    sendCommand "L" successList3
    sendCommand "C 100.100.100.100 2" successRejected
    sendCommand "D 1.1.1.1 10" successDelete
    sendCommand "L" successList4
    sendCommand "R" successHistory

    # Cleanup after test
    killall $server > /dev/null 2>&1
    echo "Server mode test completed."
}

echo -e "Rule deleted" > successDelete

# --- Execution ---
run server_mode_testcase

# Cleanup
if [ $ret -ne 0 ]; then
    echo "Server mode test failed"
else
    echo "Server mode test succeeded"
fi
exit $ret
