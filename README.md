# Custom Shell Implementation with History and Internal Commands

This project implements a basic shell that can execute both external and internal commands. It includes functionality for handling command history, argument passing, and error handling.

## Features:
- **External Commands**: Supports execution of external commands using fork and exec.
- **Internal Commands**: Implements internal commands such as `exit`, `pwd`, `cd`, and `help`.
- **Command History**: Displays the last 10 commands, supports rerunning commands with `!!`, `!n`, and manages command numbering.
- **Signal Handling**: Responds to `Ctrl+C` to display help information and returns to the prompt.
- **Bonus Features**: Supports changing directories with `cd ~` and returning to the previous directory with `cd -`.

## Feedback:
- Full marks were earned for external commands, memory management, and code quality.
- Minor issues with the current path not being displayed after internal commands and handling `!!`, `!0`, and `Ctrl+C` were noted.

This project was implemented in C and demonstrates proper use of system calls for shell functionality.
