Jake Halloran (jph74@pitt.edu)
Peoplesoft: 3911497

1.) The select call can wait up to a second meaning that sometimes it seems like it is doing nothing but is actually running fine
this is most noticible when running square.c as the wait causes a decent delay before action when entering a long string before q.

2.) intended to be compiled by first running: gcc -c graphics_library.c -o graphics_library.o
followed by                                   gcc -o graphics_driver graphics_driver.c graphics_library.o