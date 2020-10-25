# nanoShell

In informatics, a shell is an application that receives and interprets commands, like the BASH, ZSH or CSH.

In this project, we created the **nanoShell** shell application that should be able to:

1) Execute an application with multiple input parameters
2) Capture signal sent for the nanoShell
3) Support standard output redirects (**>** and **>>**) and standard errors (**2>** e **2>>**)
4) Show a help summary

## Rules
* Comment every function.
* Meaningful variable/function names


### VS Code Extensions required:
* Todo Tree
* GDB Debugger - Beyond


## Authors:

- André Azevedo - 2182634
- Alexandre Santos - 2181593

*Advanced Programming - ESTG - IPL - 2020/2021*


## To start nanoShell

* run the following commands in the project directory

    <code>make</code>

* Then one of the following options:

    <code>./nanoShell

    ./nanoShell -f help.txt

    ./nanoShell -m 5
    
    ./nanoShell -h
    
    ./nanoShell -s signal.txt
    </code>


## Help for nanoShell

* Use simple commands without metachars and pipes and ' ". Ex:

    <code>ps aux -l

    ls

    gcc -v
    </code>

* Use <code>bye</code> command to exit nanoShell

