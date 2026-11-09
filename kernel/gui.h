#pragma once

/* VoidOS desktop: wallpaper, taskbar with Start menu, draggable windows,
   PS/2 mouse cursor, and a terminal window with a tiny shell.
   Enters the event loop and never returns. */
void gui_run(const char *cpu_vendor);
