CS 415 Fall 2026
Project 1: Pseudo Shell
Author: Stella Greenvoss
Duck ID: 952000794
Date: 10/26/26

-------------------------------------
1. DESCRIPTION:

This is a pseudoshell capable of executing basic UNIX commands. It reads input (either interactively from stdin or from a file) line-by-line, splits them up into tokens, and executes each command using system calls. Multiple commands can be entered on one line, separated by ";". 

-------------------------------------
2. SUPPORTED COMMANDS:

    `ls`                  – lists the contents of the current directory

    `pwd`                 – prints the current working directory

    `mkdir "name"`        – creates a directory with the name "name>

    `cd "directory>`      – changes directories into the directory "directory>

    `cp "src> "dst>`      – copies the contents of the file "src> into the file "dst"

    `mv "src" "dst"`      – changes the name and location of the file at "src" to the file at "dst"

    `rm "filename"`       – deletes the file "filename"

    `cat "filename"`      – displays the contents of the file "filename"
    
    `exit`               – exits the program

-------------------------------------
3. COMPILATION:

Just run "make" and the Makefile should create the executable pseudo-shell.

-------------------------------------
4. USAGE:

a. Interactive Mode

  ```
  ./pseudo-shell
  >>> pwd
  /home/user/test
  >>> mkdir test-2; cd test-2
  >>> pwd
  /home/user/test/test-2
  >>> exit
  ```
  To quit, type `quit` or hit Ctrl-C. 

b. File Mode

  To run in file mode, `./pseudo-shell -f input_file`

  The output will overwrite the file `output.txt`. If the input file does not exist, the program will exit. 
