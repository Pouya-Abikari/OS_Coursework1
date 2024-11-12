#!/bin/bash

# Function to execute a command and log responses with command index and content
execute_test() {
    local index=$1
    local command=$2
    # Strip trailing commas from the command
    command="${command%,}"
    local output=$(echo "$command" | ./server -i 2>&1)
    echo "Command $index: $command" >> output.txt
    echo "Output: $output" >> output.txt
    echo "----------" >> output.txt
}

# Define the array of test commands for adding, checking, deleting, printing, and listing rules
declare -a commands=(
    "A 192.168.1.1 80",
    "A 192.168.1.1-192.168.1.10 80",
    "A 192.168.1.1 80-100",
    "A 192.168.1.1-192.168.1.10 80-100",
    "A 999.999.999.999 80",
    "A 192.168.1.1 70000",
    "A 192.168.1.1 -1",
    "A -1.168.1.1 80",
    "A abc.def.ghi.jkl 80",
    "A 192.168.1.1 abc",
    "A 80",
    "A 192.168.1.1",
    "A 192.168.1.1 80 extra",
    "A 0.0.0.0-0.0.0.1 80",
    "A 0.0.0.0-255.255.255.255 80",
    "A 192.168.1.1 0-1",
    "A 192.168.1.1 0-65535",
    "A 192.168.1.1 1000-100",
    "A 192.168.1.10-192.168.1.1 80",
    "A 192.168.1.1-192.168.1.1 80",
    "A 192.168.1 80",
    "A  80",
    "A 192.168.1.1 ",
    "A 192. 168.1.1 80",
    "A 192.168.1.1 8 0",
    "A 192.168.1.1–192.168.1.10 80",
    "A 192.168.1.1 80–100",
    "A 192.168.a.1 80",
    "A 192.168.1.1 80a",
    "A 192.168.1.254-192.168.2.1 80",
    "A 192.168.1.1 0-80",
    "A 192.168.1.1 0",
    "A 192.168.1.1 65536",
    "A 192.168.1.1 -80-100",
    "A 192.168.a.b-192.168.c.d 80",
    "A 192.168.1.1 80-abc",
    "A 255.255.255.255 65535",
    "A 0.0.0.0 0",
    "192.168.1.1 80",
    "B 192.168.1.1 80",
    "A 192.168.#.1 80",
    "A 192.168.1.1 80$",
    "A 192.168.\n1.1 80",
    "A 192.168.1.1 80\r",
    "A 192.168.1.1 - 192.168.1.10 80",
    "A 192.168.1.1 80 - 100",
    "A  192.168.1.1  80",
    "A\t192.168.1.1\t80",
    "A 10.0.0.0-10.0.0.255 80",
    "A 172.16.0.0-172.16.255.255 80",
    "A 10.0.0.0-10.255.255.255 80",
    "A 192.168.256.1 80",
    "A 192.168.1.1.1 80",
    "A 192.168..1.1 80",
    "A 192.168.1 80",
    "A",
    "A 192.168.1.1",
    "A 80",
    "A 192.168.001.001 80",
    "A 192.168.1.1 0080",
    "A 256.256.256.256 80",
    "A 255.255.255.255 80",
    "A 127.0.0.1 80",
    "A 127.0.0.0-127.0.0.255 80",
    "A 192.168.1.1 80-80",
    "A 192.168.1.1-192.168.1.1 80",
    "a 192.168.1.1 80",
    "A 192.168.1.1.1.1 80",
    "A 192.168.1.é 80",
    "A 192.168.1.1 80é",
    "A 192.16a.1.1 80",
    "A 192.168.1.1 @80",
    " A 192.168.1.1 80",
    "A 192.168.1.1 80 ",
    "A",
    "C 192.168.1.1 80",
    "C 10.0.0.1 80",
    "C 192.168.1.1 9999",
    "C 10.0.0.1 80",
    "C 999.999.999.999 80",
    "C 192.168.1.1 70000",
    "C 192.168.1.1 -1",
    "C 192.168.1.1 abc",
    "C abc.def.ghi.jkl 80",
    "C 80",
    "C 192.168.1.1",
    "C 192.168.1.1 80 extra",
    "C 192.168.100.1 80",
    "C 192.168.1.1 8080",
    "C 10.10.10.10 8080",
    "C 192.168.1.5 5000",
    "C 10.0.0.1 80",
    "C 192.168.1.1 80",
    "C 192.168.1.1 80",
    "C -1.168.1.1 80",
    "C 192.168.#.1 80",
    "C 192.168.1.1 80$",
    "C 192.168.a.1 80",
    "C 192.168.1.1 80a",
    "C 192. 168.1.1 80",
    "C 192.168.1.1 8 0",
    "C  192.168.1.1  80",
    "C\t192.168.1.1\t80",
    "C 192.168.001.001 80",
    "C 192.168.1.1 0080",
    "C 255.255.255.255 80",
    "C 127.0.0.1 80",
    "C 192.168.1.1 0",
    "C 192.168.1.1 65535",
    "C 192.168.1.1 80",
    "C 192.168.1.10 80",
    "C 192.168.1.1 80",
    "C 192.168.1.1 100",
    "C 192.168.1.1.1 80",
    "C 192.168.1 80",
    "192.168.1.1 80",
    "D 192.168.1.1 80",
    "D 192.168.1.1 80 multiple times",
    "D 192.168.1.1 80",
    "D 10.0.0.1 80",
    "D 999.999.999.999 80",
    "D 192.168.1.1 70000",
    "D 192.168.1.1 -1",
    "D abc.def.ghi.jkl 80",
    "D 192.168.1.1 abc",
    "D 80",
    "D 192.168.1.1",
    "D 192.168.1.1 80 extra",
    "D 192.168.1.1-192.168.1.10 80-100",
    "D 192.168.1.1 80-100",
    "D 192.168.1.1-192.168.1.10 80-100",
    "D 10.10.10.10 80",
    "D 192.168.1.1 8080",
    "D 10.10.10.10 8080",
    "D -1.168.1.1 80",
    "D 192.168.#.1 80",
    "D 192.168.1.1 80$",
    "D 192.168.a.1 80",
    "D 192.168.1.1 80a",
    "D 192. 168.1.1 80",
    "D 192.168.1.1 8 0",
    "D  192.168.1.1  80",
    "D\t192.168.1.1\t80",
    "D 192.168.001.001 80",
    "D 192.168.1.1 0080",
    "D 255.255.255.255 80",
    "D 127.0.0.1 80",
    "D 192.168.1.1 0",
    "D 192.168.1.1 65535",
    "D 192.168.1.1.1 80",
    "D 192.168.1 80",
    "192.168.1.1 80",
    "C 192.168.1.1 80",
    "L",
    "L",
    "L extra",
    "l",
    "L$",
    "L ",
    "L",
    "a 192.168.1.1 80",
    " A 192.168.1.1 80",
    "C 192.168.1.1 80 ",
    "X 192.168.1.1 80",
    "",
    "   ",
    "A 192.168.1.1 80\n",
    "$(printf 'A 192.168.1.1 80\0')",
    "$(perl -e 'print "A 192.168.1.1 80" . "A" x 1024')",
    "A 192.168.1.1 80\nC 192.168.1.1 80",
    "A",
    "C",
    "D",
    "P",
    "$(perl -e 'print "A " . "a" x 10000')",
    "$(printf 'A 192.168.1.1 80\x01\x02\x03')",
    "A 192.168.1.1 80; DROP TABLE users;",
    "A 192.168.1.1 80; echo $(uname -a)",
    "A 192.168.1.1 😀"
)

# Ensure the expected_results.txt file exists
touch expected_results.txt

# Clear the output file and comparison results at the start of the script
echo "" > output.txt
echo "" > comparison_result.txt

# Start the server in interactive mode in the background
./server -i &
SERVER_PID=$!

# Ensure the server is killed on script exit or interrupt
trap "kill $SERVER_PID 2>/dev/null" EXIT

# Allow the server some time to initialize fully
sleep 5

# Execute all test cases
for i in "${!commands[@]}"; do
    execute_test "$((i + 1))" "${commands[$i]}"
done

# Wait a bit before killing the server to process all commands
sleep 1

# Compare output with expected results
diff output.txt expected_results.txt > comparison_result.txt

# Check for differences and report
if [ $? -eq 0 ]; then
    echo "Test Results Match Expected Results."
else
    echo "Differences found. See comparison_result.txt for details."
fi