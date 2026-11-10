#!/bin/bash
# sweep.sh: cycles every sky look through the command file, 3 s apiece (dev tool; needs the game running with ManhattanSkies loaded).
G="/c/Program Files (x86)/Steam/steamapps/common/Marvel's Spider-Man Remastered/scripts"
send() { printf "%s\n" "$1" > "$G/ManhattanSkies.cmd"; }
send "mark sweep"; sleep 1
send "skyreset"; sleep 3
# style:variant counts: 1=DAY 8, 2=SUNSET 9, 3=MORNING 3, 4=NIGHT 7, 5=OVERCAST 4, 6=STRANGE 6
for spec in "1 8" "2 9" "3 3" "4 7" "5 4" "6 6"; do
  set -- $spec
  for ((v = 0; v < $2; v++)); do
    send "look $1 $v"; sleep 3.2
  done
done
send "look 0 0"; sleep 3
send "mark sweep-end"; sleep 1
echo done
