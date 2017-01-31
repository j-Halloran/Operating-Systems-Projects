readme

Program by Jake Halloran (jph74)

prodocons.c compiled using gcc -m32 -o prodcons -I /u/OSLab/USERNAME/linux-2.6.23.1/include/ prodcons.c
All changed files included despite only 4 being required
prodocons.h included but not used as I found it simpler just to redefine the struct


Producer Consumer best demonstrated by excessive numbers of one compared to the other
for example
./prodcons 20 1 1000
as an example call to demonstrate the proper alternation required

bugs: printing sometimes is weird with very large buffer sizes, however actual printing
of just the value reveals the problem to be merely visual
(i.e. sometimes with a 10k buffer it will print all 0 for the pancake being
consumed but it actually varies as required just a visual bug)
