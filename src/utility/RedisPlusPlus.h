#pragma once

// Prints the Redis++ startup banner to stdout.
// Colors are used only when stdout is a terminal (NO_COLOR disables them,
// CLICOLOR_FORCE=1 forces them); the layout adapts to the terminal width.
void printWelcomeScreen();
