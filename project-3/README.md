# CS350P3

nneven - nneven@usc.edu, smorgent - smorgent@usc.edu

Instructions:
- cd into xv6-public-master
- make clean
- make qemu-nox
- run test1/2/3

Important Notes:
- currently MAKEFILE has CPUS := 2 which gave us proper test output
- if test output does not work on your VM, try to change CPUS := 1
- test1/2/3output.JPG highlights the expected output (highly recommended before running tests!!!)
- graph1/2/3.png were produced via Python matplotlib using test1/2/3results.txt data sets
- it may be easier to see proper scheduler functionality via data sets rather than the graphs
- if a test does not show desired functionality at first, rerun once or twice
